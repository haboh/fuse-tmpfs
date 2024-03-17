#include "inode_lookup_table.h"
#include <stdlib.h>

void init_inode_lookup_table(inode_lookup_table* lt) {
    lt->last_inode = 0;
}

inode_t* allocate_inode(inode_lookup_table* lt) {
    // TODO: add check
    lt->inode_lookup_table[lt->last_inode].inode = lt->last_inode;
    return &(lt->inode_lookup_table[lt->last_inode++]);
}

inode_t* get_inode(inode_lookup_table* lt, inode_num_t inode_num) {
    for (size_t i = 0; i < lt->last_inode; i++) {
        if (lt->inode_lookup_table[i].inode == inode_num) {
            return &(lt->inode_lookup_table[i]);
        }
    }
    return NULL;
}

void append_new_name_to_inode(inode_t *inode, inode_num_t new_file_inode_num, char *name) {
    inode->dir_content = realloc(
        inode->dir_content
        , sizeof(dir_content_t) * (inode->dir_content_length + 1)
    );

    int index = inode->dir_content_length++;
    inode->dir_content[index].inode = new_file_inode_num;
    inode->dir_content[index].name = name;
}

void destroy_lookup_table(inode_lookup_table *lt) {
    for (size_t i = 0; i < lt->last_inode; i++) {
        for (int j = 0; j < lt->inode_lookup_table[i].dir_content_length; j++) {
            free(lt->inode_lookup_table[i].dir_content[j].name);
        }
        free(lt->inode_lookup_table[i].dir_content);
        free(lt->inode_lookup_table[i].data);
    }
}