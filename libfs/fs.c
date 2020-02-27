#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

typedef struct __attribute__((__packed__)) {
	int8_t signature[2];
	int16_t totalBlocks;
	int16_t rootDirBlockIndex;
	int16_t dataBlockStartIndex;
	int16_t numDataBlocks;
	int8_t numFATBlocks;
	int8_t padding[4079];
} superblock;

typedef int16_t* FAT;

typedef struct __attribute__((__packed__)) {
	int8_t filename[16];
	int32_t fileSize;
	int16_t firstDataBlockIndex;
	int8_t padding[10];
} rootDirectory;

typedef struct{
	superblock superblock;
	FAT FAT;
	rootDirectory rootDir;
} ECS150FS;


ECS150FS* FS = NULL;

int fs_mount(const char *diskname)
{
	block_disk_open(diskname);
	FS = malloc(sizeof(ECS150FS));
	block_read(0, &FS->superblock);
	char* sig = (char*)FS->superblock.signature;
	if(sig != "ECS150FS"){
		return -1;
	}
	FS.FAT = (int16_t*)malloc(FS->superblock.numFATBlocks*4096);
	for(int i = 1; i<=FS->superblock.numFATBlocks; i++){
		block_read(i, &FS->FAT[(i-1)*4096]);
	}
	block_read(FS->superblock.rootDirBlockIndex, &FS->rootDir);
}

int fs_umount(void)
{
	if(FS == NULL){
		return -1;
	}
	free(FS->FAT);
	return block_disk_close();

}

int fs_info(void)
{
	/* TODO: Phase 1 */
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

