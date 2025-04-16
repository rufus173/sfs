#ifndef _SFS_FUNCTIONS_H
#define _SFS_FUNCTIONS_H

#include "sfs_types.h"

//====== open and close ======
int sfs_open_fs(sfs_t *filesystem,const char *path,int flags);
int sfs_close_fs(sfs_t *filesystem,int flags);

int read_page(sfs_t *filesystem,uint64_t page);
#endif
