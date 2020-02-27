#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define UNUSED(x) (void)(x)

typedef struct __attribute__((__packed__)) {
	int8_t signature[8];
	int16_t totalBlocks;
	int16_t rootDirBlockIndex;
	int16_t dataBlockStartIndex;
	int16_t numDataBlocks;
	int8_t numFATBlocks;
	int8_t padding[4079];
} superblock;

typedef int16_t* FAT;

typedef struct __attribute__((__packed__)){
	int8_t filename[16];
	int32_t fileSize;
	int16_t firstDataBlockIndex;
	int8_t padding[10];
}rootDirEntry;

typedef struct __attribute__((__packed__)) {
	rootDirEntry rootDirEntries[128];
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
	char desiredSig[] = "ECS150FS";
	const void* sig = FS->superblock.signature;
	if(memcmp(desiredSig, sig, 2) != 0){
		return -1;
	}
	FS->FAT = malloc(FS->superblock.numFATBlocks*4096);
	for(int i = 1; i<=FS->superblock.numFATBlocks; i++){
		block_read(i, &FS->FAT[(i-1)*4096]);
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
	return block_disk_close();

}

int fs_info(void)
{
	printf("total_blk_count=%d\n", FS->superblock.totalBlocks);
	printf("fat_blk_count=%d\n", FS->superblock.numFATBlocks);
	printf("rdir_blk=%d\n", FS->superblock.rootDirBlockIndex);
	printf("data_blk=%d\n", FS->superblock.dataBlockStartIndex);
	printf("data_blk_count=%d\n", FS->superblock.numDataBlocks);
	int fat_free = 0;
	for(int i = 0; i<FS->superblock.numFATBlocks*(4096/2); i++){
		if(FS->FAT[i] == 0){
			fat_free++;
		}
	}
	printf("fat_free_ratio=%d/%d\n", fat_free, FS->superblock.numFATBlocks*(4096/2));
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
	/* TODO: Phase 2 */
	UNUSED(filename);
	return 0;
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
	UNUSED(filename);
	return 0;
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
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

