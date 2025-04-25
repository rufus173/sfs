#include "../../include/sfs_functions.h"
#include "../../include/sfs_types.h"

#include <stdio.h>
#include <assert.h>

int main(int argc, char **argv){
	if (argc != 2){
		printf("usage: %s <file>\n",argv[0]);
		return 1;
	}
	uint64_t pages_to_create = 10;
	sfs_t filesystem;
	int result = sfs_open_fs(&filesystem,argv[1],SFS_FUNC_FLAG_SKIP_SUPERBLOCK_CHECK | SFS_FUNC_FLAG_O_CREATE);
	if (result < 0){
		perror("sfs_open_fs");
		return 1;
	}
	filesystem.first_free_page_index = (uint64_t)-1;
	filesystem.page_count = pages_to_create;
	sfs_update_superblock(&filesystem);

	//====== create the root inode ======
	sfs_inode_t root_inode = {
		.inode_type = SFS_INODE_T_DIR,
		.page = 1,
		.parent_inode_pointer = 1,
		.pointer_count = 0,
		.next_page = (uint64_t)-1,
		.previous_page = (uint64_t)-1,
		.name = {"/"}
	};
	sfs_write_inode_header(&filesystem,1,&root_inode);


	//====== generate empty pages of the requested amount ======
	for (uint64_t i = 2; i < pages_to_create; i++){
		sfs_free_page(&filesystem,i);
	}

	/* testing --- testing --- testing --- testing --- */
	uint64_t cont_page = sfs_inode_insert_continuation_page(&filesystem,1);
	assert(cont_page != -1);
	sfs_inode_t cont_page_inode;
	sfs_inode_t root_page;
	assert(sfs_read_inode_headers(&filesystem,cont_page,&cont_page_inode) == 0);
	assert(sfs_read_inode_headers(&filesystem,1,&root_page) == 0);

	sfs_close_fs(&filesystem,0);
	return 0;
}
