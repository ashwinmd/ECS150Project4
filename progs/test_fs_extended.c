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
	assert(fs_open("test-fs-1") == 0);
	void* emptyBlock = malloc(BLOCK_SIZE);
	assert(fs_write(0, emptyBlock, BLOCK_SIZE) == BLOCK_SIZE);
	assert(fs_lseek(0, 2000) == 0);
	assert(fs_write(0, buf, st.st_size) == st.st_size);
	void* block = malloc(BLOCK_SIZE);
	block_read((size_t)FS->rootDir.rootDirEntries[0].firstDataBlockIndex, block);
	assert(memcmp(block + 2000, buf, st.st_size) == 0);
	char* readBuffer = malloc(st.st_size);
	assert(fs_read(0, readBuffer, st.st_size) == 0);
	assert(memcmp(readBuffer, buf, st.st_size) == 0);

	munmap(buf, st.st_size);
	close(fd);
	free(emptyBlock);
	free(block);
	free(readBuffer);


}

int main(int argc, char **argv)
{
	if(argc < 2){
		printf("Please input the diskname as a command line arg\n");
		return -1;
	}
	char* diskname = argv[1];
	fs_mount(diskname);

	test_small_rw_operation();



	fs_umount();
	return 0;
}