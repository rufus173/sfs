#define _LARGEFILE64_SOURCE

#include "../../include/sfs_functions.h"
#include "../../include/sfs_types.h"

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <endian.h>

#define PERROR(str) (fprintf(stderr,"%d %s: %s\n",__LINE__,__FUNCTION__,strerror(errno)))

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
	//read the magic number
	uint32_t magic_number;
	ssize_t bytes_read = read(filesystem_fd,&magic_number,sizeof(magic_number));
	if (bytes_read < sizeof(magic_number)){
		close(filesystem_fd);
		return -1;
	}
	//verify magic number
	if (be32toh(magic_number) != SFS_MAGIC_NO){
		close(filesystem_fd);
		return E_MALFORMED_SUPERBLOCK;
	}
	//read page count
	uint64_t page_count;
	bytes_read = read(filesystem_fd,&page_count,sizeof(page_count));
	if (bytes_read < sizeof(page_count)){
		close(filesystem_fd);
		return -1;
	}
	filesystem->page_count = be64toh(page_count);
	//read first free index  
	uint64_t first_free_page_index;
	bytes_read = read(filesystem_fd,&first_free_page_index,sizeof(first_free_page_index));
	if (bytes_read < sizeof(first_free_page_index)){
		close(filesystem_fd);
		return -1;
	}
	filesystem->first_free_page_index = be64toh(first_free_page_index);
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
int sfs_seek_to_page(sfs_t *filesystem,uint64_t page){
	off64_t offset = SFS_SUPERBLOCK_SIZE+(SFS_PAGE_SIZE*page);
	off64_t offset_result = lseek64(filesystem->filesystem_fd,offset,SEEK_SET);
	if (offset_result == (off64_t)-1){
		PERROR("lseek64");
		return -1;
	}
	return 0;
}
int sfs_free_page(sfs_t *filesystem,uint64_t page){
	int result = sfs_seek_to_page(filesystem,page);
	if (result < 0){
		return result;
	}
	//====== point the new free page to the previous first free page ======
	//if there are no previous free pages
	uint64_t next_free_page_index; //like NULL at the end of a linked list
	if (filesystem->first_free_page_index == (uint64_t)-1){
		filesystem->first_free_page_index = page; //update the head pointer
		next_free_page_index = htobe64((uint64_t)-1);
	}else{
	//if there is a previous free page, point to it and point the first free page pointer here
		next_free_page_index = htobe64(filesystem->first_free_page_index);
		filesystem->first_free_page_index = page;
	}
	//====== write the page header ======
	//1 byte of the page type
	uint8_t page_identifier = SFS_FREE_PAGE_IDENTIFIER;
	result = write(filesystem->filesystem_fd,&page_identifier,sizeof(uint8_t));
	if (result < sizeof(page_identifier)){
		return -1;
	}
	//8 bytes of next free page index
	result = write(filesystem->filesystem_fd,&next_free_page_index,sizeof(next_free_page_index));
	if (result < sizeof(next_free_page_index)){
		return -1;
	}
	return 0;
}
uint64_t sfs_allocate_page(sfs_t *filesystem){
	if (filesystem->first_free_page_index == (uint64_t)-1){
		return -1;
	}
	
	//====== go to the first free page to find the next free page ======
	int result = sfs_seek_to_page(filesystem,filesystem->first_free_page_index);
	if (result < 0){
		return -1;
	}
	//skip the page identifier bit
	off_t offset_result = lseek(filesystem->filesystem_fd,1,SEEK_CUR);
	if (offset_result == (off_t)-1){
		return -1;
	}
	uint64_t next_free_page_index;
	result = read(filesystem->filesystem_fd,&next_free_page_index,sizeof(next_free_page_index));
	if (result < 0){
		return -1;
	}
	//get the value to return
	uint64_t new_free_page = filesystem->first_free_page_index;
	//update the next free page
	filesystem->first_free_page_index = be64toh(next_free_page_index);

	return new_free_page;
}
int sfs_write_inode_header(sfs_t *filesystem,uint64_t page,sfs_inode_t *inode){
	//====== go to the inode ======
	int result = sfs_seek_to_page(filesystem,page);
	if (result < 0){
		return -1;
	}
	int fd = filesystem->filesystem_fd;
	//====== copy and correct endianness ======
	//we dont want to modify the users struct
	sfs_inode_t inode_cpy;
	memcpy(&inode_cpy,inode,sizeof(sfs_inode_t));
	inode_cpy.page = htobe64(inode_cpy.page);
	inode_cpy.parent_inode_pointer = htobe64(inode_cpy.parent_inode_pointer);
	inode_cpy.pointer_count = htobe64(inode_cpy.pointer_count);
	inode_cpy.next_page = htobe64(inode_cpy.next_page);
	inode_cpy.previous_page = htobe64(inode_cpy.previous_page);
	//====== write the struct ======
	result = write(fd,&inode_cpy,sizeof(sfs_inode_t));
	if (result < sizeof(sfs_inode_t)){
		return -1;
	}
	return 0;
}
int sfs_read_inode_headers(sfs_t *filesystem,uint64_t page,sfs_inode_t *inode){
	//====== go to the inode ======
	int result = sfs_seek_to_page(filesystem,page);
	if (result < 0){
		return -1;
	}
	int fd = filesystem->filesystem_fd;
	//====== read into the struct ======
	result = read(fd,inode,sizeof(sfs_inode_t));
	if (result < sizeof(sfs_inode_t)){
		return -1;
	}
	//====== correct endianness ======
	inode->page = be64toh(inode->page);
	inode->parent_inode_pointer = be64toh(inode->parent_inode_pointer);
	inode->pointer_count = be64toh(inode->pointer_count);
	inode->next_page = be64toh(inode->next_page);
	inode->previous_page = be64toh(inode->previous_page);
	return 0;
}
void sfs_print_info(){
	printf("page_size: %d\n",SFS_PAGE_SIZE);
	printf("inode_aligned_header_size: %lu\n",SFS_INODE_ALIGNED_HEADER_SIZE);
	printf("inode_header_size: %lu\n",sizeof(sfs_inode_t));
	printf("inode_max_pointers: %lu\n",SFS_INODE_MAX_POINTERS);
}
uint64_t sfs_inode_insert_continuation_page(sfs_t *filesystem,uint64_t page){
	//====== allocate a new page ======
	uint64_t continuation_page = sfs_allocate_page();
	if (inode_page == (uint64_t)-1){
		return (uint64_t)-1;
	}
	//====== read the inode headers we have been provided ======
	sfs_inode_t previous_inode_page;
	int result = sfs_read_inode_headers(filesystem,page,&previous_inode_page);
	if (result < 0){
		return (uint64_t)-1;
	}
	//===== setup inode page =====
	sfs_inode_t new_inode_page
	//copy the old page into the new one
	memcpy(new_inode_page,previous_inode_page,sizeof(sfs_inode_t));
	new_inode_page.next_page = (uint64_t)-1;
	//set previous pointer
	new_inode_page.previous_page = page
	if (previous_inode_page.next_page != (uint64_t)-1){
		//====== if there is a page after ======
		//update the next pointer
		new_inode_page.next_page = previous_inode_page.next_page;
		//update the next page to point back here
		sfs_inode_t previous_next_inode_page;
		int result = sfs_read_inode_headers(filesystem,previous_inode_page.next_page,previous_next_inode_page);
		if (result < 0){
			return (uint64_t)-1;
		}
		//point it back here
		previous_next_inode_page.previous_page = continuation_page;
		result = sfs_write_inode_headers(filesystem,previous_inode_page.next_page,previous_next_inode_page);
		if (result < 0){
			return (uint64_t)-1;
		}
	}
	//====== update the previous inode page to point here ======
	previous_inode_page.next_page = continuation_page;
	result = sfs_write_inode_headers(filesystem,page,&previous_inode_page);
	if (result < 0){
		return (uint64_t)-1;
	}
	//====== write the new inode page ======
	result = sfs_write_inode_headers(filesystem,continuation_page,&new_inode_page);
	if (result < 0){
		return (uint64_t)-1;
	}

	return continuation_page;
}
