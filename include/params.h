#ifndef _PARAMS_H_
#define _PARAMS_H_

#define FUSE_USE_VERSION 26

#define _XOPEN_SOURCE 500

#include "inode_lookup_table.h"

#include <limits.h>
#include <stdio.h>
struct tmp_state
{
  FILE *logfile;
  char *rootdir;
  uid_t uid_init;
  gid_t gid_init;
  inode_lookup_table inode_table;
};
#define TMP_DATA ((struct tmp_state *)fuse_get_context()->private_data)
#define TMP_LOOKUP_TABLE (&TMP_DATA->inode_table)

#endif
