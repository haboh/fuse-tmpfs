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

#include "filesystem.h"

void
create_self_and_parent_link
(
    dir_content_t **dir_content
    , size_t *dir_content_length
    , inode_num_t self
    , inode_num_t parent
)
{
    *dir_content = malloc(2 * sizeof(dir_content_t));
    *dir_content_length = 2;
    (*dir_content)->inode = self;
    (*dir_content)->name = ".";

    ((*dir_content) + 1)->inode = parent;
    ((*dir_content) + 1)->name = "..";
}

//  All the paths I see are relative to the root of the mounted
//  filesystem.  In order to get to the underlying filesystem, I need to
//  have the mountpoint.  I'll save it away early on in main(), and then
//  whenever I need a path for something I'll call this to construct
//  it.
static void bb_fullpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, BB_DATA->rootdir);
    strncat(fpath, path, PATH_MAX); // ridiculously long paths will
				    // break here

    log_msg("    bb_fullpath:  rootdir = \"%s\", path = \"%s\", fpath = \"%s\"\n",
	    BB_DATA->rootdir, path, fpath);
}


int
get_inode_by_name(const char *name, inode_num_t *inode) {
    for (size_t i = 0; i < BB_DATA->inode_lookup_table[*inode].dir_content_length; i++) {
        char *filename = BB_DATA->inode_lookup_table[*inode].dir_content[i].name;
        if (filename != NULL && strcmp(filename, name) == 0) {
            *inode = BB_DATA->inode_lookup_table[*inode].dir_content[i].inode;
            return 0;
        }
    }
    return -1;
}

int
get_inode_num(const char *path, inode_num_t *inode, char **token) {
    char* path_copy = malloc(strlen(path) + 1);
    strcpy(path_copy, path);
    *token = strtok(path_copy, "/");
    *inode = 0;
    while (*token) {
        if (get_inode_by_name(*token, inode) != 0) {
            return -ENOENT;
        }
        char *new_token = strtok(NULL, "/");
        if (new_token == NULL) {
            break;
        }
        *token = new_token;
    }
    return 0;
}

int bb_getattr(const char *path, struct stat *statbuf)
{
    log_msg("\nbb_getattr(path=\"%s\", statbuf=0x%08x)\n",
	  path, statbuf);

    inode_num_t inode;
    char *token;
    int search_res = get_inode_num(path, &inode, &token);
    log_msg("search res: %d\n", search_res);
    if (search_res != 0) {
        return search_res;
    }

    mode_t st_mode = S_IRWXU | S_IRWXG | S_IRWXO; // change
    if (BB_DATA->inode_lookup_table[inode].file_type == FILE_T) {
        st_mode = S_IFREG;
    } else {
        st_mode = S_IFDIR;
    }

    struct stat new_stat_buf = {
        .st_ino = inode,
        .st_mode = st_mode,
        .st_nlink = BB_DATA->inode_lookup_table[inode].nlink,
        .st_size = BB_DATA->inode_lookup_table[inode].data_len,
        .st_dev = 0,
        .st_uid = 0, // change
        .st_gid = 0, // change
        .st_rdev = 0,
        .st_blksize = 0,
        .st_blocks = 0,
        .st_atime = 0,
        .st_mtime = 0,
        .st_ctime = 0 
    };

    *statbuf = new_stat_buf; 
    log_stat(statbuf);
    
    return 0;
}

int mk_item(enum FILE_TYPE_T type, const char *path, inode_num_t inode, char *token) {
    log_msg("file not exist %s\n", path);

    inode_t new_file_inode = {
        .data = NULL,
        .data_len = 0,
        .dir_content = NULL,
        .file_type = type,
        .num = 0,
        .parent_inode = inode,
        .nlink = 0
        // add mode
    };

    inode_num_t new_file_inode_num = ++BB_DATA->last_inode;

    if (new_file_inode_num == INODE_COUNT) {
        return -ENOSPC;
    }

    log_msg("inode: %d\n", inode);
    log_msg("length: %d\n", BB_DATA->inode_lookup_table[inode].dir_content_length);
    // change
    BB_DATA->inode_lookup_table[inode].dir_content = realloc(
        BB_DATA->inode_lookup_table[inode].dir_content
        , sizeof(dir_content_t) * (BB_DATA->inode_lookup_table[inode].dir_content_length + 1)
    );

    int index = BB_DATA->inode_lookup_table[inode].dir_content_length++;

    BB_DATA->inode_lookup_table[inode].dir_content[index].inode = new_file_inode_num;
    log_msg("%s\n", token);
    log_msg("%d\n", strlen(token));
    BB_DATA->inode_lookup_table[inode].dir_content[index].name = token;

    log_msg("%s\n", BB_DATA->inode_lookup_table[inode].dir_content[index].name);

    BB_DATA->inode_lookup_table[new_file_inode_num] = new_file_inode;
    
    log_msg("length: %d\n", BB_DATA->inode_lookup_table[inode].dir_content_length);
    for (size_t i = 0; i < BB_DATA->inode_lookup_table[inode].dir_content_length; i++) {
        if (BB_DATA->inode_lookup_table[inode].dir_content[i].name != NULL) {
            log_msg("chld inode: %d, chld name: %s\n", 
                BB_DATA->inode_lookup_table[inode].dir_content[i].inode, 
                BB_DATA->inode_lookup_table[inode].dir_content[i].name);
        }
   }
    return 0;
}

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
// shouldn't that comment be "if" there is no.... ?
int bb_mknod(const char *path, mode_t mode, dev_t dev)
{
    log_msg("\nbb_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n",
	  path, mode, dev);
    inode_num_t inode;
    char *token;
    if (get_inode_num(path, &inode, &token) == 0) {
        // file already exist
        return -EEXIST;
    } else {
        // file not exist

       return mk_item(FILE_T, path, inode, token);
    }
}

/** Create a directory */
int bb_mkdir(const char *path, mode_t mode)
{
    inode_num_t inode;
    char *token;
    if (get_inode_num(path, &inode, &token) == 0) {
        // file already exist
        return -EEXIST;
    } else {
        // file not exist
        return mk_item(DIRECTORY_T, path, inode, token);
   }
}

/** Remove a file */
int bb_unlink(const char *path)
{
    char fpath[PATH_MAX];
    
    log_msg("bb_unlink(path=\"%s\")\n",
	    path);

    // TODO

    return 0;
}

/** Remove a directory */
int bb_rmdir(const char *path)
{
    char fpath[PATH_MAX];
    
    inode_num_t inode;
    char *token;
    if (get_inode_num(path, &inode, &token) != 0) {
        // file already exist
        return -ENOENT;
    } else {
        log_msg("token: %s\n", token);
        for (size_t i = 0; i < BB_DATA->inode_lookup_table[BB_DATA->inode_lookup_table[inode].parent_inode].dir_content_length; i++) {
            const char *name = BB_DATA->inode_lookup_table[BB_DATA->inode_lookup_table[inode].parent_inode].dir_content[i].name;
            if (name != NULL && strcmp(name, token) == 0) {
                BB_DATA->inode_lookup_table[BB_DATA->inode_lookup_table[inode].parent_inode].dir_content[i].name = NULL;
            }
        }
    }

    log_msg("bb_rmdir(path=\"%s\")\n",
	    path);

    return 0;
}

/** Rename a file */
// both path and newpath are fs-relative
int bb_rename(const char *path, const char *newpath)
{
    log_msg("\nbb_rename(fpath=\"%s\", newpath=\"%s\")\n",
	    path, newpath);

    // TODO

    return 0;
}

/** Create a hard link to a file */
int bb_link(const char *path, const char *newpath)
{
    log_msg("\nbb_link(path=\"%s\", newpath=\"%s\")\n",
	    path, newpath);
    // TODO
    return 0;
}

/** Change the size of a file */
int bb_truncate(const char *path, off_t newsize)
{
    inode_num_t inode;
    char *token;

    if (get_inode_num(path, &inode, &token) != 0) {
        return log_error("truncate");
    }

    BB_DATA->inode_lookup_table[inode].data_len = newsize;
    BB_DATA->inode_lookup_table[inode].data = realloc(BB_DATA->inode_lookup_table[inode].data, newsize);

    log_msg("\nbb_truncate(path=\"%s\", newsize=%lld)\n",
	    path, newsize);

    return 0;
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int bb_open(const char *path, struct fuse_file_info *fi)
{
    inode_num_t inode;
    char *token;

    if (get_inode_num(path, &inode, &token) != 0) {
        return log_error("open");
    }

    // if the open call succeeds, my retstat is the file descriptor,
    // else it's -errno.  I'm making sure that in that case the saved
    // file descriptor is exactly -1.
	
    fi->fh = inode;

    log_fi(fi);
    
    return 0;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
// I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the fusexmp code which returns the amount of data also
// returned by read.
int bb_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nbb_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);

    inode_num_t inode;
    char *token;
    if (get_inode_num(path, &inode, &token) != 0) {
        return 0;
    }

    size_t bytes_read_count = offset + size <= BB_DATA->inode_lookup_table[inode].data_len ?
        size : BB_DATA->inode_lookup_table[inode].data_len - offset;

    memcpy(buf, BB_DATA->inode_lookup_table[inode].data, bytes_read_count);

    return bytes_read_count;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
// As  with read(), the documentation above is inconsistent with the
// documentation for the write() system call.
int bb_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nbb_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi
	    );
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);

    inode_num_t inode;
    char *token;
    if (get_inode_num(path, &inode, &token) != 0) {
        return 0;
    }
    
    if (offset + size > BB_DATA->inode_lookup_table[inode].data_len) {
        BB_DATA->inode_lookup_table[inode].data_len = offset + size;
        BB_DATA->inode_lookup_table[inode].data = realloc(BB_DATA->inode_lookup_table[inode].data, offset + size);
    }
 
    memcpy(BB_DATA->inode_lookup_table[inode].data + offset, buf, size);
    return size;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int bb_release(const char *path, struct fuse_file_info *fi)
{
    log_msg("\nbb_release(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    log_fi(fi);

    return 0;
}

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int bb_opendir(const char *path, struct fuse_file_info *fi)
{
    log_msg("\nbb_opendir(path=\"%s\", fi=0x%08x)\n",
	  path, fi);

    inode_num_t inode;
    char *token;
    if (get_inode_num(path, &inode, &token) != 0) {
        return log_error("bb_opendir opendir");
    }

    fi->fh = inode;

    log_fi(fi);
    return 0;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */

int bb_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{

    inode_num_t inode;
    char *token;
    if (get_inode_num(path, &inode, &token) != 0 || BB_DATA->inode_lookup_table[inode].file_type != DIRECTORY_T) {
        errno = EBADF;
        return -1;
    }

    for (size_t i = 0; i < BB_DATA->inode_lookup_table[inode].dir_content_length; i++) { 
        if (BB_DATA->inode_lookup_table[inode].dir_content[i].name == NULL) {
            continue;
        }

        if (filler(buf, BB_DATA->inode_lookup_table[inode].dir_content[i].name, NULL, 0)) {
            log_msg("    ERROR bb_readdir filler:  buffer full");
            return -ENOMEM;
        }
    }

    log_fi(fi);

    return 0;
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
// Undocumented but extraordinarily useful fact:  the fuse_context is
// set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to
// fuse_main().  Really seems like either it should be a third
// parameter coming in here, or else the fact should be documented
// (and this might as well return void, as it did in older versions of
// FUSE).


void *bb_init(struct fuse_conn_info *conn)
{
    log_msg("\nbb_init()\n");

    inode_t root_inode = {
        .data = NULL,
        .data_len = 0,
        .dir_content = NULL,
        .file_type = DIRECTORY_T,
        .num = 0,
        .parent_inode = 0,
        .nlink = 0
    };

    create_self_and_parent_link(&root_inode.dir_content, &root_inode.dir_content_length, 0, 0);

    BB_DATA->inode_lookup_table[0] = root_inode;

    log_conn(conn);
    log_fuse_context(fuse_get_context());
    
    return BB_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void bb_destroy(void *userdata)
{
    log_msg("\nbb_destroy(userdata=0x%08x)\n", userdata);
}

/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int bb_access(const char *path, int mask)
{
    int retstat = 0;

    log_msg("\nbb_access(path=\"%s\", mask=0%o)\n",
	    path, mask);

    return 0;
}

struct fuse_operations bb_oper = {
  .getattr = bb_getattr, // ok
  .getdir = NULL, // ok
  .mknod = bb_mknod, // ok
  .mkdir = bb_mkdir, // ok
  .unlink = bb_unlink, // ok
  .rmdir = bb_rmdir, // ok
  .rename = bb_rename, // ok
  .link = bb_link, // ok
  .truncate = bb_truncate, // ok
  .open = bb_open, // ok
  .read = bb_read, // ok
  .write = bb_write, // ok
  .release = bb_release, // ok
  .opendir = bb_opendir, //ok
  .readdir = bb_readdir, // ok
  .init = bb_init, // ok
  .destroy = bb_destroy, // ok
  .access = bb_access, // ok
};

void bb_usage()
{
    fprintf(stderr, "usage:  bbfs [FUSE and mount options] rootDir mountPoint\n");
    abort();
}

int main(int argc, char *argv[])
{
    int fuse_stat;
    struct bb_state *bb_data;

    // bbfs doesn't do any access checking on its own (the comment
    // blocks in fuse.h mention some of the functions that need
    // accesses checked -- but note there are other functions, like
    // chown(), that also need checking!).  Since running bbfs as root
    // will therefore open Metrodome-sized holes in the system
    // security, we'll check if root is trying to mount the filesystem
    // and refuse if it is.  The somewhat smaller hole of an ordinary
    // user doing it with the allow_other flag is still there because
    // I don't want to parse the options string.
    if ((getuid() == 0) || (geteuid() == 0)) {
    	fprintf(stderr, "Running BBFS as root opens unnacceptable security holes\n");
    	return 1;
    }

    // See which version of fuse we're running
    fprintf(stderr, "Fuse library version %d.%d\n", FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
    
    // Perform some sanity checking on the command line:  make sure
    // there are enough arguments, and that neither of the last two
    // start with a hyphen (this will break if you actually have a
    // rootpoint or mountpoint whose name starts with a hyphen, but so
    // will a zillion other programs)
    if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
	bb_usage();

    bb_data = malloc(sizeof(struct bb_state));
    if (bb_data == NULL) {
	perror("main calloc");
	abort();
    }

    // Pull the rootdir out of the argument list and save it in my
    // internal data
    bb_data->rootdir = realpath(argv[argc-2], NULL);
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;
    
    bb_data->logfile = log_open();
    bb_data->last_inode = 0;
    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main\n");
    fuse_stat = fuse_main(argc, argv, &bb_oper, bb_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
    return fuse_stat;
}
