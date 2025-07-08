#ifndef _SFS_TYPES_H
#define _SFS_TYPES_H

#include <stddef.h>
#include <stdint.h>

//====== helpfull macros ======
#define SFS_CALCULATE_ALIGNMENT_PADDING(structure,type) ((sizeof(type)-(sizeof(structure)%sizeof(type)))%sizeof(type))

#define SFS_PAGE_SIZE 1024
#define SFS_SUPERBLOCK_SIZE 256
//uint32_t
#define SFS_MAGIC_NO 0xC0FFEE

//====== type to represent the filesystem as a whole ======
struct sfs_struct {
	uint64_t page_count;
	int filesystem_fd;
	uint64_t first_free_page_index;
	uint64_t current_generation_number;
};
typedef struct sfs_struct sfs_t;

//====== struct to represent an inode ======
#define SFS_MAX_FILENAME_SIZE 256
struct __attribute__((__packed__)) sfs_inode {
	uint64_t page;
	uint64_t parent_inode_pointer;
	uint64_t pointer_count;
	uint64_t next_page;
	uint64_t previous_page;
	uint64_t generation_number;
	uint32_t mode;
	uint32_t uid;
	uint32_t gid;
	char name[SFS_MAX_FILENAME_SIZE];
};
typedef struct sfs_inode sfs_inode_t;
#define SFS_INODE_ALIGNED_HEADER_SIZE (SFS_CALCULATE_ALIGNMENT_PADDING(sfs_inode_t,uint64_t)+sizeof(sfs_inode_t))
#define SFS_INODE_MAX_POINTERS ((SFS_PAGE_SIZE-SFS_INODE_ALIGNED_HEADER_SIZE)/sizeof(uint64_t))

//====== all of the different pages ======
#define SFS_FREE_PAGE_IDENTIFIER 1
#define SFS_DATA_PAGE_IDENTIFIER 2
#define SFS_INODE_PAGE IDENTIFIER 3

//====== errors ======
#define E_MALFORMED_SUPERBLOCK -2

//====== function flags ======
enum sfs_function_flags {
	SFS_FUNC_FLAG_SKIP_SUPERBLOCK_CHECK  = (1<<0),
	SFS_FUNC_FLAG_O_CREATE = (1<<1),
};

#endif
