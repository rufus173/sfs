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
		.pointer_count = 10,
		.next_page = (uint64_t)-1,
		.previous_page = (uint64_t)-1,
		.name = {"/"}
	};
	sfs_write_inode_header(&filesystem,1,&root_inode);


	//====== generate empty pages of the requested amount ======
	for (uint64_t i = 2; i < pages_to_create; i++){
		sfs_free_page(&filesystem,i);
	}
	sfs_inode_set_pointer(&filesystem,1,9,6969);

	/* testing --- testing --- testing --- testing --- */
	sfs_inode_t root_page;
	assert(sfs_read_inode_header(&filesystem,1,&root_page) == 0);
	assert(sfs_inode_realocate_pointers(&filesystem,1,400) == 0);
	assert(sfs_read_inode_header(&filesystem,1,&root_page) == 0);

	assert(sfs_inode_set_pointer(&filesystem,1,380,69420) == 0);
	assert(sfs_read_inode_header(&filesystem,root_page.next_page,&root_page) == 0);
	printf("%lu\n",sfs_inode_get_pointer(&filesystem,1,380));


	sfs_close_fs(&filesystem,0);
	return 0;
}
