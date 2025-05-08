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

#define PERROR(str) (fprintf(stderr,"[%d] %s in %s: %s\n",__LINE__,str,__FUNCTION__,strerror(errno)))

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
void sfs_PERROR(char *msg,int error){
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
		PERROR("write");
		return -1;
	}
	//8 bytes of next free page index
	result = write(filesystem->filesystem_fd,&next_free_page_index,sizeof(next_free_page_index));
	if (result < sizeof(next_free_page_index)){
		PERROR("write");
		return -1;
	}
	return 0;
}
uint64_t sfs_allocate_page(sfs_t *filesystem){
	if (filesystem->first_free_page_index == (uint64_t)-1){
		errno = ENOMEM;
		PERROR("allocating page");
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
		PERROR("lseek");
		return -1;
	}
	uint64_t next_free_page_index;
	result = read(filesystem->filesystem_fd,&next_free_page_index,sizeof(next_free_page_index));
	if (result < 0){
		PERROR("read");
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
		PERROR("write");
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
		PERROR("write");
		return -1;
	}
	return 0;
}
int sfs_read_inode_header(sfs_t *filesystem,uint64_t page,sfs_inode_t *inode){
	//====== go to the inode ======
	int result = sfs_seek_to_page(filesystem,page);
	if (result < 0){
		return -1;
	}
	int fd = filesystem->filesystem_fd;
	//====== read into the struct ======
	result = read(fd,inode,sizeof(sfs_inode_t));
	if (result < sizeof(sfs_inode_t)){
		PERROR("read");
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
	uint64_t continuation_page = sfs_allocate_page(filesystem);
	if (continuation_page == (uint64_t)-1){
		return (uint64_t)-1;
	}
	//====== read the inode headers we have been provided ======
	sfs_inode_t previous_inode_page;
	int result = sfs_read_inode_header(filesystem,page,&previous_inode_page);
	if (result < 0){
		return (uint64_t)-1;
	}
	//===== setup inode page =====
	sfs_inode_t new_inode_page;
	//copy the old page into the new one
	memcpy(&new_inode_page,&previous_inode_page,sizeof(sfs_inode_t));
	new_inode_page.next_page = (uint64_t)-1;
	//set previous pointer
	new_inode_page.previous_page = page;
	if (previous_inode_page.next_page != (uint64_t)-1){
		//====== if there is a page after ======
		//update the next pointer
		new_inode_page.next_page = previous_inode_page.next_page;
		//update the next page to point back here
		sfs_inode_t previous_next_inode_page;
		int result = sfs_read_inode_header(filesystem,previous_inode_page.next_page,&previous_next_inode_page);
		if (result < 0){
			return (uint64_t)-1;
		}
		//point it back here
		previous_next_inode_page.previous_page = continuation_page;
		result = sfs_write_inode_header(filesystem,previous_inode_page.next_page,&previous_next_inode_page);
		if (result < 0){
			return (uint64_t)-1;
		}
	}
	//====== update the previous inode page to point here ======
	previous_inode_page.next_page = continuation_page;
	result = sfs_write_inode_header(filesystem,page,&previous_inode_page);
	if (result < 0){
		return (uint64_t)-1;
	}
	//====== write the new inode page ======
	result = sfs_write_inode_header(filesystem,continuation_page,&new_inode_page);
	if (result < 0){
		return (uint64_t)-1;
	}

	return continuation_page;
}
int sfs_inode_remove_continuation_page(sfs_t *filesystem,uint64_t page){
	//====== read the given inode to remove ======
	sfs_inode_t inode_to_remove;
	int result = sfs_read_inode_header(filesystem,page,&inode_to_remove);
	if (result < 0){
		return -1;
	}
	//====== update the previous page's next pointer to skip this page ======
	//verify it has a previous page
	if (inode_to_remove.previous_page != (uint64_t)-1){
		//read the previous page
		sfs_inode_t previous_page;
		int result = sfs_read_inode_header(filesystem,inode_to_remove.previous_page,&previous_page);
		if (result < 0){
			return -1;
		}
		//even if the inode to remove has no next pointer (-1), setting the previous page to that value will just set it to -1 because yeah
		previous_page.next_page = inode_to_remove.next_page;
		//write the updated page
		result = sfs_write_inode_header(filesystem,inode_to_remove.previous_page,&previous_page);
		if (result < 0){
			return -1;
		}
	}
	//====== update the next page's previous pointer to skip this page  ======
	//verify it has a next page
	if (inode_to_remove.next_page != (uint64_t)-1){
		//(same old stuff as before)
		//read next page
		sfs_inode_t next_page;
		int result = sfs_read_inode_header(filesystem,inode_to_remove.next_page,&next_page);
		if (result < 0){
			return -1;
		}

		next_page.previous_page = inode_to_remove.previous_page;
		//write the updated page
		result = sfs_write_inode_header(filesystem,inode_to_remove.next_page,&next_page);
		if (result < 0){
			return -1;
		}
	}
	//====== register this page as free ======
	result = sfs_free_page(filesystem,page);
	if (result < 0){
		return -1;
	}
	return 0;
}
uint64_t sfs_inode_get_pointer(sfs_t *filesystem,uint64_t inode,uint64_t index){
	//====== read the given inode headers ======
	sfs_inode_t current_inode;
	int result = sfs_read_inode_header(filesystem,inode,&current_inode);
	if (result < 0){
		return (uint64_t)-1;
	}

	//check pointer is in the range of the max pointer count
	if (index >= current_inode.pointer_count){
		errno = EFAULT;
		PERROR("sfs_inode_get_pointer");
		return (uint64_t)-1;
	}

	//calculate which continuation page it is on
	int continuation_page_target_index = index/SFS_INODE_MAX_POINTERS;
	int current_page = inode;
	//====== travel to correct continuation page ======
	for (int continuation_page_index = 0; continuation_page_index < continuation_page_target_index;continuation_page_index++){
		//can we reach the next page?
		if (current_inode.next_page == (uint64_t)-1){
			errno = EFAULT;
			PERROR("sfs_inode_get_pointer");
			return (uint64_t)-1;
		}
		current_page = current_inode.next_page;
		int result = sfs_read_inode_header(filesystem,current_inode.next_page,&current_inode);
		if (result < 0){
			return (uint64_t)-1;
		}
	}
	//====== seek to page then to pointer ======
	result = sfs_seek_to_page(filesystem,current_page);
	if (result < 0){
		return (uint64_t)-1;
	}
	result = lseek(filesystem->filesystem_fd,SFS_INODE_ALIGNED_HEADER_SIZE,SEEK_CUR);
	if (result < 0){
		return (uint64_t)-1;
	}
	//====== read the pointer ======
	uint64_t pointer;
	result = read(filesystem->filesystem_fd,&pointer,sizeof(pointer));
	if (result < 0){
		return (uint64_t)-1;
	}
	return be64toh(pointer);
}
int sfs_inode_set_pointer(sfs_t *filesystem,uint64_t inode,uint64_t index,uint64_t pointer){
	//====== read the given inode headers ======
	sfs_inode_t current_inode;
	int result = sfs_read_inode_header(filesystem,inode,&current_inode);
	if (result < 0){
		return -1;
	}

	//check pointer is in the range of the max pointer count
	if (index >= current_inode.pointer_count){
		errno = EFAULT;
		PERROR("sfs_inode_set_pointer");
		return -1;
	}

	//calculate which continuation page it is on
	int continuation_page_target_index = index/SFS_INODE_MAX_POINTERS;
	int current_page = inode;
	//====== travel to correct continuation page ======
	for (int continuation_page_index = 0; continuation_page_index < continuation_page_target_index;continuation_page_index++){
		//can we reach the next page?
		if (current_inode.next_page == (uint64_t)-1){
			errno = EFAULT;
			PERROR("sfs_inode_set_pointer");
			return -1;
		}
		current_page = current_inode.next_page;
		int result = sfs_read_inode_header(filesystem,current_inode.next_page,&current_inode);
		if (result < 0){
			return -1;
		}
	}
	//====== seek to page then to pointer ======
	result = sfs_seek_to_page(filesystem,current_page);
	if (result < 0){
		return -1;
	}
	result = lseek(filesystem->filesystem_fd,SFS_INODE_ALIGNED_HEADER_SIZE,SEEK_CUR);
	if (result < 0){
		return -1;
	}
	//====== write the pointer ======
	//correct endianness
	uint64_t corrected_pointer = htobe64(pointer);
	result = write(filesystem->filesystem_fd,&corrected_pointer,sizeof(pointer));
	if (result < 0){
		return -1;
	}
	return 0;
}
int sfs_inode_realocate_pointers(sfs_t *filesystem,uint64_t inode,uint64_t count){
	//====== read the inode headers ======
	sfs_inode_t inode_header;
	int result = sfs_read_inode_header(filesystem,inode,&inode_header);
	if (result < 0){
		return -1;
	}
	uint64_t old_count = inode_header.pointer_count;
	//====== write the updated header ======
	inode_header.pointer_count = count;
	result = sfs_write_inode_header(filesystem,inode,&inode_header);
	if (result < 0){
		return -1;
	}
	//====== if the pointer count will not change do nothing ======
	if (old_count == count){
		return 0;
	}
	//====== add pages if required ======
	if (count > old_count){
		//traverse
		uint64_t page = inode;
		uint64_t pages_traversed = 0;
		uint64_t target_pages_to_traverse = count/SFS_INODE_MAX_POINTERS;
		for (;pages_traversed < target_pages_to_traverse;pages_traversed++){
			//if there is no next page allocate a new one
			if (inode_header.next_page == (uint64_t)-1){
				page = sfs_inode_insert_continuation_page(filesystem,page);
				if (page == (uint64_t)-1){
					return -1;
				}
			}else{
				page = inode_header.next_page;
			}
			//go to the next page
			int result = sfs_read_inode_header(filesystem,page,&inode_header);
			if (result < 0){
				return -1;
			}
		}
		return 0;
	}
	//====== remove pages ======
	if (count < old_count){
		//check if any pages have become redundant
		uint64_t old_page_count = (old_count+SFS_INODE_MAX_POINTERS-1)/SFS_INODE_MAX_POINTERS; //ceil division
		uint64_t new_page_count = (count+SFS_INODE_MAX_POINTERS-1)/SFS_INODE_MAX_POINTERS; //ceil division
		//we need at least one page so dont remove it
		if (new_page_count < 1) return 0;
		//only traverse if we need to
		if (new_page_count < old_page_count){
			sfs_inode_t current_inode_page;
			int result = sfs_read_inode_header(filesystem,inode,&current_inode_page);
			if (result < 0){
				return -1;
			}
			//count the current page
			new_page_count--;
			//traverse to the last needed page
			for (;new_page_count > 0;new_page_count--){
				uint64_t next_page = current_inode_page.next_page;
				if (next_page == (uint64_t)-1){
					//if there werent as many pages as expected, nothing more needs to be done
					return 0;
				}
				int result = sfs_read_inode_header(filesystem,next_page,&current_inode_page);
				if (result < 0){
					return -1;
				}
			}
			//for each unwanted page remove it
			for (;;){
				uint64_t next_page = current_inode_page.next_page;
				if (next_page == (uint64_t)-1){
					//all unwanted pages removed
					break;
				}
				int result = sfs_read_inode_header(filesystem,next_page,&current_inode_page);
				if (result < 0){
					return -1;
				}
				//the actual removal
				result = sfs_inode_remove_continuation_page(filesystem,next_page);
				if (result < 0){
					return -1;
				}
			}
		}
		return 0;
	}
	//====== it shouldnt be possible to get here ======
	return -1;
}
int sfs_inode_remove_pointer(sfs_t *filesystem,uint64_t inode,uint64_t index){
	//====== rearrange pointers ======
	/* example: removing pointer 56
	              p1          step 1: pointer count decreased (range now from p1 to p57)
	              ...
	              p56 <-+ step 2: swap the pointer to be removed and the last pointer
	 needs        p57   |
	 removing --> p58 <-+ this results in the swapped pointer being outside the range of
	                      accessible pointers (deleted)
		      ^^^
		      last pointer
	*/
	return 0;
}
