#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>


#include <fs.c>


void test_small_rw_operation(){
  //Use lseek to move the offset. Write our test-fs-1 to disk. Then, read the file. We should only get the message in test-fs-1.
  struct stat st;

  /* Open file on host computer */
  int fd = open("test-fs-1", O_RDONLY);
  if (fd < 0)
    return;
  if (fstat(fd, &st))
    return;
  if (!S_ISREG(st.st_mode))
    return;

  /* Map file into buffer */
  char* buf = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (!buf)
    return;


  assert(fs_create("test-fs-1") == 0);
  assert(fs_create("test-fs-2") == 0);
  assert(fs_open("test-fs-1") == 0);
  assert(fs_open("test-fs-2") == 1);

  //Write max number of blocks, then write another one should fail
  char* maxBlock = malloc(FS->superblock.numDataBlocks*BLOCK_SIZE);
  char* maxBlock2 = malloc(FS->superblock.numDataBlocks*BLOCK_SIZE);
  for(int i = 0; i < FS->superblock.numDataBlocks * BLOCK_SIZE; i++) {
    maxBlock[i] = 3;
  }
  for(int i = 0; i < FS->superblock.numDataBlocks * BLOCK_SIZE; i++) {
    maxBlock2[i] = 4;
  }
  assert(fs_create("test-fs-3") == 0);
  assert(fs_open("test-fs-3") == 2);
  assert(fs_write(0, maxBlock, FS->superblock.numDataBlocks * BLOCK_SIZE) == 0);
  assert(fs_write(2, maxBlock2, FS->superblock.numDataBlocks * BLOCK_SIZE) == -1);
  close(2);


  //Test FAT first allocation
  char* appendBlock = malloc(BLOCK_SIZE*2);
  for(int i = 0; i < BLOCK_SIZE*2; i++) {
    appendBlock[i] = 3;
  }
  char* filledBlock = malloc(BLOCK_SIZE*2 + 2000);
  for(int i = 0; i < BLOCK_SIZE*2 + 2000; i++) {
    filledBlock[i] = 1;
  }
  //Write to file 1, then read it in and check its contents. Then close and delete file 1
  assert(fs_write(0, filledBlock, BLOCK_SIZE*2 + 2000));
  assert(fs_stat(0) == BLOCK_SIZE*2 + 2000);
  void* file1ReadBuffer = malloc(BLOCK_SIZE*2 + 2000);
  assert(fs_lseek(0,0) == 0);
  assert(fs_read(0, file1ReadBuffer, BLOCK_SIZE*2 + 2000) == BLOCK_SIZE*2 + 2000);
  assert(memcmp(file1ReadBuffer, filledBlock, BLOCK_SIZE*2 + 2000) == 0);
  char* filledBlock2 = malloc (3000);
  for(int i = 0; i < 3000; i++) {
    filledBlock2[i] = 2;
  }

  //Write to file 2, then delete file 1 and write to file 2 again
  assert(fs_write(1, filledBlock2, 3000));
  assert(fs_stat(1) == 3000);
  assert(fs_close(0) == 0);
  assert(fs_delete("test-fs-1") == 0);
  assert(fs_write(1, appendBlock, BLOCK_SIZE*2));
  printf("fs_stat 1 = %d\n", fs_stat(1));
  assert(fs_stat(1) == 3000 + BLOCK_SIZE*2);
  //Read to file 2 and check its contents
  void* file2ReadBuffer1 = malloc(3000);
  assert(fs_lseek(1, 0) == 0);
  assert(fs_read(1, file2ReadBuffer1, 3000) == 3000);
  assert(memcmp(file2ReadBuffer1, filledBlock2, 3000) == 0);
  void* file2ReadBuffer2 = malloc(BLOCK_SIZE*2);
  assert(fs_read(1, file2ReadBuffer2, BLOCK_SIZE*2) == BLOCK_SIZE*2);
  int numDiff = 0;
  int firstDiffPos = -1;
  for(int i = 0; i<BLOCK_SIZE*2; i++){
    if(((char*)file2ReadBuffer2)[i] != ((char*)appendBlock)[i]){
      if(firstDiffPos == -1){
        firstDiffPos = i;
      }
      numDiff++;
      printf("File2ReadBuffer: %d, File2ReadBuffer: %d\n", ((char*)file2ReadBuffer2)[i], ((char*)appendBlock)[i]);
    }
  }
  printf("Num different = %d, first different position = %d\n", numDiff, firstDiffPos);
  assert(memcmp(file2ReadBuffer2, appendBlock, BLOCK_SIZE*2) == 0);
  assert(FS->rootDir.rootDirEntries[1].firstDataBlockIndex == 4);
  assert(FS->FAT[4] == 1);
  assert(FS->FAT[1] == 2);
  assert(FS->FAT[2] == FAT_EOC);
  assert(fs_close(1) == 0);
  assert(fs_delete("test-fs-2") == 0);

  free(filledBlock2);
  free(filledBlock);
  free(appendBlock);
  free(file1ReadBuffer);
  free(file2ReadBuffer2);
  free(file2ReadBuffer1);

  printf("Finished FAT first test cases\n");


  //Test block write + read and test small read/write operation

  //Read entire empty block to disk
  assert(fs_create("test-fs-1") == 0);
  assert(fs_open("test-fs-1") == 0);
  char* emptyBlock = malloc(BLOCK_SIZE);
  emptyBlock[10] = 3;
  assert(fs_write(0, emptyBlock, BLOCK_SIZE) == BLOCK_SIZE);
  //Try reading the whole empty block in and seeing if it matches. This is a block write + block read test
  void* emptyBlockReadBuffer = malloc(BLOCK_SIZE);
  assert(fs_lseek(0,0) == 0);
  assert(fs_read(0, emptyBlockReadBuffer, BLOCK_SIZE) == BLOCK_SIZE);
  assert(memcmp(emptyBlockReadBuffer, emptyBlock, BLOCK_SIZE) == 0);

  //Write a small file in the middle of a block. Then, try reading it. This is a small read/write operations test
  assert(fs_lseek(0, 20) == 0);
  assert(fs_write(0, buf, st.st_size) == st.st_size);
  assert(fs_stat(0) == 4096);
  void* block = malloc(BLOCK_SIZE);
  block_read((size_t)(FS->rootDir.rootDirEntries[0].firstDataBlockIndex + FS->superblock.dataBlockStartIndex), block);
  assert(memcmp(block + 20, buf, st.st_size) == 0);
  printf("Got past read\n");
  assert(fs_lseek(0, 20) == 0);
  char* readBuffer = malloc(st.st_size);
  assert(fs_read(0, readBuffer, st.st_size) == st.st_size);
  assert(memcmp(readBuffer, buf, st.st_size) == 0);

  munmap(buf, st.st_size);
  close(fd);
  free(emptyBlockReadBuffer);
  free(emptyBlock);
  free(block);
  free(readBuffer);

}

void test_oob_read(){
  //testing writing half a block then reading moree than half. should return -1;
  struct stat st;

  /* Open file on host computer */
  int fd = open("test-fs-1", O_RDONLY);
  if (fd < 0)
    return;
  if (fstat(fd, &st))
    return;
  if (!S_ISREG(st.st_mode))
    return;

  /* Map file into buffer */
  char* buf = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (!buf)
    return;

  char* half_block = malloc(BLOCK_SIZE/2);
  for(int i = 0; i < BLOCK_SIZE/2; i++){
    half_block[i] = 1;
  }
  fs_create("test-oob-read");
  fs_open("test-oob-read");

  assert(fs_write(0, half_block, BLOCK_SIZE/2) == BLOCK_SIZE/2);
  assert(fs_read(0, half_block, BLOCK_SIZE) == -1);
  close(fd);
  close(0);

}

void test_open_and_create(){
  //test creating over max num of files
  char buffer[100];
  for(int i = 0; i < FS_FILE_MAX_COUNT + 3; i++) {
    sprintf(buffer, "test-max-create%d", i);
    assert(fs_create(buffer) == 0);
    if(i == FS_FILE_MAX_COUNT) {
      assert(fs_create("test-out-bounds") == -1);
      break;
    }
  }

  for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
    sprintf(buffer, "test-max-create%d", i);
    assert(fs_open(buffer) == 0);
    if(i == FS_OPEN_MAX_COUNT) {
      assert(fs_open(buffer) == 1);
      break;
    }
  }
  for(int i = 0; i < FS_FILE_MAX_COUNT ; i++) {
    close(i);
  }

}

int main(int argc, char **argv)
{
  if(argc < 2){
    printf("Please input the diskname as a command line arg\n");
    return -1;
  }
  char* diskname = argv[1];
  assert(fs_mount(diskname) == 0);

  test_small_rw_operation();
  test_oob_read();
  test_open_and_create();



  fs_umount();
  return 0;
}