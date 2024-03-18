#ifndef INODE_LOOKUP_TABLE_H
#define INODE_LOOKUP_TABLE_H

#include "filesystem.h"

#define INODE_COUNT (1000)

typedef struct {
    size_t last_inode;
    inode_t inode_lookup_table[INODE_COUNT];
} inode_lookup_table;

void init_inode_lookup_table(inode_lookup_table* lt);
void destroy_lookup_table(inode_lookup_table* lt);

inode_t* allocate_inode(inode_lookup_table* lt);
void create_self_and_parent_link(inode_lookup_table *lt, inode_num_t self_inode_num, inode_num_t parent_inode_num);

inode_t* get_inode_by_inode_num(inode_lookup_table* lt, inode_num_t inode_num);
int get_inode_by_path(inode_lookup_table* lt, const char *path, inode_num_t *inode, char **token);

#endif // INODE_LOOKUP_TABLE_H