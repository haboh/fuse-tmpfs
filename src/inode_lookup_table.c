#include "inode_lookup_table.h"
#include "filesystem.h"
#include "log.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

void init_inode_lookup_table(inode_lookup_table *lt)
{
    lt->last_inode = 0;
}

inode_t *allocate_inode(inode_lookup_table *lt)
{

    lt->inode_lookup_table[lt->last_inode].inode = lt->last_inode;
    return &(lt->inode_lookup_table[lt->last_inode++]);
}

inode_t *get_inode_by_inode_num(inode_lookup_table *lt, inode_num_t inode_num)
{
    for (size_t i = 0; i < lt->last_inode; i++)
    {
        if (lt->inode_lookup_table[i].inode == inode_num)
        {
            return &(lt->inode_lookup_table[i]);
        }
    }
    return NULL;
}

void create_self_and_parent_link(inode_lookup_table *lt, inode_num_t self_inode_num, inode_num_t parent_inode_num)
{
    append_new_name_to_inode(
        get_inode_by_inode_num(lt, self_inode_num),
        self_inode_num,
        copy_string("."));
    append_new_name_to_inode(
        get_inode_by_inode_num(lt, self_inode_num),
        parent_inode_num,
        copy_string(".."));
}

void destroy_lookup_table(inode_lookup_table *lt)
{
    for (size_t i = 0; i < lt->last_inode; i++)
    {
        if (lt->inode_lookup_table[i].file_type == DIRECTORY_T)
        {
            for (size_t j = 0; j < get_dir_length(&lt->inode_lookup_table[i]); j++)
            {
                free(get_dir_content(&lt->inode_lookup_table[i])[j].name);
            }
        }
        free(lt->inode_lookup_table[i].data);
    }
}

void destroy_inode(inode_lookup_table *lt, inode_t *inode)
{
    free(inode->data);
}

int get_inode_by_name(inode_lookup_table *lt, const char *name, inode_num_t *inode_num)
{
    inode_t *inode = get_inode_by_inode_num(lt, *inode_num);
    for (size_t i = 0; i < get_dir_length(inode); i++)
    {
        char *filename = get_dir_content(inode)[i].name;
        if (strcmp(filename, name) == 0)
        {
            *inode_num = get_dir_content(inode)[i].inode;
            return 0;
        }
    }
    return -1;
}

int get_inode_by_path(inode_lookup_table *lt, const char *path, inode_num_t *inode, char **token)
{
    char *path_copy = copy_string(path);
    *token = strtok(path_copy, "/");
    *inode = 0;
    while (*token)
    {
        if (get_inode_by_name(lt, *token, inode) != 0)
        {
            return -ENOENT;
        }
        char *new_token = strtok(NULL, "/");
        if (new_token == NULL)
        {
            break;
        }
        *token = new_token;
    }
    return 0;
}

int get_prev_inode_by_path(inode_lookup_table *lt, const char *path, inode_num_t *inode, char **token)
{
    char *path_copy = copy_string(path);
    *token = strtok(path_copy, "/");
    *inode = 0;
    while (*token)
    {
        inode_num_t new_inode = *inode;
        if (get_inode_by_name(lt, *token, &new_inode) != 0)
        {
            return -ENOENT;
        }
        char *new_token = strtok(NULL, "/");
        if (new_token == NULL)
        {
            break;
        }
        *inode = new_inode;
        *token = new_token;
    }
    return 0;
}

bool is_inode_subinode(inode_lookup_table *lt, inode_t *inode, inode_t *subinode)
{
    if (inode->file_type != DIRECTORY_T)
    {
        return 0;
    }

    if (inode->inode == subinode->inode)
    {
        return 1;
    }
    // start from .. exclusive
    bool result = 0;
    for (size_t i = 2; i < get_dir_length(inode); i++)
    {
        inode_num_t dir_inode_num = get_dir_content(inode)[i].inode;
        result |= is_inode_subinode(lt, get_inode_by_inode_num(lt, dir_inode_num), subinode);
    }
    return result;
}
