#ifndef _SFS_TYPES_H
#define _SFS_TYPES_H

#include <stddef.h>
#include <stdint.h>

#define SFS_PAGE_SIZE 1024
#define SFS_SUPERBLOCK_SIZE 256
//uint32_t
#define SFS_MAGIC_NO 0xC0FFEE

//====== type to represent  the filesystem as a whole ======
struct sfs_struct {
	uint64_t page_count;
	int filesystem_fd;
};
typedef struct sfs_struct sfs_t;

//====== all of the different pages ======
enum sfs_page_identifier {
	SFS_HEADER_PAGE,
	SFS_DATA_PAGE,
	SFS_INODE_PAGE,
	SFS_FREE_PAGE,
}:
struct sfs_inode_page {
	enum sfs_page_identifier page_type;
};
struct sfs_data_page {
	enum sfs_page_identifier page_type;
};
struct sfs_free_page {
	enum sfs_page_identifier page_type;
};


//====== one union to rule them all ======
union sfs_page {
	unsigned char empty_page[SFS_PAGE_SIZE];
	struct sfs_inode_page inode_page;
	struct sfs_data_page data_page;
	struct sfs_free_page free_page;
};

//====== typedefs to make things easier ======
typedef struct sfs_page sfs_page_t;
typedef struct sfs_data_page sfs_data_page_t;
typedef struct sfs_inode_page sfs_inode_page_t;
typedef struct sfs_free_page sfs_free_page_t;

#endif
