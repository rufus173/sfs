#include "../../include/sfs_types.h"
#include "../../include/sfs_functions.h"

#define FUSE_USE_VERSION 34

#include <fuse_lowlevel.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>

#define FUSE_ROOT_INODE 1

//====== miscelanious prototypes ======
static int sfs_stat(fuse_ino_t ino, struct stat *statbuf);
static void sfs_opendir(fuse_req_t request,fuse_ino_t ino,struct fuse_file_info *file_info);
static void sfs_readdir(fuse_req_t request,fuse_ino_t ino,size_t size,off_t offset,struct fuse_file_info *file_info);
static void sfs_releasedir(fuse_req_t request,fuse_ino_t ino,struct fuse_file_info *file_info);
static void sfs_getattr(fuse_req_t request,fuse_ino_t ino,struct fuse_file_info *file_info);
static void show_usage(char *name);

//====== prototypes for sfs_lowlevel_operations ======
struct fuse_lowlevel_ops sfs_lowlevel_operations = {
	.opendir = sfs_opendir,
	.readdir = sfs_readdir,
	.releasedir = sfs_releasedir,
	.getattr = sfs_getattr
};

sfs_t *sfs_filesystem;

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

	//====== cleanup ======
	fuse_session_unmount(session);
	fuse_remove_signal_handlers(session);
	fuse_session_destroy(session);
	fuse_opt_free_args(&f_args);

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
	file_info->fh = ino;
	assert(fuse_reply_open(request,file_info) == 0);
}
static void sfs_readdir(fuse_req_t request,fuse_ino_t ino,size_t size,off_t offset,struct fuse_file_info *file_info){
	printf("readdir requested on inode %lu, requested size %lu\n",ino,size);
	uint64_t dirent = sfs_inode_get_pointer(sfs_filesystem,file_info->fh,offset);
	if (dirent == (uint64_t)-1){
		//====== no more dirents ======
		printf("--> end of stream\n");
		assert(fuse_reply_buf(request,NULL,0) == 0);
	}else{
		//====== send the next dirent ======
		struct stat statbuf;
		sfs_inode_t inode;
		assert(sfs_read_inode_header(sfs_filesystem,dirent,&inode) == 0);
		assert(sfs_stat(dirent,&statbuf) == 0);
		printf("--> inode %lu, name %s\n",dirent,inode.name);
		//get required buffer size
		size_t bufsize = fuse_add_direntry(request,NULL,0,inode.name,&statbuf,offset+1);
		assert(bufsize <= size);
		char *buf = malloc(bufsize);
		//pack the buffer
		assert(fuse_add_direntry(request,buf,bufsize,inode.name,&statbuf,offset+1) <= bufsize);
		//send the buffer
		assert(fuse_reply_buf(request,buf,bufsize) == 0);
		//free the buffer
		free(buf);
	}
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
static void show_usage(char *name){
	printf("usage: %s [options] <filesystem image> <mountpoint>\n",name);
}
