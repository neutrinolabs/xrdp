/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This file is the interface to the FUSE file system used by
 * chansrv
 */

#ifndef _CHANSRV_XFS
#define _CHANSRV_XFS

/* Skip this include if there's no FUSE */
#ifdef XRDP_FUSE

#include <stddef.h>
#include <fuse_lowlevel.h>
#include <time.h>

#include "arch.h"

#define XFS_MAXFILENAMELEN 255

/*
 * Incomplete types for the public interface
 */
struct xfs_fs;
struct xfs_dir_handle;

typedef struct xfs_inode
{
    fuse_ino_t      inum;              /* File serial number.               */
    mode_t          mode;              /* File mode.                        */
    uid_t           uid;               /* User ID of the file's owner.      */
    gid_t           gid;               /* Group ID of the file's group.     */
    off_t           size;              /* Size of file, in bytes.           */
    time_t          atime;             /* Time of last access.              */
    time_t          mtime;             /* Time of last modification.        */
    time_t          ctime;             /* Time of last status change.       */
    char            name[XFS_MAXFILENAMELEN + 1]; /* Short name             */
    tui32           generation;        /* Changes if inode is reused        */
    char            is_redirected;     /* file is on redirected device      */
    tui32           device_id;         /* device ID of redirected device    */
    int             lindex;            /* used in clipboard operations      */
} XFS_INODE;

/*
 * Create a new filesystem instance
 *
 * @param   umask Umask to apply to initial data structures
 * @param   uid Owner UID for initial root directory
 * @param   gid Owner GID for initial root directory
 * @return  Pointer to instance, or NULL if no memory
 */
struct xfs_fs *
xfs_create_xfs_fs(mode_t umask, uid_t uid, gid_t gid);

/*
 * Delete a filesystem instance
 *
 * @param xfs Filesystem instance
 */
void
xfs_delete_xfs_fs(struct xfs_fs *xfs);

/*
 * Add an entry to the filesystem
 *
 * The returned element has default values inherited from the parent
 *
 * The specified mode is sanitised in that:-
 * - Bits other than the lowest nine permissions bits, the directory
 *   bit (S_IFDIR) and the regular bit (S_IFREG) are cleared.
 * - S_IFREG is cleared if S_IFDIR is set
 * - S_IFREG is set if neither S_IFDIR or S_IFREG is set
 *
 * NULL is returned for one of the following conditions:-
 * - the parent does not exist
 * - the parent is not a directory
 * - the name length exceeds XFS_MAXFILENAMELEN
 * - the entry already exists
 * - memory is exhausted.
 *
 * @param xfs  filesystem instance
 * @param parent_inode parent inode
 * @param name   Name of entry
 * @param mode   Initial mode for file.
 * @return inode, or NULL
 */
XFS_INODE *
xfs_add_entry(struct xfs_fs *xfs, fuse_ino_t parent_inum,
              const char *name, mode_t mode);

/*
 * Delete the contents of a directory
 *
 * If normal files are opened when they are deleted, the inode is not
 * released until the open count goes to zero.
 *
 * @param xfs  filesystem instance
 * @param inode Reference to entry to delete
 *
 */
void
xfs_remove_directory_contents(struct xfs_fs *xfs, fuse_ino_t inum);

/*
 * Delete an entry from the filesystem
 *
 * If normal files are opened when they are deleted, the inode is not
 * released until the open count goes to zero.
 *
 * For directories, the contents are removed first.
 *
 * @param xfs  filesystem instance
 * @param inode Reference to entry to delete
 *
 */
void
xfs_remove_entry(struct xfs_fs *xfs, fuse_ino_t inum);

/*
 * Get an XFS_INODE for an inum
 *
 * @param xfs  filesystem instance
 * @param inum Inumber
 * @return Pointer to XFS_INODE
 */
XFS_INODE *
xfs_get(struct xfs_fs *xfs, fuse_ino_t inum);

/*
 * Get the full path for an inum
 *
 * The path is dynamically allocated, and must be freed after use
 *
 * @param xfs  filesystem instance
 * @param inum Inumber to get path for
 * @return Full path (free after use)
 */
char *
xfs_get_full_path(struct xfs_fs *xfs, fuse_ino_t inum);

/*
 * Lookup a file in a directory
 *
 * @param xfs  filesystem instance
 * @param inum Inumber of the directory
 * @param name Name of the file to lookup
 * @return Pointer to XFS_INODE if found
 */
XFS_INODE *
xfs_lookup_in_dir(struct xfs_fs *xfs, fuse_ino_t inum, const char *name);

/*
 * Inquires as to whether a directory is empty.
 *
 * The caller must check that the inum is actually a directory, or
 * the result is undefined.
 *
 * @param xfs  filesystem instance
 * @param inum Inumber of the directory
 * @return True if the directory is empty
 */
int
xfs_is_dir_empty(struct xfs_fs *xfs, fuse_ino_t inum);

/*
 * Inquires as to whether an entry is under a directory.
 *
 * This can be used to check for invalid renames, where we try to
 * rename a directory into one of its sub-directories.
 *
 * Returned value means one of the following:-
 * 0   Entry is not related to the directory
 * 1   Entry is an immediate member of the directory
 * 2.. Entry is a few levels below the directory
 *
 * @param xfs  filesystem instance
 * @param dir  Inumber of the directory
 * @param entry Inumber of the entry
 * @return Nesting count of entry in the directory, or 0 for
 *         no nesting
 */
unsigned int
xfs_is_under(struct xfs_fs *xfs, fuse_ino_t dir, fuse_ino_t entry);

/*
 * Opens a directory for reading
 *
 * @param xfs  filesystem instance
 * @param dir  Inumber of the directory
 * @return handle to be passed to xfs_readdir() and xfs_closedir()
 */
struct xfs_dir_handle *
xfs_opendir(struct xfs_fs *xfs, fuse_ino_t dir);

/*
 * Gets the next entry from a directory
 *
 * This function is safe to call while the filesystem is being modified.
 * Whether files added or removed from the directory in question are
 * returned or not is unspecified by this interface.
 *
 * @param xfs  filesystem instance
 * @param handle Handle from xfs_opendir
 * @param off    Offset (by reference). Pass in zero to get the first
 *               entry. After the call, the offset is updated so that
 *               the next call gets the next entry.
 *
 * @return pointer to details of file entry, or NULL if no more
 *         entries are available.
 */
XFS_INODE *
xfs_readdir(struct xfs_fs *xfs, struct xfs_dir_handle *handle, off_t *off);


/*
 * Closes a directory opened for reading
 *
 * @param xfs  filesystem instance
 * @param handle Earlier result of readdir() call
 */
void
xfs_closedir(struct xfs_fs *xfs, struct xfs_dir_handle *handle);

/*
 * Increment the file open count
 *
 * The file open count is used to be sure when an entry can be deleted from
 * the data structures. It allows us to remove an entry while still retaining
 * enough state to manage the file
 *
 * This call is only necessary for regular files - not directories
 *
 * @param xfs  filesystem instance
 * @param inum File to increment the count of
 */
void
xfs_increment_file_open_count(struct xfs_fs *xfs, fuse_ino_t inum);

/*
 * Decrement the file open count
 *
 * This call will ensure that deleted inodes are cleared up at the appropriate
 * time.
 *
 * This call is only necessary for regular files - not directories
 *
 * @param xfs  filesystem instance
 * @param inum File to decrement the count of
 */
void
xfs_decrement_file_open_count(struct xfs_fs *xfs, fuse_ino_t inum);

/*
 * Return the file open count for a regular file
 */
unsigned int
xfs_get_file_open_count(struct xfs_fs *xfs, fuse_ino_t inum);

/*
 * Deletes all redirected entries with the matching device id
 *
 * Files are deleted even if they are open
 *
 * @param device_id Device ID
 */
void
xfs_delete_redirected_entries_with_device_id(struct xfs_fs *xfs,
        tui32 device_id);

/*
 * Check an entry move will be successful
 *
 * A move will fail if:-
 * - Any of the parameters are invalid
 * - if the entry is a directory, and the new parent is below the
 *   entry in the existing hierarchy.
 *
 * @param xfs  filesystem instance
 * @param inum Inumber of entry
 * @param new_parent New parent
 * @param name New name
 *
 * @result != 0 if all looks OK
 */
int
xfs_check_move_entry(struct xfs_fs *xfs, fuse_ino_t inum,
                     fuse_ino_t new_parent_inum, const char *name);


/*
 * Move (rename) an entry
 *
 * @param xfs  filesystem instance
 * @param inum Inumber of entry
 * @param new_parent New parent
 * @param name New name
 *
 * @result 0, or a suitable errno value.
 */
int
xfs_move_entry(struct xfs_fs *xfs, fuse_ino_t inum,
               fuse_ino_t new_parent_inum, const char *name);

#endif /*  XRDP_FUSE   */
#endif /* _CHANSRV_XFS */
