#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define UNUSED(x) (void)(x)

#define FAT_EOC 0xFFFF

typedef struct __attribute__((__packed__)){
    uint8_t signature[8];
    uint16_t totalBlocks;
    uint16_t rootDirBlockIndex;
    uint16_t dataBlockStartIndex;
    uint16_t numDataBlocks;
    uint8_t numFATBlocks;
    uint8_t padding[4079];
}superblock;

typedef uint16_t* FAT;

typedef struct __attribute__((__packed__)){
    uint8_t filename[FS_FILENAME_LEN];
    uint32_t fileSize;
    uint16_t firstDataBlockIndex;
    uint8_t padding[10];
}rootDirEntry;

typedef struct __attribute__((__packed__)) {
    rootDirEntry rootDirEntries[FS_FILE_MAX_COUNT];
} rootDirectory;

typedef struct{
    superblock superblock;
    FAT FAT;
    rootDirectory rootDir;
} ECS150FS;

typedef struct{
    uint8_t filename[FS_FILENAME_LEN];
    size_t offset;
} fileDescriptor;

fileDescriptor fileDescriptorTable[FS_OPEN_MAX_COUNT];
int numFiles;
int numOpenFiles;


ECS150FS* FS;

int fs_mount(const char *diskname)
{
  if(diskname == NULL){
    return -1;
  }
  if(block_disk_open(diskname) == -1){
    return -1;
  }
  FS = malloc(sizeof(ECS150FS));
  block_read(0, &FS->superblock);
  char desiredSig[] = "ECS150FS";
  const void *sig = (char*)FS->superblock.signature;
  if(memcmp(desiredSig, sig, 8) != 0){
    return -1;
  }
  FS->FAT = (uint16_t*)malloc(FS->superblock.numFATBlocks*BLOCK_SIZE);
  for(int i = 0; i<FS->superblock.numFATBlocks; i++){
    block_read(i + 1, FS->FAT + (BLOCK_SIZE/2) * i);
  }
  block_read(FS->superblock.rootDirBlockIndex, &FS->rootDir);
  for(int i = 0; i<FS_OPEN_MAX_COUNT; i++){
    fileDescriptorTable[i].filename[0] = '\0';
  }
  numFiles = 0;
  numOpenFiles = 0;
  return 0;
}

int fs_umount(void)
{
  //Check if underlying virtual disk open
  if(block_disk_count() == -1){
    return -1;
  }
  if(FS == NULL || numOpenFiles != 0){
    return -1;
  }
  block_write(0, &(FS->superblock));
  for(int i = 0; i<FS->superblock.numFATBlocks; i++){
    block_write(i + 1, FS->FAT + (BLOCK_SIZE/2) * i);
  }
  block_write(FS->superblock.numFATBlocks+1, &(FS->rootDir));
  free(FS->FAT);
  free(FS);
  return block_disk_close();

}

int fs_info(void)
{
  //Check if underlying virtual disk open
  if(block_disk_count() == -1){
    return -1;
  }
  printf("FS Info:\n");
  printf("total_blk_count=%d\n", FS->superblock.totalBlocks);
  printf("fat_blk_count=%d\n", FS->superblock.numFATBlocks);
  printf("rdir_blk=%d\n", FS->superblock.rootDirBlockIndex);
  printf("data_blk=%d\n", FS->superblock.dataBlockStartIndex);
  printf("data_blk_count=%d\n", FS->superblock.numDataBlocks);
  int fat_free = 0;
  for(int i = 0; i<FS->superblock.numDataBlocks; i++){
    if(FS->FAT[i] == 0){
      fat_free++;
    }
  }
  printf("fat_free_ratio=%d/%d\n", fat_free, FS->superblock.numDataBlocks);
  int rdir_free = 0;
  for(int i = 0; i<FS_FILE_MAX_COUNT; i++){
    if((char)FS->rootDir.rootDirEntries[i].filename[0] == '\0'){
      rdir_free++;
    }
  }
  printf("rdir_free_ratio=%d/%d\n", rdir_free, FS_FILE_MAX_COUNT);
  return 0;
}

int fs_create(const char *filename)
{
  //Check error conditions
  size_t filename_len = strlen(filename);
  if(filename == NULL || filename[filename_len] != '\0' || filename_len > FS_FILENAME_LEN || numFiles > FS_FILE_MAX_COUNT){
    return -1;
  }
  //Check if root directory contains max number of files
  if(FS->rootDir.rootDirEntries[FS_FILE_MAX_COUNT-1].filename[0] != '\0'){
    return -1;
  }
  //Check if file already exists
  for(int i = 0; i<FS_FILE_MAX_COUNT; i++){
    if(memcmp(FS->rootDir.rootDirEntries[i].filename, filename, filename_len) == 0){
      return -1;
    }
  }
  //Find a place in the root directory to add the file
  int availableIndex = -1;
  for(int i = 0; i<FS_FILE_MAX_COUNT; i++){
    if(FS->rootDir.rootDirEntries[i].filename[0] == '\0'){
      availableIndex = i;
      break;
    }
  }
  //Create a new empty file
  rootDirEntry r = FS->rootDir.rootDirEntries[availableIndex];
  for(size_t i = 0; i<filename_len; i++){
    r.filename[i] = (uint8_t)filename[i];
  }
  r.fileSize = 0;
  r.firstDataBlockIndex = FAT_EOC;
  FS->rootDir.rootDirEntries[availableIndex] = r;
  numFiles++;
  return 0;
}

int fs_delete(const char *filename)
{
  //Removing a file is the opposite procedure: the file’s entry must be emptied and all the data blocks containing the file’s contents must be freed in the FAT.

  //Check error conditions
  size_t filename_len = strlen(filename);
  if(filename == NULL || filename[filename_len] != '\0' || filename_len > FS_FILENAME_LEN){
    return -1;
  }
  int fileIndex = -1;
  for(int i = 0; i<FS_FILE_MAX_COUNT; i++){
    if(memcmp(FS->rootDir.rootDirEntries[i].filename, filename, filename_len) == 0){
      fileIndex = i;
    }
  }
  if (fileIndex == -1){
    return -1;
  }

  //CHECK IF FILE IS OPEN
  for(int i = 0; i<FS_OPEN_MAX_COUNT; i++){
    if(memcmp(fileDescriptorTable[i].filename, filename, FS_FILENAME_LEN) == 0){
      return -1;
    }
  }

  //Free the file's entries in the FAT
  size_t FAT_index = FS->rootDir.rootDirEntries[fileIndex].firstDataBlockIndex;
  while(FAT_index != FAT_EOC){
    size_t new_FAT_index = FS->FAT[FAT_index];
    FS->FAT[FAT_index] = 0;
    FAT_index = new_FAT_index;
  }

  //Free the root directory entry
  FS->rootDir.rootDirEntries[fileIndex].filename[0] = '\0';
  FS->rootDir.rootDirEntries[fileIndex].fileSize = 0;
  FS->rootDir.rootDirEntries[fileIndex].firstDataBlockIndex = FAT_EOC;
  numFiles--;
  return 0;
}

int fs_ls(void)
{
  //Check if underlying virtual disk open
  if(block_disk_count() == -1){
    return -1;
  }
  printf("FS Ls:\n");
  for(int i = 0; i<FS_FILE_MAX_COUNT; i++){
    rootDirEntry r = FS->rootDir.rootDirEntries[i];
    if(r.filename[0] != '\0'){
      printf("file: %s, size: %d, data_blk: %d\n", r.filename, r.fileSize, r.firstDataBlockIndex);
    }
  }
  return 0;
}

int fs_open(const char *filename)
{
  //Check error conditions
  size_t filename_len = strlen(filename);
  if(filename == NULL || filename[filename_len] != '\0' || filename_len > FS_FILENAME_LEN){
    return -1;
  }
  int fileIndex = -1;
  for(int i = 0; i<FS_FILE_MAX_COUNT; i++){
    if(memcmp(FS->rootDir.rootDirEntries[i].filename, filename, filename_len) == 0){
      fileIndex = i;
      break;
    }
  }
  if (fileIndex == -1){
    return -1;
  }

  if(numOpenFiles >= FS_OPEN_MAX_COUNT){
    return -1;
  }

  int fd = -1;
  for(int i = 0; i<FS_OPEN_MAX_COUNT; i++){
    if(fileDescriptorTable[i].filename[0] == '\0'){
      for(size_t j = 0; j<filename_len; j++){
        fileDescriptorTable[i].filename[j] = (uint8_t)filename[j];
      }
      fileDescriptorTable[i].offset = 0;
      fd = i;
      break;
    }
  }
  numOpenFiles++;
  return fd;
}

int fs_close(int fd)
{
  if(fd >= FS_OPEN_MAX_COUNT || fd < 0){
    return -1;
  }
  if(fileDescriptorTable[fd].filename[0] == '\0'){
    return -1;
  }

  fileDescriptorTable[fd].filename[0] = '\0';
  fileDescriptorTable[fd].offset = 0;
  numOpenFiles--;
  return 0;
}

int fs_stat(int fd)
{
  if(fd >= FS_OPEN_MAX_COUNT || fd < 0){
    return -1;
  }
  if(fileDescriptorTable[fd].filename[0] == '\0'){
    return -1;
  }

  uint8_t filename[FS_FILENAME_LEN];
  memcpy(filename, fileDescriptorTable[fd].filename, sizeof(fileDescriptorTable[fd].filename));
  for(int i = 0; i<FS_FILE_MAX_COUNT; i++){
    if(memcmp(FS->rootDir.rootDirEntries[i].filename, filename, sizeof(filename)) == 0){
      return FS->rootDir.rootDirEntries[i].fileSize;
    }
  }
  return -1;
}

int fs_lseek(int fd, size_t offset)
{
  if(fd >= FS_OPEN_MAX_COUNT || fd < 0){
    return -1;
  }
  if(fileDescriptorTable[fd].filename[0] == '\0'){
    return -1;
  }
  if(offset > (size_t)fs_stat(fd)){
    return -1;
  }
  fileDescriptorTable[fd].offset = offset;
  return 0;
}

uint16_t allocateNewBlock(int rootDirEntryIndex){
  uint16_t availableIndex = 0;
  if(FS->rootDir.rootDirEntries[rootDirEntryIndex].firstDataBlockIndex == FAT_EOC){
    for(uint16_t i = 0; i< FS_FILE_MAX_COUNT; i++){
      if(FS->FAT[i] == 0){
        FS->FAT[i] = FAT_EOC;
        availableIndex = i;
        FS->rootDir.rootDirEntries[rootDirEntryIndex].firstDataBlockIndex = i;
        return availableIndex;
      }
    }
  }
  else{
    for(uint16_t i = 0; i< FS_FILE_MAX_COUNT; i++){
      if(FS->FAT[i] == 0){
        availableIndex = i;
        break;
      }
    }
    if(availableIndex == 0){
      return FAT_EOC;
    }
    uint16_t curBlockIndex = FS->rootDir.rootDirEntries[rootDirEntryIndex].firstDataBlockIndex;
    while(FS->FAT[curBlockIndex] != FAT_EOC){
      curBlockIndex = FS->FAT[curBlockIndex];
    }

    FS->FAT[curBlockIndex] = availableIndex;
    FS->FAT[availableIndex] = FAT_EOC;
    FS->superblock.numDataBlocks++;
    return availableIndex;
  }
  return FAT_EOC;

}

static uint16_t findCurrentBlockIndex(rootDirEntry r, size_t offset){
  uint16_t curBlockIndex = r.firstDataBlockIndex;
  while(offset > BLOCK_SIZE && curBlockIndex != FAT_EOC){
    curBlockIndex = FS->FAT[curBlockIndex];
    offset = offset - BLOCK_SIZE;
  }
  return curBlockIndex;
}

static int bounceBufferRead(void* buf, uint16_t curBlockIndex, size_t blockOffset, size_t numBytesToRead){
  void* bounceBuffer = malloc(BLOCK_SIZE);
  if(block_read((size_t)curBlockIndex, bounceBuffer) == 0){
    memcpy(buf, bounceBuffer + blockOffset, numBytesToRead);
    free(bounceBuffer);
    return 0;
  }
  free(bounceBuffer);
  return -1;
}

static int bounceBufferWrite(void* buf, uint16_t curBlockIndex, size_t blockOffset, size_t numBytesToWrite){
  void* bounceBuffer = malloc(BLOCK_SIZE);
  if(block_read((size_t)curBlockIndex, bounceBuffer) == 0){
    memcpy(bounceBuffer + blockOffset, buf, numBytesToWrite);
    if(block_write((size_t)curBlockIndex, bounceBuffer) == 0){
      free(bounceBuffer);
      return 0;
    }
  }
  free(bounceBuffer);
  return -1;
}



int fs_write(int fd, void *buf, size_t count)
{
  if(fd >= FS_OPEN_MAX_COUNT || fd < 0){
    return -1;
  }
  if(fileDescriptorTable[fd].filename[0] == '\0'){
    return -1;
  }
  int rootDirIndex = -1;
  for(int i = 0; i<FS_FILE_MAX_COUNT; i++){
    if(memcmp(fileDescriptorTable[fd].filename, FS->rootDir.rootDirEntries[i].filename, FS_FILENAME_LEN) == 0){
      rootDirIndex = i;
      break;
    }
  }

  size_t blockOffset = fileDescriptorTable[fd].offset%BLOCK_SIZE;
  size_t bytesLeftToWrite = count;
  uint32_t newBlocksAllocated = 0;
  uint16_t curBlockIndex = findCurrentBlockIndex(FS->rootDir.rootDirEntries[rootDirIndex], fileDescriptorTable[fd].offset);

  if(curBlockIndex == FAT_EOC){
    //Allocate space. If we were unable to allocate a new block, return.
    curBlockIndex = allocateNewBlock(rootDirIndex);
    if(curBlockIndex == FAT_EOC){
      fileDescriptorTable[fd].offset += count - bytesLeftToWrite;
      return count - bytesLeftToWrite;
    }
    //If allocated, add to new blocks allocated
    newBlocksAllocated++;
  }


  while(bytesLeftToWrite > BLOCK_SIZE){
    size_t distanceToBlockEnd = BLOCK_SIZE - blockOffset;
    if(distanceToBlockEnd < BLOCK_SIZE){
      void* fragmentToWrite = malloc(distanceToBlockEnd);
      memcpy(fragmentToWrite, buf, distanceToBlockEnd);
      if(bounceBufferWrite(fragmentToWrite, curBlockIndex, blockOffset, distanceToBlockEnd) != 0){
        return -1;
      }
      else{
        buf = buf + distanceToBlockEnd;
        bytesLeftToWrite = bytesLeftToWrite - distanceToBlockEnd;
      }
    }
    else{
      block_write(curBlockIndex, buf);
      buf = buf + BLOCK_SIZE;
      bytesLeftToWrite = bytesLeftToWrite - BLOCK_SIZE;
    }
    blockOffset = 0;
    curBlockIndex = FS->FAT[curBlockIndex];
    if(curBlockIndex == FAT_EOC){
        //Allocate space
        curBlockIndex = allocateNewBlock(rootDirIndex);
        //If allocation unsuccessful, return
        if(curBlockIndex == FAT_EOC){
          fileDescriptorTable[fd].offset += count - bytesLeftToWrite;
          return count - bytesLeftToWrite;
        }
        //increment new blocks allocated
        newBlocksAllocated++;
    }
  }


  if(bytesLeftToWrite == BLOCK_SIZE){
    block_write(curBlockIndex, buf);
    FS->rootDir.rootDirEntries[rootDirIndex].fileSize+=(newBlocksAllocated*BLOCK_SIZE);
    bytesLeftToWrite-=BLOCK_SIZE;
  }
  else if(bytesLeftToWrite != 0 && bytesLeftToWrite < BLOCK_SIZE){
    if(bounceBufferWrite(buf, curBlockIndex, blockOffset, bytesLeftToWrite) != 0){
      return -1;
    }
    if((FS->rootDir.rootDirEntries[rootDirIndex].fileSize%BLOCK_SIZE != 0 || FS->rootDir.rootDirEntries[rootDirIndex].fileSize == 0) && bytesLeftToWrite + blockOffset > FS->rootDir.rootDirEntries[rootDirIndex].fileSize%BLOCK_SIZE){
      FS->rootDir.rootDirEntries[rootDirIndex].fileSize += (bytesLeftToWrite + blockOffset - FS->rootDir.rootDirEntries[rootDirIndex].fileSize%BLOCK_SIZE);
      if(newBlocksAllocated != 0){
        FS->rootDir.rootDirEntries[rootDirIndex].fileSize += (newBlocksAllocated-1)*BLOCK_SIZE;
      }
    }
    bytesLeftToWrite=0;
  }

  fileDescriptorTable[fd].offset+=(count-bytesLeftToWrite);
  return count - bytesLeftToWrite;
}

int fs_read(int fd, void *buf, size_t count)
{
  if(fd >= FS_OPEN_MAX_COUNT || fd < 0){
    return -1;
  }
  if(fileDescriptorTable[fd].filename[0] == '\0'){
    return -1;
  }
  rootDirEntry r = FS->rootDir.rootDirEntries[0];
  for(int i = 0; i<FS_FILE_MAX_COUNT; i++){
    if(memcmp(fileDescriptorTable[fd].filename, FS->rootDir.rootDirEntries[i].filename, FS_FILENAME_LEN) == 0){
      r = FS->rootDir.rootDirEntries[i];
      break;
    }
  }

  uint16_t curBlockIndex = -1;
  curBlockIndex = findCurrentBlockIndex(r, fileDescriptorTable[fd].offset);
  size_t blockOffset = fileDescriptorTable[fd].offset%(size_t)BLOCK_SIZE;
  size_t bytesLeftToRead = count;

  while(bytesLeftToRead > BLOCK_SIZE){
    size_t distanceToBlockEnd = BLOCK_SIZE - blockOffset;
    if(distanceToBlockEnd < BLOCK_SIZE){
      void* fragmentToRead = malloc(distanceToBlockEnd);
      if(bounceBufferRead(fragmentToRead, curBlockIndex, blockOffset, distanceToBlockEnd) != 0){
        return -1;
      }
      else{
        memcpy(buf, fragmentToRead, distanceToBlockEnd);
        buf = buf + distanceToBlockEnd;
        bytesLeftToRead = bytesLeftToRead - distanceToBlockEnd;
      }
    }
    else{
      block_read(curBlockIndex, buf);
      buf = buf + BLOCK_SIZE;
      bytesLeftToRead = bytesLeftToRead - BLOCK_SIZE;
    }
    blockOffset = 0;
    curBlockIndex = FS->FAT[curBlockIndex];
    if(curBlockIndex == FAT_EOC){
      fileDescriptorTable[fd].offset += (count - bytesLeftToRead);
      return count - bytesLeftToRead;
    }
  }


  if(bytesLeftToRead == BLOCK_SIZE){
    block_read(curBlockIndex, buf);
  }
  else if(bytesLeftToRead < BLOCK_SIZE && bytesLeftToRead != 0){
    if(bounceBufferRead(buf, curBlockIndex, blockOffset, bytesLeftToRead) != 0){
      return -1;
    }
  }

  fileDescriptorTable[fd].offset+= (count - bytesLeftToRead);
  return count - bytesLeftToRead;
}