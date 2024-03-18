#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdbool.h>
#include <stddef.h>

typedef size_t inode_num_t;

enum FILE_TYPE_T
{
      FILE_T      = 0
    , DIRECTORY_T = 1
    , LINK_T      = 2
};

typedef struct
{
    char *name;
    inode_num_t inode;
} dir_content_t;

typedef struct
{
    inode_num_t parent_inode;
    inode_num_t inode;

    void *data;
    size_t data_len;

    enum FILE_TYPE_T file_type;

    int nlink;
} inode_t;

dir_content_t* get_dir_content(inode_t *inode);
size_t get_dir_length(inode_t *inode);

void append_new_name_to_inode(inode_t *inode, inode_num_t new_file_inode_num, char *name);
void remove_name_from_inode(inode_t *inode, char *name);

bool is_inode_subinode(inode_t* inode, inode_t* subinode);

#endif // FILESYSTE_H