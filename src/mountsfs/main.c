#include "../../include/sfs_types.h"
#include "../../include/sfs_functions.h"

#define FUSE_USE_VERSION 34

#include <fuse_lowlevel.h>
#include <stdlib.h>
#include <stdio.h>

struct fuse_lowlevel_ops sfs_lowlevel_operations = {
};

int main(int argc, char **argv){
	//====== initialise fuse variables ======
	struct fuse_args args = FUSE_ARGS_INIT(argc,argv);
	struct fuse_session *session;
	struct fuse_cmdline_opts options;
	struct fuse_loop_config config;

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

	//====== cleanup ======
	free(options.mountpoint);
	fuse_opt_free_args(&args);

	return 0;
}
