#include "config.h"
#include "params.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h> #include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "log.h"

#include "filesystem.h"
#include "utils.h"
#include "inode_lookup_table.h"
#include "filesystem.h"

int tmp_getattr(const char *path, struct stat *statbuf)
{
    log_msg("\ntmp_getattr(path=\"%s\", statbuf=0x%08x)\n",
            path, statbuf);

    inode_num_t inode_num;
    char *token;
    int search_res = get_inode_by_path(TMP_LOOKUP_TABLE, path, &inode_num, &token);
    log_msg("search res: %d\n", search_res);
    if (search_res != 0)
    {
        return search_res;
    }

    inode_t *inode = get_inode_by_inode_num(TMP_LOOKUP_TABLE, inode_num);

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

int mk_item(enum FILE_TYPE_T type, const char *path, inode_num_t inode_num, char *name)
{
    inode_t new_file_inode_value = {
        .data = NULL,
        .data_len = 0,
        .file_type = type,
        .inode = 0,
        .parent_inode = inode_num,
        .nlink = 0
        // add mode
    };

    inode_t *inode = get_inode_by_inode_num(TMP_LOOKUP_TABLE, inode_num);
    inode_t *new_file_inode = allocate_inode(TMP_LOOKUP_TABLE);
    if (new_file_inode == NULL)
    {
        return -ENOSPC;
    }
    new_file_inode_value.inode = new_file_inode->inode;

    append_new_name_to_inode(inode, new_file_inode->inode, name);

    *new_file_inode = new_file_inode_value;

    if (type == DIRECTORY_T)
    {
        create_self_and_parent_link(TMP_LOOKUP_TABLE, new_file_inode->inode, inode->inode);
    }

    return 0;
}

int tmp_mknod(const char *path, mode_t mode, dev_t dev)
{
    log_msg("\ntmp_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n",
            path, mode, dev);
    inode_num_t inode;
    char *token;
    if (get_inode_by_path(TMP_LOOKUP_TABLE, path, &inode, &token) == 0)
    {
        // file already exist
        return -EEXIST;
    }
    else
    {
        if (is_dir_staff(token))
        {
            return -EINVAL;
        }
        // file not exist
        return mk_item(FILE_T, path, inode, token);
    }
}

/** Create a directory */
int tmp_mkdir(const char *path, mode_t mode)
{
    log_msg("\ntmp_mkdir(path=\"%s\", mode=0%3o)\n",
            path, mode);
    inode_num_t inode;
    char *token;
    if (get_inode_by_path(TMP_LOOKUP_TABLE, path, &inode, &token) == 0)
    {
        // file already exist
        return -EEXIST;
    }
    else
    {
        if (is_dir_staff(token))
        {
            return -EINVAL;
        }
        // file not exist
        return mk_item(DIRECTORY_T, path, inode, token);
    }
}

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
    if (get_inode_by_path(TMP_LOOKUP_TABLE, path, &inode_num, &token) != 0)
    {
        // file not exist
        return -ENOENT;
    }
    else
    {
        if (is_dir_staff(token))
        {
            return -EINVAL;
        }
        inode_t *inode = get_inode_by_inode_num(TMP_LOOKUP_TABLE, get_inode_by_inode_num(TMP_LOOKUP_TABLE, inode_num)->parent_inode);
        remove_name_from_inode(inode, token);
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

    inode_num_t inode_num;
    char *token;
    if (get_inode_by_path(TMP_LOOKUP_TABLE, path, &inode_num, &token) != 0)
    {
        // file not exist
        return -ENOENT;
    }

    inode_num_t new_inode_num;
    char *new_token;
    /*if (get_inode_by_path(TMP_LOOKUP_TABLE, path, &new_inode_num, &new_token) != 0)
    {
        // file not exist
        return -ENOENT;
    }*/

    inode_t *inode = get_inode_by_inode_num(TMP_LOOKUP_TABLE, inode_num);
    inode_t *inode_new = get_inode_by_inode_num(TMP_LOOKUP_TABLE, new_inode_num);

    if (is_dir_staff(token) || is_dir_staff(new_token))
    {
        return -EINVAL;
    }

    // if (inode->file_type == FILE_T && new_inode)

    // if (is_inode_subinode(ino))

    return 0;
}

int tmp_link(const char *path, const char *newpath)
{
    log_msg("\ntmp_link(path=\"%s\", newpath=\"%s\")\n",
            path, newpath);

    inode_num_t inode;
    char *token;
    inode_num_t new_inode;
    char *new_token;
    if (get_inode_by_path(TMP_LOOKUP_TABLE, path, &inode, &token) != 0 ||
        get_inode_by_path(TMP_LOOKUP_TABLE, newpath, &new_inode, &new_token) == 0)
    {
        // file not exist or link already busy
        return -ENOENT;
    }
    else
    {
        inode_t new_file_inode_value = {
            .data = NULL,
            .data_len = 0,
            .file_type = LINK_T,
            .inode = 0,
            .parent_inode = new_inode,
            .nlink = 0
            // add mode
        };

        inode_t *inode = get_inode_by_inode_num(TMP_LOOKUP_TABLE, new_inode);
        inode_t *new_file_inode = allocate_inode(TMP_LOOKUP_TABLE);
        if (new_file_inode == NULL)
        {
            return -ENOSPC;
        }
        new_file_inode_value.inode = new_file_inode->inode;

        append_new_name_to_inode(inode, new_file_inode->inode, new_token);

        *new_file_inode = new_file_inode_value;

        return 0;
    }

    return 0;
}

int tmp_truncate(const char *path, off_t newsize)
{
    inode_num_t inode_num;
    char *token;

    if (get_inode_by_path(TMP_LOOKUP_TABLE, path, &inode_num, &token) != 0)
    {
        return log_error("truncate");
    }

    inode_t *inode = get_inode_by_inode_num(TMP_LOOKUP_TABLE, inode_num);

    inode->data_len = newsize;
    inode->data = realloc(inode->data, newsize);

    if (inode->data == NULL)
    {
        perror("out of memory");
        abort();
    }

    log_msg("\ntmp_truncate(path=\"%s\", newsize=%lld)\n",
            path, newsize);

    return 0;
}

int tmp_open(const char *path, struct fuse_file_info *fi)
{
    inode_num_t inode;
    char *token;

    if (get_inode_by_path(TMP_LOOKUP_TABLE, path, &inode, &token) != 0)
    {
        return log_error("open failed");
    }

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

    inode_t *inode = get_inode_by_inode_num(TMP_LOOKUP_TABLE, fi->fh);

    if (inode == NULL)
    {
        return -EBADF;
    }

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
    log_fi(fi);

    inode_t *inode = get_inode_by_inode_num(TMP_LOOKUP_TABLE, fi->fh);

    if (inode == NULL)
    {
        return -EBADF;
    }

    if (offset + size > inode->data_len)
    {
        inode->data_len = offset + size;
        inode->data = realloc(inode->data, offset + size);
        if (inode->data == NULL)
        {
            perror("out of memory");
            abort();
        }
    }

    memcpy(inode->data + offset, buf, size);
    return size;
}

int tmp_release(const char *path, struct fuse_file_info *fi)
{
    log_msg("\ntmp_release(path=\"%s\", fi=0x%08x)\n",
            path, fi);
    log_fi(fi);

    // reset context with invalid descriptor
    fi->fh = -1;
    return 0;
}

int tmp_opendir(const char *path, struct fuse_file_info *fi)
{
    log_msg("\ntmp_opendir(path=\"%s\", fi=0x%08x)\n",
            path, fi);

    inode_num_t inode;
    char *token;
    if (get_inode_by_path(TMP_LOOKUP_TABLE, path, &inode, &token) != 0)
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
    log_msg("\ntmp_readdir(path=\"%s\", fi=0x%08x)\n",
            path, fi);

    log_fi(fi);

    inode_t *inode = get_inode_by_inode_num(TMP_LOOKUP_TABLE, fi->fh);

    if (inode == NULL)
    {
        return -EBADF;
    }

    for (size_t i = 0; i < get_dir_length(inode); i++)
    {
        if (filler(buf, get_dir_content(inode)[i].name, NULL, 0))
        {
            log_msg("    ERROR tmp_readdir filler:  buffer full");
            return -ENOMEM;
        }
    }

    return 0;
}

void *tmp_init(struct fuse_conn_info *conn)
{
    log_msg("\ntmp_init()\n");

    inode_t root_inode = {
        .data = NULL,
        .data_len = 0,
        .file_type = DIRECTORY_T,
        .inode = 0,
        .parent_inode = 0,
        .nlink = 0};

    *allocate_inode(TMP_LOOKUP_TABLE) = root_inode;

    create_self_and_parent_link(TMP_LOOKUP_TABLE, 0, 0);

    log_conn(conn);
    log_fuse_context(fuse_get_context());

    return TMP_DATA;
}

void tmp_destroy(void *userdata)
{
    log_msg("\ntmp_destroy(userdata=0x%08x)\n", userdata);
    destroy_lookup_table(TMP_LOOKUP_TABLE);
}

int tmp_access(const char *path, int mask)
{
    int retstat = 0;

    log_msg("\ntmp_access(path=\"%s\", mask=0%o)\n", path, mask);

    return 0;
}

struct fuse_operations tmp_oper = {
    .getattr = tmp_getattr,
    .getdir = NULL,
    .mknod = tmp_mknod,
    .mkdir = tmp_mkdir,
    .unlink = tmp_unlink,
    .rmdir = tmp_rmdir,
    .rename = tmp_rename,
    .link = tmp_link,
    .truncate = tmp_truncate,
    .open = tmp_open,
    .read = tmp_read,
    .write = tmp_write,
    .release = tmp_release,
    .opendir = tmp_opendir,
    .readdir = tmp_readdir,
    .init = tmp_init,
    .destroy = tmp_destroy,
    .access = tmp_access,
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
