#ifndef _SFS_FUNCTIONS_H
#define _SFS_FUNCTIONS_H

#include "sfs_types.h"

//====== open and close ======
int sfs_open_fs(sfs_t *filesystem,const char *path,int flags);
int sfs_close_fs(sfs_t *filesystem,int flags);

//====== page management ======
//works even if the page was never allocated
int sfs_free_page(sfs_t *filesystem,uint64_t page);
//returns allocated page index and removes it from the list of free pages
uint64_t sfs_allocate_page(sfs_t *filesystem); 
//places the file cursor at the begining of the given page
int sfs_seek_to_page(sfs_t *filesystem,uint64_t page);
int sfs_read_page(sfs_t *filesystem,uint64_t page);

//====== inodes ======
//write the sfs_inode_t to the given page (does not affect inode pointers)
int sfs_write_inode_header(sfs_t *filesystem,uint64_t page,sfs_inode_t *inode);
//same here
int sfs_read_inode_header(sfs_t *filesystem,uint64_t page,sfs_inode_t *inode);
//insert after the given inode page
uint64_t sfs_inode_insert_continuation_page(sfs_t *filesystem,uint64_t page);
//remove given continuation page and adjust the others to point to the correct places
int sfs_inode_remove_continuation_page(sfs_t *filesystem,uint64_t page);

//====== superblock ======
//closing the filesystem calls this, but it wont hurt to call this occasionaly
int sfs_update_superblock(sfs_t *filesystem);


//====== errors and debug ======
void sfs_perror(char *msg,int error);
void sfs_print_info();

const char *sfs_errno_to_str(int result);
#endif
