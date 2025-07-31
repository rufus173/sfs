#ifndef _SFS_FUNCTIONS_H
#define _SFS_FUNCTIONS_H

#include "sfs_types.h"
#include <sys/stat.h>

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

//--- offset finding ---
//successor to sfs_seek_to_page
uint64_t sfs_page_offset(sfs_t *filesystem,uint64_t page);
//successor to sfs_seek_to_inode
uint64_t sfs_inode_pointer_offset(sfs_t *filesystem,uint64_t inode,uint64_t index);

//====== inodes ======
//write the sfs_inode_t to the given page (does not affect inode pointers)
//both the read and write functions correct endianness from machine to be automaticaly
int sfs_write_inode_header(sfs_t *filesystem,uint64_t page,sfs_inode_t *inode);
//same here
int sfs_read_inode_header(sfs_t *filesystem,uint64_t page,sfs_inode_t *inode);
//insert after the given inode page
uint64_t sfs_inode_insert_continuation_page(sfs_t *filesystem,uint64_t page);
//remove given continuation page and adjust the others to point to the correct places
int sfs_inode_remove_continuation_page(sfs_t *filesystem,uint64_t page);
//helper function used in get and set pointer functions. moves the file cursor to the requested pointer while traversing continuation pages
int sfs_inode_seek_to_pointer(sfs_t *filesystem,uint64_t inode,uint64_t index);
//sets pointer in an inode at said index. deals with traversing continuation pages automaticaly
int sfs_inode_set_pointer(sfs_t *filesystem,uint64_t inode,uint64_t index,uint64_t pointer);
//the same as the set pointer function but returns the pointer value
uint64_t sfs_inode_get_pointer(sfs_t *filesystem,uint64_t inode,uint64_t index);
//changes the inode header to reflect the new number and removes or adds continuation pages to fit the new count
int sfs_inode_realocate_pointers(sfs_t *filesystem,uint64_t inode,uint64_t count);
// O(1) removal by replacing the requested pointer with the last pointer and decrementing the pointer count
//automaticaly calls the sfs_inode_reallocate_pointers to shrink the inode for you
int sfs_inode_remove_pointer(sfs_t *filesystem,uint64_t inode,uint64_t index);
//similar to set pointer but resizes inode and appends pointer to the end
int sfs_inode_add_pointer(sfs_t *filesystem,uint64_t inode,uint64_t pointer);
//creates an inode under a parent inode and returns the inode number of the created node
uint64_t sfs_inode_create(sfs_t *filesystem,const char *name,mode_t mode,uid_t uid,gid_t gid,uint64_t parent);

//====== regular files ======
//does both truncate and extending to change file size to new size
//leave bytes to zero as -1 to fill all new spots with '\0'
int sfs_file_resize(sfs_t *filesystem,uint64_t inode,uint64_t new_size,int64_t bytes_to_zero);
//read and write return (size_t)-1 on error
size_t sfs_file_read(sfs_t *filesystem,uint64_t inode,off_t offset,char buffer[],size_t len);
size_t sfs_file_write(sfs_t *filesystem,uint64_t inode,off_t offset,const char buffer[],size_t len);

//====== superblock ======
//closing the filesystem calls this, but it wont hurt to call this occasionaly
int sfs_update_superblock(sfs_t *filesystem);


//====== errors and debug ======
void sfs_perror(char *msg,int error);
void sfs_print_info();

const char *sfs_errno_to_str(int result);
#endif
