#include "../../include/sfs_types.h"
#include "../../include/sfs_functions.h"
#include "../libbst/libbst.h"
#include "../libtable/libtable.h"

#define FUSE_USE_VERSION 34

#include <fuse_lowlevel.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#define FUSE_ROOT_INODE 1

#define MAX_OPEN_DIRS 1024

//====== miscelanious prototypes ======
static int sfs_stat(fuse_ino_t ino, struct stat *statbuf);
static void sfs_opendir(fuse_req_t request,fuse_ino_t ino,struct fuse_file_info *file_info);
static void sfs_readdir(fuse_req_t request,fuse_ino_t ino,size_t size,off_t offset,struct fuse_file_info *file_info);
static void sfs_releasedir(fuse_req_t request,fuse_ino_t ino,struct fuse_file_info *file_info);
static void sfs_getattr(fuse_req_t request,fuse_ino_t ino,struct fuse_file_info *file_info);
static void sfs_lookup(fuse_req_t request,fuse_ino_t parent,const char *name);
static void sfs_forget(fuse_req_t request,fuse_ino_t ino, uint64_t lookup);
static void sfs_mkdir(fuse_req_t request,fuse_ino_t parent,const char *name,mode_t mode);
static void sfs_rmdir(fuse_req_t request, fuse_ino_t parent, const char *name);
static void show_usage(char *name);
int referenced_inodes_bst_cmp(void *a,void *b);
int increase_inode_ref_count(uint64_t inode, int count);
int decrease_inode_ref_count(uint64_t inode, int count);
void print_referenced_inode(void *);
uint64_t generate_unique_runid();
uint64_t inode_lookup_by_name(uint64_t parent,const char *name,sfs_inode_t *inode_return,uint64_t *inode_index_return);
void scheduled_rmdir(void *data);
struct open_dir_tracker *initialise_open_dir_tracker();

//====== prototypes for sfs_lowlevel_operations ======
struct fuse_lowlevel_ops sfs_lowlevel_operations = {
	.opendir = sfs_opendir,
	.readdir = sfs_readdir,
	.releasedir = sfs_releasedir,
	.getattr = sfs_getattr,
	.lookup = sfs_lookup,
	.forget = sfs_forget,
	.mkdir = sfs_mkdir,
	.rmdir = sfs_rmdir
};

//====== types ======
struct pointer_parent_inode_trio {
	uint64_t pointer;
	uint64_t parent;
	uint64_t inode;
};
struct referenced_inode {
	uint64_t inode;
	int reference_count;
	void *data;
	void (*destructor)(void *);
};
struct cached_dirent {
	struct stat statbuf;
	char name[SFS_MAX_FILENAME_SIZE];
};
struct cached_directory {
	uint64_t inode;
	uint64_t dirent_count;
	struct cached_dirent *dirent_array;
};

//====== globals ======
sfs_t *sfs_filesystem;
BST *referenced_inodes;
TABLE *cached_dirents;

int main(int argc, char **argv){
	//====== initialise various variables ======
	struct fuse_args f_args = FUSE_ARGS_INIT(1,argv);
	struct fuse_session *session;
	struct fuse_cmdline_opts options;
	static struct option long_options[] = {
		{"fuse-args",	required_argument,	0,'f'},
		{"help",	no_argument,		0,'h'}
	};
	//struct fuse_loop_config config;
	sfs_t filesystem;
	sfs_filesystem = &filesystem;
	struct bst_user_functions bst_funcs = {
		.free_data = free,
		.datacmp = referenced_inodes_bst_cmp,
		.print_data = print_referenced_inode
	};
	referenced_inodes = bst_new(&bst_funcs);
	cached_dirents = table_new(MAX_OPEN_DIRS);
	
	//====== process our custom arguments first ======
	for (;;){
		int option_index = 0;
		int result = getopt_long(argc,argv,"hf:",long_options,&option_index);
		if (result == -1) break; //end of option arguments
		switch(result){
			case 'f':
				//====== arguments to pass to fuse ======
				//append a dash
				char *full_arg = malloc(strlen(optarg)+2);
				full_arg[0] = '-';
				strcpy(full_arg+1,optarg);
				fuse_opt_add_arg(&f_args,full_arg);
				free(full_arg);
				break;
			case 'h':
				//help
				show_usage(argv[0]);
				sfs_print_info();
				return 0;
		}
	}
	//====== non option arguments ======
	if (argc-optind != 2){
		fprintf(stderr,"Please provide only the filesystem image and mountpoint\n");
		show_usage(argv[0]);
		return 0;
	}
	char *filesystem_path = argv[optind];
	char *mountpoint = argv[optind+1];

	//====== open the filesystem ======
	int result = sfs_open_fs(sfs_filesystem,filesystem_path,0);
	if (result < 0){
		fprintf(stderr,"Could not open filesystem.\n");
		return 1;
	}

	//====== parse arguments ======
	if (fuse_parse_cmdline(&f_args,&options) != 0) return 1;
	if (options.show_help) {
		show_usage(argv[0]);
		fuse_cmdline_help();
		fuse_lowlevel_help();
		fuse_opt_free_args(&f_args);
		return 0;
	}else if (options.show_version) {
		printf("FUSE library version %d\n", fuse_version());
		fuse_opt_free_args(&f_args);
		return 0;
	}
 
 	//====== create a session ======
	session = fuse_session_new(&f_args,&sfs_lowlevel_operations,sizeof(sfs_lowlevel_operations),NULL);
	if (session == NULL){
		fuse_opt_free_args(&f_args);
		return 1;
	}
	if (fuse_set_signal_handlers(session) != 0){
		fuse_session_destroy(session);
		fuse_opt_free_args(&f_args);
		return -1;
	}
	if (fuse_session_mount(session,mountpoint) != 0){
		fuse_remove_signal_handlers(session);
		fuse_session_destroy(session);
		fuse_opt_free_args(&f_args);
		return -1;
	}

	//fuse_daemonize(options.foreground);
	//single threaded (lets keep it simple)
	int return_val = fuse_session_loop(session);
	
	/*
	int return_val = 0;
	//====== TESTING = TESTING = TESTING ======
	printf("TESTING - TESTING - TESTING\n");
	for (int i = 0; i < 90; i++){
		printf("%lu\n",sfs_allocate_page(sfs_filesystem));
	}
	printf("%d\n",sfs_free_page(sfs_filesystem,4084));
	for (int i = 0; i < 90; i++){
		printf("%lu\n",sfs_allocate_page(sfs_filesystem));
	}
	*/

	//=========================================

	//====== cleanup ======
	fuse_session_unmount(session);
	fuse_remove_signal_handlers(session);
	fuse_session_destroy(session);
	fuse_opt_free_args(&f_args);

	bst_delete(referenced_inodes);
	table_delete(cached_dirents);

	return return_val;
}
static int sfs_stat(fuse_ino_t ino, struct stat *statbuf){
	sfs_inode_t inode;
	memset(statbuf,0,sizeof(struct stat));
	assert(sfs_read_inode_header(sfs_filesystem,ino,&inode) == 0);
	statbuf->st_ino = ino;
	//====== the permissions + filetype ======
	int mode = 0;
	switch(inode.inode_type){
	case SFS_INODE_T_DIR:
		mode = S_IFDIR;	break;
	case SFS_INODE_T_FILE:
		mode = S_IFREG;	break;
	}
	//permitions (not implemented)
	mode |= S_IRWXU;
	mode |= S_IRWXG;
	mode |= S_IRWXO;
	statbuf->st_mode = mode;
	//hard links not implemented
	statbuf->st_nlink = 1;
	//owners not implemented
	statbuf->st_uid = getuid();
	statbuf->st_gid = getgid();
	//other various fields
	statbuf->st_blksize = SFS_PAGE_SIZE;
	statbuf->st_blocks = inode.pointer_count;
	return 0;
}
static void sfs_opendir(fuse_req_t request,fuse_ino_t ino,struct fuse_file_info *file_info){
	printf("opendir requested on inode %lu\n",ino);
	printf("====== inode %lu contains ======\n",ino);
	//====== setup cache ======
	int cache_index = table_allocate_index(cached_dirents);
	if (cache_index == -1){
		assert(fuse_reply_err(request,EMFILE) == 0);
		return;
	}
	struct cached_directory *directory_cache = malloc(sizeof(struct cached_directory));
	table_set_data(cached_dirents,cache_index,directory_cache);
	//read the parent inode headers
	sfs_inode_t inode;
	assert(sfs_read_inode_header(sfs_filesystem,ino,&inode) == 0);
	directory_cache->inode = ino;
	directory_cache->dirent_count = inode.pointer_count;
	directory_cache->dirent_array = malloc(sizeof(struct cached_dirent)*inode.pointer_count);
	//====== cache all the dirents ======
	for (uint64_t pointer_index = 0; pointer_index < inode.pointer_count; pointer_index++){
		uint64_t dirent = sfs_inode_get_pointer(sfs_filesystem,ino,pointer_index);
		assert(sfs_stat(dirent,&directory_cache->dirent_array[pointer_index].statbuf) == 0);
		//read the name
		sfs_inode_t inode;
		assert(sfs_read_inode_header(sfs_filesystem,dirent,&inode) == 0);
		memcpy(directory_cache->dirent_array[pointer_index].name,inode.name,SFS_MAX_FILENAME_SIZE);
		printf("%lu [%s]\n",dirent,inode.name);
	}

	printf("====== end of inode ======\n");
	//====== send it off ======
	file_info->fh = cache_index;
	assert(fuse_reply_open(request,file_info) == 0);
}
static void sfs_readdir(fuse_req_t request,fuse_ino_t ino,size_t size,off_t offset,struct fuse_file_info *file_info){
	//====== read the cache ======
	struct cached_directory *directory_cache = table_get_data(cached_dirents,file_info->fh);
	if (offset >= directory_cache->dirent_count){
		//====== no more dirents ======
		assert(fuse_reply_buf(request,NULL,0) == 0);
	}else{
		//====== send the next dirent ======
		//get required buffer size
		size_t bufsize = fuse_add_direntry(request,NULL,0,directory_cache->dirent_array[offset].name,&directory_cache->dirent_array[offset].statbuf,offset+1);
		assert(bufsize <= size);
		char *buf = malloc(bufsize);
		//pack the buffer
		assert(fuse_add_direntry(request,buf,bufsize,directory_cache->dirent_array[offset].name,&directory_cache->dirent_array[offset].statbuf,offset+1) <= bufsize);
		//send the buffer
		assert(fuse_reply_buf(request,buf,bufsize) == 0);
		//free the buffer
		free(buf);
	}
}
static void sfs_releasedir(fuse_req_t request,fuse_ino_t ino,struct fuse_file_info *file_info){
	//free all the cached data
	struct cached_directory *directory_cache = table_get_data(cached_dirents,file_info->fh);
	free(directory_cache->dirent_array);
	free(directory_cache);
	table_free_index(cached_dirents,file_info->fh);
	//send success
	assert(fuse_reply_err(request,0) == 0);
}
static void sfs_getattr(fuse_req_t request,fuse_ino_t ino,struct fuse_file_info *file_info){
	printf("getattr requested on inode %lu\n",ino);
	//send the gathered attribute back to the kernel
	struct stat attr;
	assert(sfs_stat(ino,&attr) == 0);
	assert(fuse_reply_attr(request,&attr,1.0) == 0);
}
static void show_usage(char *name){
	printf("usage: %s [options] <filesystem image> <mountpoint>\n",name);
}
static void sfs_lookup(fuse_req_t request,fuse_ino_t parent,const char *name){
	printf("lookup requested on parent %lu for %s\n",parent,name);
	//====== check parent is a directory ======
	sfs_inode_t inode;
	assert(sfs_read_inode_header(sfs_filesystem,parent,&inode) == 0);
	if (inode.inode_type != SFS_INODE_T_DIR){
		fuse_reply_err(request,ENOTDIR);
		return;
	}
	//====== itterate through all the entries in a directory ======
	for (uint64_t i = 0; i < inode.pointer_count;i++){
		uint64_t sub_inode_pointer = sfs_inode_get_pointer(sfs_filesystem,parent,i);
		assert(sub_inode_pointer != (uint64_t)-1);
		sfs_inode_t sub_inode;
		assert(sfs_read_inode_header(sfs_filesystem,sub_inode_pointer,&sub_inode) == 0);
		if (strcmp(sub_inode.name,name) == 0){
			//====== directory found ======
			//generate the dir entry
			struct fuse_entry_param entry = {
				.ino = sub_inode_pointer,
				.generation = sub_inode.generation_number,
				.attr_timeout = 1.0,
				.entry_timeout = 1.0,
			};
			//fill in the stat struct
			assert(sfs_stat(sub_inode_pointer,&entry.attr) == 0);
			//send it off
			fuse_reply_entry(request,&entry);
			//increment reference count
			increase_inode_ref_count(sub_inode_pointer,1);
			//leave
			return;
		}
	}
	//====== if it does not exist ======
	fuse_reply_err(request,ENOENT);
}
static void sfs_mkdir(fuse_req_t request,fuse_ino_t parent,const char *name,mode_t mode){
	//TODO: fix issues where creating one directory changes the name of others
	printf("mkdir requested for [%s] with parent %lu\n",name,parent);
	//====== verify it doesnt already exist ======
	if (inode_lookup_by_name(parent,name,NULL,NULL) != (uint64_t)-1){
		//file / folder exists already
		fuse_reply_err(request,EEXIST);
		return;
	}
	//====== create the new inode ======
	uint64_t new_inode = sfs_inode_create(sfs_filesystem,name,SFS_INODE_T_DIR,parent);
	assert(new_inode != (uint64_t)-1);
	printf("mkdir created new inode %lu\n",new_inode);
	//====== return the entry for the new inode ======
	sfs_inode_t inode_header;
	assert(sfs_read_inode_header(sfs_filesystem,new_inode,&inode_header) == 0);
	struct fuse_entry_param entry = {
		.ino = new_inode,
		.generation = inode_header.generation_number,
		.attr_timeout = 1.0,
		.entry_timeout = 1.0,
	};
	assert(sfs_stat(new_inode,&entry.attr) == 0);
	increase_inode_ref_count(new_inode,1);
	assert(fuse_reply_entry(request,&entry) == 0);
}
int referenced_inodes_bst_cmp(void *a,void *b){
	int value;
	struct referenced_inode *inode_a = a;
	struct referenced_inode *inode_b = b;
	value = inode_b->inode - inode_a->inode;
	return value;
}
int increase_inode_ref_count(uint64_t inode, int count){
	//====== create node to hold ref count if needed ======
	struct referenced_inode *referenced_inode_to_match = malloc(sizeof(struct referenced_inode));
	memset(referenced_inode_to_match,0,sizeof(struct referenced_inode));
	referenced_inode_to_match->inode = inode;
	struct bst_node *ref_node = bst_find_node(referenced_inodes,referenced_inode_to_match);
	if (ref_node == NULL){
		//create a new node
		ref_node = bst_new_node(referenced_inodes,referenced_inode_to_match);
	}else{
		free(referenced_inode_to_match);
	}
	//====== increment ref count ======
	struct referenced_inode *ref_count_node = ref_node->data;
	ref_count_node->reference_count += count;

	printf("====== reference counts ======\n");
	bst_print_nodes_inorder(referenced_inodes);
	printf("====== end reference counts ======\n");
	return 0;
}
int decrease_inode_ref_count(uint64_t inode, int count){
	//====== locate the inode in the bst ======
	struct referenced_inode referenced_inode_to_match;
	memset(&referenced_inode_to_match,0,sizeof(struct referenced_inode));
	referenced_inode_to_match.inode = inode;
	struct bst_node *bst_node = bst_find_node(referenced_inodes,&referenced_inode_to_match);
	if (bst_node == NULL){
		//node does not have a stored reference count, so do nothing
		return 1;
	}
	struct referenced_inode *ref_node = bst_node->data;
	if (ref_node == NULL) return -1; //inode does not have a reference count entry
	//decrease the reference count
	ref_node->reference_count -= count;
	if (ref_node->reference_count < 1){
		//====== inode reference count reached zero ======
		//call the destructor if there is one
		if (ref_node->destructor != NULL){
			ref_node->destructor(ref_node->data);
		}
		//remove the ref node from the bst
		bst_delete_node(referenced_inodes,bst_node);
	}

	return 0;
}
void print_referenced_inode(void *data){
	struct referenced_inode *inode_reference = data;
	printf("(inode %lu - %d)\n",inode_reference->inode,inode_reference->reference_count);
}
//good luck exausting 18446744073709551615 runids
uint64_t generate_unique_runid(){
	static uint64_t current_runid = 0;
	current_runid++;
	return current_runid;
}

static void sfs_forget(fuse_req_t request,fuse_ino_t ino, uint64_t lookup){
	printf("inode %lu forgotten\n",ino);
	decrease_inode_ref_count(ino,lookup);
	//no reply required
	fuse_reply_none(request);
}
uint64_t inode_lookup_by_name(uint64_t parent,const char *name,sfs_inode_t *inode_header_return,uint64_t *pointer_index_return){
	//====== get pointer count ======
	sfs_inode_t parent_header;
	sfs_read_inode_header(sfs_filesystem,parent,&parent_header);
	uint64_t pointer_count = parent_header.pointer_count;
	//====== linear search for matching name ======
	for (uint64_t i = 0; i < pointer_count; i++){
		//get the inode
		uint64_t inode = sfs_inode_get_pointer(sfs_filesystem,parent,i);
		//read the inode header
		sfs_inode_t child_inode_header;
		sfs_read_inode_header(sfs_filesystem,inode,&child_inode_header);
		//compare the name to the name we were given
		if (strcmp(child_inode_header.name,name) == 0){
			//copy the header info if requested
			if (inode_header_return != NULL) memcpy(inode_header_return,&child_inode_header,sizeof(sfs_inode_t));
			//copy the index if requested
			if (pointer_index_return != NULL) *pointer_index_return = i;
			//return the inode number
			return inode;
		}
	}
	//failure to find
	return (uint64_t)-1;
}
static void sfs_rmdir(fuse_req_t request, fuse_ino_t parent, const char *name){
	//====== find the inode ======
	uint64_t index;
	uint64_t inode = inode_lookup_by_name(parent,name,NULL,&index);
	if (inode == (uint64_t)-1){
		fuse_reply_err(request,ENOENT);
		return;
	}
	//====== schedule removal ======
	//prepare data
	struct pointer_parent_inode_trio *data = malloc(sizeof(struct pointer_parent_inode_trio));
	data->pointer = index;
	data->parent = parent;
	data->inode = inode;
	//find entry
	struct referenced_inode match = {
		.inode = inode
	};
	struct bst_node *node = bst_find_node(referenced_inodes,&match);
	if (node == NULL){
		//if unreferenced delete now
		scheduled_rmdir(data);
		goto success;
	}
	//schedule deletion
	((struct referenced_inode *)(node->data))->destructor = scheduled_rmdir;
	((struct referenced_inode *)(node->data))->data = data;
	
	//signal success
	success:
	fuse_reply_err(request,0);
}
void scheduled_rmdir(void *data){
	//====== unpack data ======
	struct pointer_parent_inode_trio *typed_data = data;
	uint64_t pointer = typed_data->pointer;
	uint64_t parent = typed_data->parent;
	uint64_t inode = typed_data->inode;
	printf("deleting inode %lu\n",inode);

	//====== delete the reference in the parent ======
	assert(sfs_inode_remove_pointer(sfs_filesystem,parent,pointer) == 0);
	//====== free the page ======
	assert(sfs_free_page(sfs_filesystem,inode) == 0);

	free(data);
}
