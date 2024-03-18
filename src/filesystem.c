#include "filesystem.h"
#include "log.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

dir_content_t* get_dir_content(inode_t *inode) {
    assert(inode->file_type == DIRECTORY_T && "File is not directory");
    return (dir_content_t*)inode->data;
}

size_t get_dir_length(inode_t *inode) {
    assert(inode->file_type == DIRECTORY_T && "File is not directory");
    return inode->data_len;
}

void append_new_name_to_inode(inode_t *inode, inode_num_t new_file_inode_num, char *name) {
    assert((inode->file_type == DIRECTORY_T || inode->file_type == LINK_T) && "File is not directory");

    inode->data = realloc(
        inode->data
        , sizeof(dir_content_t) * (inode->data_len + 1)
    );
    
    if (inode->data == NULL) {
        perror("out of memory");
        abort();
    }

    int index = inode->data_len++;
    get_dir_content(inode)[index].inode = new_file_inode_num;
    get_dir_content(inode)[index].name = name;
}

void remove_name_from_inode(inode_t *inode, char *name) {
    dir_content_t* dir_content = get_dir_content(inode);
    size_t dir_length = get_dir_length(inode);
    for (size_t i = 0; i < dir_length; i++) {
        if (strcmp(dir_content[i].name, name) == 0) {
            dir_content[i] = dir_content[dir_length - 1];
            inode->data = realloc
            (
                (void*)dir_content, (dir_length - 1) * sizeof(dir_content_t)
            );
            if (inode->data == NULL) {
                perror("undefined error");
                abort();
            }
            inode->data_len--;
            break;
        }
    }
}

bool is_inode_subinode(inode_t* inode, inode_t* subinode) {
    if (inode->inode == subinode->inode) {
        return 1;
    }
    // start from .. exclusive
    bool result = 0;
    for (size_t i = 2; i < get_dir_length(inode); i++) {
        result |= is_inode_subinode(&get_dir_content(inode)[i], subinode);
    }
    return result;
}