#include "../../include/sfs_types.h"
#include "../../include/sfs_functions.h"

#define FUSE_USE_VERSION 34

#include <fuse_lowlevel.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#define FUSE_ROOT_INODE 1

//====== miscelanious prototypes ======
static int sfs_stat(fuse_ino_t ino, struct stat *statbuf);
static void sfs_opendir(fuse_req_t request,fuse_ino_t ino,struct fuse_file_info *file_info);
static void sfs_readdir(fuse_req_t request,fuse_ino_t ino,size_t size,off_t offset,struct fuse_file_info *file_info);
static void sfs_releasedir(fuse_req_t request,fuse_ino_t ino,struct fuse_file_info *file_info);
static void sfs_getattr(fuse_req_t request,fuse_ino_t ino,struct fuse_file_info *file_info);

//====== prototypes for sfs_lowlevel_operations ======
struct fuse_lowlevel_ops sfs_lowlevel_operations = {
	.opendir = sfs_opendir,
	.readdir = sfs_readdir,
	.releasedir = sfs_releasedir,
	.getattr = sfs_getattr
};

sfs_t *sfs_filesystem;

int main(int argc, char **argv){
	//====== initialise fuse variables ======
	struct fuse_args args = FUSE_ARGS_INIT(argc,argv);
	struct fuse_session *session;
	struct fuse_cmdline_opts options;
	//struct fuse_loop_config config;
	sfs_t filesystem;
	sfs_filesystem = &filesystem;

	//====== parse arguments ======
	if (fuse_parse_cmdline(&args,&options) != 0) return 1;
	if (options.show_help) {
		printf("usage: %s [options] <mountpoint>\n", argv[0]);
		fuse_cmdline_help();
		fuse_lowlevel_help();
		free(options.mountpoint);
		fuse_opt_free_args(&args);
		return 0;
	}else if (options.show_version) {
		printf("FUSE library version %d\n", fuse_version());
		free(options.mountpoint);
		fuse_opt_free_args(&args);
		return 0;
	}
	if (options.mountpoint == NULL) {
		printf("usage: %s [options] <mountpoint>\n", argv[0]);
		printf("       %s --help\n", argv[0]);
		free(options.mountpoint);
		fuse_opt_free_args(&args);
		return 1;
	}
 
 	//====== create a session ======
	session = fuse_session_new(&args,&sfs_lowlevel_operations,sizeof(sfs_lowlevel_operations),NULL);
	if (session == NULL){
		free(options.mountpoint);
		fuse_opt_free_args(&args);
		return 1;
	}
	if (fuse_set_signal_handlers(session) != 0){
		fuse_session_destroy(session);
		free(options.mountpoint);
		fuse_opt_free_args(&args);
		return -1;
	}
	if (fuse_session_mount(session,options.mountpoint) != 0){
		fuse_remove_signal_handlers(session);
		fuse_session_destroy(session);
		free(options.mountpoint);
		fuse_opt_free_args(&args);
		return -1;
	}

	//fuse_daemonize(options.foreground);
	//single threaded (lets keep it simple)
	int return_val = fuse_session_loop(session);

	//====== cleanup ======
	fuse_session_unmount(session);
	fuse_remove_signal_handlers(session);
	fuse_session_destroy(session);
	free(options.mountpoint);
	fuse_opt_free_args(&args);

	return return_val;
}
static int sfs_stat(fuse_ino_t ino, struct stat *statbuf){
	sfs_inode_t inode;
	memset(statbuf,0,sizeof(struct stat));
	assert(sfs_read_inode_headers(sfs_filesystem,ino,&inode) == 0);
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
	file_info->fh = ino;
	assert(fuse_reply_open(request,file_info) == 0);
}
static void sfs_readdir(fuse_req_t request,fuse_ino_t ino,size_t size,off_t offset,struct fuse_file_info *file_info){
	printf("readdir requested on inode %lu, requested size %lu\n",ino,size);
	assert(fuse_reply_buf(request,NULL,0) == 0);
}
static void sfs_releasedir(fuse_req_t request,fuse_ino_t ino,struct fuse_file_info *file_info){
	//send success
	assert(fuse_reply_err(request,0) == 0);
}
static void sfs_getattr(fuse_req_t request,fuse_ino_t ino,struct fuse_file_info *file_info){
	//send the gathered attribute back to the kernel
	struct stat attr;
	assert(sfs_stat(ino,&attr) == 0);
	assert(fuse_reply_attr(request,&attr,1.0) == 0);
}
