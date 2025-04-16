#include "../../include/sfs_functions.h"
#include "../../include/sfs_types.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <endian.h>

const char *sfs_errno_to_str(int result){
	switch(result){
	case 0:
		return "Success";
	case -1:
		return strerror(errno);
	case E_MALFORMED_SUPERBLOCK:
		return "Malformed superblock encountered";
	default:
		return "Unknown error";
	}
}
void sfs_perror(char *msg,int error){
	fprintf(stderr,"%s: %s\n",msg,sfs_errno_to_str(error));
}

int sfs_open_fs(sfs_t *filesystem,const char *path,int flags){
	//====== open the filesystem ======
	memset(filesystem,0,sizeof(sfs_t));
	int open_flags = O_RDWR;
	//allow it to be created if requested
	if ((flags & SFS_FUNC_FLAG_O_CREATE) != 0) open_flags |= O_CREAT;
	int filesystem_fd = open(path,open_flags,0666);
	if (filesystem_fd < 0){
		return -1;
	}
	filesystem->filesystem_fd = filesystem_fd;

	//if skip superblock check flag on
	if ((flags & SFS_FUNC_FLAG_SKIP_SUPERBLOCK_CHECK) != 0) return 0;

	printf("reading superblock\n");
	//====== attempt to read the superblock ======
	unsigned char superblock[SFS_SUPERBLOCK_SIZE];
	ssize_t bytes_read = read(filesystem_fd,superblock,SFS_SUPERBLOCK_SIZE);
	if (bytes_read < SFS_SUPERBLOCK_SIZE){
		close(filesystem_fd);
		return E_MALFORMED_SUPERBLOCK;
	}
	return 0;
}
int sfs_close_fs(sfs_t *filesystem,int flags){
	int return_val = 0;
	//update the superblock
	int result = sfs_update_superblock(filesystem);
	if (result < 0){
		return_val = result;
	}
	//close the filesystem fd
	result = close(filesystem->filesystem_fd);
	if (result < 0){
		return_val = result;
	}
	//return the status
	return return_val;
}

int sfs_update_superblock(sfs_t *filesystem){
	int filesystem_fd = filesystem->filesystem_fd;
	//====== go to begining ======
	off_t position = lseek(filesystem_fd,0,SEEK_SET);
	if (position == (off_t)-1){
		return -1;
	}
	//====== write the fields (with endianness corrected) ======
	//4 byte magic number
	uint32_t magic_number = htobe32(SFS_MAGIC_NO);
	int result = write(filesystem_fd,&magic_number,sizeof(magic_number));
	if (result < sizeof(magic_number)) return -1;
	//8 byte page count
	uint64_t page_count = htobe64(filesystem->page_count);
	result = write(filesystem_fd,&page_count,sizeof(page_count));
	if (result < sizeof(page_count)) return -1;
	//8 byte first free page
	uint64_t first_free_page_index = htobe64(filesystem->first_free_page_index);
	result = write(filesystem_fd,&first_free_page_index,sizeof(first_free_page_index));
	if (result < sizeof(first_free_page_index)) return -1;
	return 0;
}
