#ifndef _SFS_TYPES_H
#define _SFS_TYPES_H

#include <stddef.h>
#include <stdint.h>

#define SFS_PAGE_SIZE 1024
#define SFS_SUPERBLOCK_SIZE 256
//uint32_t
#define SFS_MAGIC_NO 0xC0FFEE

//====== type to represent the filesystem as a whole ======
struct sfs_struct {
	uint64_t page_count;
	int filesystem_fd;
	uint64_t first_free_page_index;
};
typedef struct sfs_struct sfs_t;

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
