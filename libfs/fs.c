//
// Created by Blake on 2/26/2020.
//
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
    uint8_t filename[16];
    uint32_t fileSize;
    uint16_t firstDataBlockIndex;
    uint8_t padding[10];
}rootDirEntry;

typedef struct __attribute__((__packed__)) {
    rootDirEntry rootDirEntries[128];
} rootDirectory;

typedef struct{
    superblock superblock;
    FAT FAT;
    rootDirectory rootDir;
} ECS150FS;


ECS150FS* FS;

int fs_mount(const char *diskname)
{
  block_disk_open(diskname);
  FS = malloc(sizeof(ECS150FS));
  block_read(0, &FS->superblock);
  if(FS->superblock.totalBlocks != block_disk_count()) {
    printf("No match\n");
  }
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
  return 0;
}

int fs_umount(void)
{
  if(FS == NULL){
    return -1;
  }
  free(FS->FAT);
  free(FS);
  return block_disk_close();

}

int fs_info(void)
{
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
  for(int i = 0; i<128; i++){
    if((char)FS->rootDir.rootDirEntries[i].filename[0] == '\0'){
      rdir_free++;
    }
  }
  printf("rdir_free_ratio=%d/%d\n", rdir_free, 128);
  return 0;
}

int fs_create(const char *filename)
{
  //Check error conditions
  size_t filename_len = strlen(filename);
  if(filename == NULL || filename[filename_len] != '\0' || filename_len > FS_FILENAME_LEN){
  	return -1;
  }
  for(int i = 0; i<128; i++){
  	if(memcmp(FS->rootDir.rootDirEntries[i].filename, filename, filename_len) == 0){
  		return -1;
  	}
  }
  //Find a place in the root directory to add the file
  int availableIndex = -1;
  for(int i = 0; i<128; i++){
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
  for(int i = 0; i<128; i++){
  	if(memcmp(FS->rootDir.rootDirEntries[i].filename, filename, filename_len) == 0){
  		fileIndex = i;
  	}
  }
  if (fileIndex == -1){
  	return -1;
  }

  //CHECK IF FILE IS OPEN

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
  return 0;
}

int fs_ls(void)
{
  //Check if underlying virtual disk open
  if(block_disk_count() == -1){
  	return -1;
  }  
  printf("FS Ls:\n");
  for(int i = 0; i<128; i++){
  	rootDirEntry r = FS->rootDir.rootDirEntries[i];
  	if(r.filename[0] != '\0'){
  		printf("file: %s, size: %d, data_blk: %d\n", r.filename, r.fileSize, r.firstDataBlockIndex);
  	}
  	
  }
  return 0;
}

int fs_open(const char *filename)
{
  /* TODO: Phase 3 */
  UNUSED(filename);
  return 0;
}

int fs_close(int fd)
{
  /* TODO: Phase 3 */
  UNUSED(fd);
  return 0;
}

int fs_stat(int fd)
{
  /* TODO: Phase 3 */
  UNUSED(fd);
  return 0;
}

int fs_lseek(int fd, size_t offset)
{
  /* TODO: Phase 3 */
  UNUSED(fd);
  UNUSED(offset);
  return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
  /* TODO: Phase 4 */
  UNUSED(fd);
  UNUSED(buf);
  UNUSED(count);
  return 0;
}

int fs_read(int fd, void *buf, size_t count)
{
  /* TODO: Phase 4 */
  UNUSED(fd);
  UNUSED(buf);
  UNUSED(count);
  return 0;
}
