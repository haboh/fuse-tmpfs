#include "config.h"
#include "params.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "log.h"

#include "inode_lookup_table.h"
#include "filesystem.h"

char* copy_static_string(const char *s) {
    int size = strlen(s) + 1;
    char *new_string = malloc(size);
    strcpy(new_string, s);
    return new_string;
}

void create_self_and_parent_link(inode_num_t self_inode_num, inode_num_t parent_inode_num)
{
    log_msg("boba");
    append_new_name_to_inode
    (
        get_inode(&TMP_DATA->inode_table, self_inode_num),
        self_inode_num,
        copy_static_string(".")
    );
    append_new_name_to_inode
    (
        get_inode(&TMP_DATA->inode_table, self_inode_num),
        parent_inode_num,
        copy_static_string("..")
    );
    log_msg("biba");
}

int get_inode_by_name(const char *name, inode_num_t *inode_num)
{
    inode_t *inode = get_inode(&TMP_DATA->inode_table, *inode_num);
    for (size_t i = 0; i < inode->dir_content_length; i++)
    {
        char *filename = inode->dir_content[i].name;
        if (filename != NULL && strcmp(filename, name) == 0)
        {
            *inode_num = inode->dir_content[i].inode;
            return 0;
        }
    }
    return -1;
}

int get_inode_num(const char *path, inode_num_t *inode, char **token)
{
    char *path_copy = malloc(strlen(path) + 1);
    strcpy(path_copy, path);
    *token = strtok(path_copy, "/");
    *inode = 0;
    while (*token)
    {
        if (get_inode_by_name(*token, inode) != 0)
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

int tmp_getattr(const char *path, struct stat *statbuf)
{
    log_msg("\ntmp_getattr(path=\"%s\", statbuf=0x%08x)\n",
            path, statbuf);

    inode_num_t inode_num;
    char *token;
    int search_res = get_inode_num(path, &inode_num, &token);
    log_msg("search res: %d\n", search_res);
    if (search_res != 0)
    {
        return search_res;
    }

    inode_t *inode = get_inode(&TMP_DATA->inode_table, inode_num);

    mode_t st_mode = S_IRWXU | S_IRWXG | S_IRWXO; // change
    if (inode->file_type == FILE_T)
    {
        st_mode = S_IFREG;
    }
    else
    {
        st_mode = S_IFDIR;
    }

    struct stat new_stat_buf = {
        .st_ino = inode->inode,
        .st_mode = st_mode,
        .st_nlink = inode->nlink,
        .st_size = inode->data_len,
        .st_dev = 0,
        .st_uid = 0, // change
        .st_gid = 0, // change
        .st_rdev = 0,
        .st_blksize = 0,
        .st_blocks = 0,
        .st_atime = 0,
        .st_mtime = 0,
        .st_ctime = 0};

    *statbuf = new_stat_buf;
    log_stat(statbuf);

    return 0;
}

int mk_item(enum FILE_TYPE_T type, const char *path, inode_num_t inode_num, char *token)
{
    log_msg("file not exist %s\n", path);

    inode_t new_file_inode_value = {
        .data = NULL,
        .data_len = 0,
        .dir_content = NULL,
        .file_type = type,
        .inode = 0,
        .parent_inode = inode_num,
        .nlink = 0
        // add mode
    };

    inode_t *inode = get_inode(&TMP_DATA->inode_table, inode_num);
    inode_t *new_file_inode = allocate_inode(&TMP_DATA->inode_table);
    if (new_file_inode == NULL)
    {
        return -ENOSPC;
    }
    new_file_inode_value.inode = new_file_inode->inode;

    append_new_name_to_inode(inode, new_file_inode->inode, token);

    if (type == DIRECTORY_T) {
        create_self_and_parent_link(new_file_inode->inode, inode->inode);
    }

    *new_file_inode = new_file_inode_value;
    return 0;
}

int tmp_mknod(const char *path, mode_t mode, dev_t dev)
{
    log_msg("\ntmp_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n",
            path, mode, dev);
    inode_num_t inode;
    char *token;
    if (get_inode_num(path, &inode, &token) == 0)
    {
        // file already exist
        return -EEXIST;
    }
    else
    {
        // file not exist
        return mk_item(FILE_T, path, inode, token);
    }
}

/** Create a directory */
int tmp_mkdir(const char *path, mode_t mode)
{
    inode_num_t inode;
    char *token;
    if (get_inode_num(path, &inode, &token) == 0)
    {
        // file already exist
        return -EEXIST;
    }
    else
    {
        // file not exist
        return mk_item(DIRECTORY_T, path, inode, token);
    }
}

/** Remove a file */
int tmp_unlink(const char *path)
{
    char fpath[PATH_MAX];

    log_msg("tmp_unlink(path=\"%s\")\n",
            path);

    // TODO

    return 0;
}

/** Remove a directory */
int tmp_rmdir(const char *path)
{
    char fpath[PATH_MAX];

    inode_num_t inode_num;
    char *token;
    if (get_inode_num(path, &inode_num, &token) != 0)
    {
        // file already exist
        return -ENOENT;
    }
    else
    {
        log_msg("token: %s\n", token);
        inode_t *inode = get_inode(&TMP_DATA->inode_table, get_inode(&TMP_DATA->inode_table, inode_num)->parent_inode);
        for (size_t i = 0; i < inode->dir_content_length; i++)
        {
            const char *name = inode->dir_content[i].name;
            if (name != NULL && strcmp(name, token) == 0)
            {
                inode->dir_content[i].name = NULL;
            }
        }
    }

    log_msg("tmp_rmdir(path=\"%s\")\n",
            path);

    return 0;
}

// both path and newpath are fs-relative
int tmp_rename(const char *path, const char *newpath)
{
    log_msg("\ntmp_rename(fpath=\"%s\", newpath=\"%s\")\n",
            path, newpath);

    // TODO

    return 0;
}

int tmp_link(const char *path, const char *newpath)
{
    log_msg("\ntmp_link(path=\"%s\", newpath=\"%s\")\n",
            path, newpath);
    // TODO
    return 0;
}

int tmp_truncate(const char *path, off_t newsize)
{
    inode_num_t inode_num;
    char *token;

    if (get_inode_num(path, &inode_num, &token) != 0)
    {
        return log_error("truncate");
    }

    inode_t *inode = get_inode(&TMP_DATA->inode_table, inode_num);

    inode->data_len = newsize;
    inode->data = realloc(inode->data, newsize);

    log_msg("\ntmp_truncate(path=\"%s\", newsize=%lld)\n",
            path, newsize);

    return 0;
}

int tmp_open(const char *path, struct fuse_file_info *fi)
{
    inode_num_t inode;
    char *token;

    if (get_inode_num(path, &inode, &token) != 0)
    {
        return log_error("open");
    }

    // if the open call succeeds, my retstat is the file descriptor,
    // else it's -errno.  I'm making sure that in that case the saved
    // file descriptor is exactly -1.

    fi->fh = inode;

    log_fi(fi);

    return 0;
}

int tmp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;

    log_msg("\ntmp_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
            path, buf, size, offset, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);

    inode_num_t inode_num;
    char *token;
    if (get_inode_num(path, &inode_num, &token) != 0)
    {
        return 0;
    }

    inode_t *inode = get_inode(&TMP_DATA->inode_table, inode_num);

    size_t bytes_read_count = offset + size <= inode->data_len ? size : inode->data_len - offset;

    memcpy(buf, inode->data, bytes_read_count);

    return bytes_read_count;
}

int tmp_write(const char *path, const char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi)
{
    int retstat = 0;

    log_msg("\ntmp_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
            path, buf, size, offset, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);

    inode_num_t inode_num;
    char *token;
    if (get_inode_num(path, &inode_num, &token) != 0)
    {
        return 0;
    }

    inode_t *inode = get_inode(&TMP_DATA->inode_table, inode_num);

    if (offset + size > inode->data_len)
    {
        inode->data_len = offset + size;
        inode->data = realloc(inode->data, offset + size);
    }

    memcpy(inode->data + offset, buf, size);
    return size;
}

int tmp_release(const char *path, struct fuse_file_info *fi)
{
    log_msg("\ntmp_release(path=\"%s\", fi=0x%08x)\n",
            path, fi);
    log_fi(fi);

    return 0;
}

int tmp_opendir(const char *path, struct fuse_file_info *fi)
{
    log_msg("\ntmp_opendir(path=\"%s\", fi=0x%08x)\n",
            path, fi);

    inode_num_t inode;
    char *token;
    if (get_inode_num(path, &inode, &token) != 0)
    {
        return log_error("tmp_opendir opendir");
    }

    fi->fh = inode;

    log_fi(fi);
    return 0;
}

int tmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
                struct fuse_file_info *fi)
{

    inode_num_t inode_num;
    inode_t *inode;
    char *token;
    if (get_inode_num(path, &inode_num, &token) != 0 || (inode = get_inode(&TMP_DATA->inode_table, inode_num))->file_type != DIRECTORY_T)
    {
        errno = EBADF;
        return -1;
    }

    for (size_t i = 0; i < inode->dir_content_length; i++)
    {
        if (inode->dir_content[i].name == NULL)
        {
            continue;
        }

        if (filler(buf, inode->dir_content[i].name, NULL, 0))
        {
            log_msg("    ERROR tmp_readdir filler:  buffer full");
            return -ENOMEM;
        }
    }

    log_fi(fi);

    return 0;
}

void *tmp_init(struct fuse_conn_info *conn)
{
    log_msg("\ntmp_init()\n");

    inode_t root_inode = {
        .data = NULL,
        .data_len = 0,
        .dir_content = NULL,
        .file_type = DIRECTORY_T,
        .inode = 0,
        .parent_inode = 0,
        .nlink = 0};

    *allocate_inode(&TMP_DATA->inode_table) = root_inode;

    create_self_and_parent_link(0, 0);

    log_conn(conn);
    log_fuse_context(fuse_get_context());

    return TMP_DATA;
}

void tmp_destroy(void *userdata)
{
    log_msg("\ntmp_destroy(userdata=0x%08x)\n", userdata);
    destroy_lookup_table(&TMP_DATA->inode_table);
}

int tmp_access(const char *path, int mask)
{
    int retstat = 0;

    log_msg("\ntmp_access(path=\"%s\", mask=0%o)\n", path, mask);

    return 0;
}

struct fuse_operations tmp_oper = {
    .getattr = tmp_getattr,   // ok
    .getdir = NULL,           // ok
    .mknod = tmp_mknod,       // ok
    .mkdir = tmp_mkdir,       // ok
    .unlink = tmp_unlink,     // ok
    .rmdir = tmp_rmdir,       // ok
    .rename = tmp_rename,     // ok
    .link = tmp_link,         // ok
    .truncate = tmp_truncate, // ok
    .open = tmp_open,         // ok
    .read = tmp_read,         // ok
    .write = tmp_write,       // ok
    .release = tmp_release,   // ok
    .opendir = tmp_opendir,   // ok
    .readdir = tmp_readdir,   // ok
    .init = tmp_init,         // ok
    .destroy = tmp_destroy,   // ok
    .access = tmp_access,     // ok
};

void tmp_usage()
{
    fprintf(stderr, "usage:  tmpfs [FUSE and mount options] mountPoint\n");
    abort();
}

int main(int argc, char *argv[])
{
    int fuse_stat;
    struct tmp_state *tmp_data;

    if ((getuid() == 0) || (geteuid() == 0))
    {
        fprintf(stderr, "Running tmpfs as root opens unnacceptable security holes\n");
        return 1;
    }

    fprintf(stderr, "Fuse library version %d.%d\n", FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);

    if ((argc < 2) || (argv[argc - 1][0] == '-'))
    {
        tmp_usage();
    }

    tmp_data = malloc(sizeof(struct tmp_state));
    if (tmp_data == NULL)
    {
        perror("main calloc");
        abort();
    }

    tmp_data->rootdir = realpath(argv[argc - 1], NULL);

    tmp_data->logfile = log_open();
    init_inode_lookup_table(&tmp_data->inode_table);

    fprintf(stderr, "about to call fuse_main\n");
    fuse_stat = fuse_main(argc, argv, &tmp_oper, tmp_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);

    return fuse_stat;
}
