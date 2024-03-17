#ifndef INODE_LOOKUP_TABLE_H
#define INODE_LOOKUP_TABLE_H

#include "filesystem.h"

#define INODE_COUNT (1000)

typedef struct {
    size_t last_inode;
    inode_t inode_lookup_table[INODE_COUNT];
} inode_lookup_table;

void init_inode_lookup_table(inode_lookup_table* lt);
inode_t* allocate_inode(inode_lookup_table* lt);
inode_t* get_inode(inode_lookup_table* lt, inode_num_t inode_num);
void append_new_name_to_inode(inode_t *inode, inode_num_t new_file_inode_num, char *name);
void destroy_lookup_table(inode_lookup_table* lt);

#endif // INODE_LOOKUP_TABLE_H