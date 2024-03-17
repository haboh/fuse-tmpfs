#ifndef INODE_H
#define INODE_H

#include <stddef.h>

typedef size_t inode_num_t;

enum FILE_TYPE_T
{
    FILE_T = 0
    , DIRECTORY_T = 1
    , LINK = 2
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

    char *data;
    size_t data_len;

    enum FILE_TYPE_T file_type;

    dir_content_t *dir_content;
    size_t dir_content_length;

    int nlink;
} inode_t;

#endif