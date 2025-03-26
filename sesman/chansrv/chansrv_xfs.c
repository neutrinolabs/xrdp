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
 * This file implements the interface in chansrv_fuse_fs.h
 */
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "os_calls.h"
#include "log.h"

#include "chansrv_xfs.h"

/*
 * Skip this module if FUSE is not supported. A standards-compliant C
 * translation unit must contain at least one declaration (C99:6.9), and we've
 * fulfilled that requirement by this stage.
 */
#ifdef XRDP_FUSE

#define INODE_TABLE_ALLOCATION_INITIAL     4096
#define INODE_TABLE_ALLOCATION_GRANULARITY 100

/* inum of the delete pending directory */
#define DELETE_PENDING_ID 2

/*
 * A double-linked list of inodes, sorted by inum
 *
 * The elements in the list are sorted in increasing inum order, as this
 * allows a directory enumeration to be easily resumed if elements
 * are removed or added. See xfs_readdir() for details on this.
 */
typedef struct xfs_inode_all XFS_INODE_ALL;
typedef struct xfs_list
{
    XFS_INODE_ALL *begin;
    XFS_INODE_ALL *end;
} XFS_LIST;

/*
 * A complete inode, including the private elements used by the
 * implementation
 */
typedef struct xfs_inode_all
{
    XFS_INODE      pub;                /* Public elements                  */
    /*
     * Directory linkage elements
     *
     * Because we don't support hard-linking, elements can be stored in
     * one and only one directory:-
     */
    struct xfs_inode_all *parent;      /* Parent inode                     */
    struct xfs_inode_all *next;        /* Next entry in parent             */
    struct xfs_inode_all *previous;    /* Previous entry in parent         */
    XFS_LIST             dir;          /* Directory only - children        */
    /*
     * Other private elements
     */
    unsigned int         open_count;   /* Regular files only               */
} XFS_INODE_ALL;


/* the xrdp file system in memory
 *
 * inode_table    allows for O(1) access to any file based on the inum.
 *                Index 0 is unused, so we can use an inode of zero for
 *                an invalid inode, and avoid off-by-one errors index
 *                1 is our '.' directory.
 *                2 is the delete pending directory, where we can place
 *                  inodes with a positive open count which are
 *                  deleted.
 * free_list      List of free inode numbers. Allows for O(1) access to
 *                a free node, provided the free list is not empty.
 */
struct xfs_fs
{
    XFS_INODE_ALL    **inode_table;  /* a table of entries; can grow.        */
    fuse_ino_t        *free_list;    /* Free inodes                          */
    unsigned int inode_count;        /* Current number of inodes             */
    unsigned int free_count;         /* Size of free_list                    */
    unsigned int generation;         /* Changes when an inode is deleted     */
};

/* A directory handle
 *
 * inum           inum of the directory being scanned
 * generation     Generation of the inum we opened
 */
struct xfs_dir_handle
{
    fuse_ino_t  inum;
    tui32       generation;
};


/*  ------------------------------------------------------------------------ */
static int
grow_xfs(struct xfs_fs *xfs, unsigned int extra_inodes)
{
    int result = 0;
    unsigned int new_count = xfs->inode_count + extra_inodes;
    XFS_INODE_ALL **new_table;
    fuse_ino_t *new_free_list;

    new_table = (XFS_INODE_ALL **)
                realloc(xfs->inode_table, new_count * sizeof(new_table[0]));
    if (new_table != NULL)
    {
        unsigned int i;
        for (i = xfs->inode_count ; i < new_count ; ++i)
        {
            new_table[i] = NULL;
        }
        xfs->inode_table = new_table;

        new_free_list = (fuse_ino_t *)
                        realloc(xfs->free_list,
                                new_count * sizeof(new_free_list[0]));
        if (new_free_list)
        {
            /* Add the new inodes in to the new_free_list, so the lowest
             * number is allocated first
             */
            i = new_count;
            while (i > xfs->inode_count)
            {
                new_free_list[xfs->free_count++] = --i;
            }

            xfs->free_list = new_free_list;
            xfs->inode_count = new_count;

            result = 1;
        }
    }

    return result;
}

/*  ------------------------------------------------------------------------ */
static void
add_inode_to_list(XFS_LIST *list, XFS_INODE_ALL *xino)
{
    fuse_ino_t inum = xino->pub.inum;

    /* Find the element we need to insert after */
    XFS_INODE_ALL *predecessor = list->end;
    while (predecessor != NULL && predecessor->pub.inum > inum)
    {
        predecessor = predecessor->previous;
    }

    if (predecessor == NULL)
    {
        /* Inserting at the beginning */
        /* Set up links in node */
        xino->next = list->begin;
        xino->previous = NULL;

        /* Set up back-link to node */
        if (list->begin == NULL)
        {
            /* We are the last node */
            list->end = xino;
        }
        else
        {
            list->begin->previous = xino;
        }
        /* Set up forward-link to node */
        list->begin = xino;
    }
    else
    {
        /* Set up links in node */
        xino->next = predecessor->next;
        xino->previous = predecessor;

        /* Set up back-link to node */
        if (predecessor->next == NULL)
        {
            list->end = xino;
        }
        else
        {
            predecessor->next->previous = xino;
        }
        /* Set up forward-link to node */
        predecessor->next = xino;
    }
}

/*  ------------------------------------------------------------------------ */
static void
remove_inode_from_list(XFS_LIST *list, XFS_INODE_ALL *xino)
{
    if (xino->previous == NULL)
    {
        /* First element */
        list->begin = xino->next;
    }
    else
    {
        xino->previous->next = xino->next;
    }

    if (xino->next == NULL)
    {
        /* Last element */
        list->end = xino->previous;
    }
    else
    {
        xino->next->previous = xino->previous;
    }

}

/*  ------------------------------------------------------------------------ */
static void
link_inode_into_directory_node(XFS_INODE_ALL *dinode, XFS_INODE_ALL *xino)
{
    xino->parent = dinode;
    add_inode_to_list(&dinode->dir, xino);
}

/*  ------------------------------------------------------------------------ */
static void
unlink_inode_from_parent(XFS_INODE_ALL *xino)
{
    remove_inode_from_list(&xino->parent->dir, xino);

    xino->next = NULL;
    xino->previous = NULL;
    xino->parent = NULL;
}

/*  ------------------------------------------------------------------------ */
struct xfs_fs *
xfs_create_xfs_fs(mode_t umask, uid_t uid, gid_t gid)
{
    struct xfs_fs *xfs = g_new0(struct xfs_fs, 1);
    XFS_INODE_ALL *xino1 = NULL;
    XFS_INODE_ALL *xino2 = NULL;
    char *xino1_name = NULL;
    char *xino2_name = NULL;

    if (xfs != NULL)
    {
        xfs->inode_count = 0;
        xfs->free_count = 0;
        xfs->inode_table = NULL;
        xfs->free_list   = NULL;
        xfs->generation = 1;

        /* xfs->inode_table check should be superfluous here, but it
         * prevents cppcheck 2.2/2.3 generating a false positive nullPointer
         * report */
        if (!grow_xfs(xfs, INODE_TABLE_ALLOCATION_INITIAL) ||
                xfs->inode_table == NULL ||
                (xino1 = g_new0(XFS_INODE_ALL, 1)) == NULL ||
                (xino2 = g_new0(XFS_INODE_ALL, 1)) == NULL ||
                (xino1_name = strdup(".")) == NULL ||
                (xino2_name = strdup(".delete-pending")) == NULL ||
                // This keeps Coverity happy (see below)
                xfs->free_count < 3)
        {
            free(xino1);
            free(xino2);
            free(xino1_name);
            free(xino2_name);
            xfs_delete_xfs_fs(xfs);
            xfs = NULL;
        }
        else
        {
            /*
             * The use of grow_xfs to allocate the inode table will make
             * inodes 0,  1 (FUSE_ROOT_ID) and 2 (DELETE_PENDING_ID) the first
             * available free inodes. We can ignore these */
            xfs->free_count -= 3;

            xfs->inode_table[0] = NULL;
            xfs->inode_table[FUSE_ROOT_ID] = xino1;
            xfs->inode_table[DELETE_PENDING_ID] = xino2;

            xino1->pub.inum = FUSE_ROOT_ID;
            xino1->pub.mode = (S_IFDIR | 0777) & ~umask;
            xino1->pub.uid = uid;
            xino1->pub.gid = gid;
            xino1->pub.size = 0;
            xino1->pub.atime = time(0);
            xino1->pub.mtime = xino1->pub.atime;
            xino1->pub.ctime = xino1->pub.atime;
            xino1->pub.name = xino1_name;
            xino1->pub.generation = xfs->generation;
            xino1->pub.is_redirected = 0;
            xino1->pub.device_id = 0;

            /*
             * FUSE_ROOT_ID has no parent rather than being a parent
             * of itself. This is intentional */
            xino1->parent = NULL;
            xino1->next = NULL;
            xino1->previous = NULL;
            xino1->dir.begin = NULL;
            xino1->dir.end = NULL;

            xino2->pub.inum = DELETE_PENDING_ID;
            xino2->pub.mode = (S_IFDIR | 0777) & ~umask;
            xino2->pub.uid = uid;
            xino2->pub.gid = gid;
            xino2->pub.size = 0;
            xino2->pub.atime = time(0);
            xino2->pub.mtime = xino2->pub.atime;
            xino2->pub.ctime = xino2->pub.atime;
            xino2->pub.name = xino2_name;
            xino2->pub.generation = xfs->generation;
            xino2->pub.is_redirected = 0;
            xino2->pub.device_id = 0;

            xino2->parent = NULL;
            xino2->next = NULL;
            xino2->previous = NULL;
            xino2->dir.begin = NULL;
            xino2->dir.end = NULL;
            /*
             * Uncomment this line to make the .delete-pending
             * directory visible to the user in the root
             */
            /* link_inode_into_directory_node(xino1, xino2); */
        }
    }

    return xfs;
}

/*  ------------------------------------------------------------------------ */
static void
xfs_free_inode(XFS_INODE_ALL *xino)
{
    if (xino != NULL)
    {
        free(xino->pub.name);
        free(xino);
    }
}

/*  ------------------------------------------------------------------------ */
void
xfs_delete_xfs_fs(struct xfs_fs *xfs)
{
    if (xfs == NULL)
    {
        return;
    }

    if (xfs->inode_table != NULL)
    {
        size_t i;
        for (i = 0 ; i < xfs->inode_count; ++i)
        {
            xfs_free_inode(xfs->inode_table[i]);
        }
    }
    free(xfs->inode_table);
    free(xfs->free_list);
    free(xfs);
}

/*  ------------------------------------------------------------------------ */
XFS_INODE *
xfs_add_entry(struct xfs_fs *xfs, fuse_ino_t parent_inum,
              const char *name, mode_t mode)
{
    XFS_INODE *result = NULL;
    XFS_INODE_ALL *parent = NULL;

    /* Checks:-
     * 1) the parent exists (and is a directory)
     * 2) the caller is not inserting into the .delete-pending directory,
     * 3) Name's not too long
     * 4) Entry does not already exist
     */
    if (parent_inum < xfs->inode_count &&
            ((parent = xfs->inode_table[parent_inum]) != NULL) &&
            (parent->pub.mode & S_IFDIR) != 0 &&
            parent_inum != DELETE_PENDING_ID &&
            strlen(name) <= XFS_MAXFILENAMELEN &&
            !xfs_lookup_in_dir(xfs, parent_inum, name))
    {
        /* Sanitise the mode so one-and-only-one of S_IFDIR and
         * S_IFREG is set */
        if ((mode & S_IFDIR) != 0)
        {
            mode = (mode & 0777) | S_IFDIR;
        }
        else
        {
            mode = (mode & 0777) | S_IFREG;
        }

        /* Space for a new entry? */
        if (xfs->free_count > 0 ||
                grow_xfs(xfs, INODE_TABLE_ALLOCATION_GRANULARITY))
        {
            XFS_INODE_ALL *xino = g_new0(XFS_INODE_ALL, 1);
            char *cpyname = strdup(name);
            if (xino == NULL || cpyname == NULL)
            {
                free(xino);
                free(cpyname);
            }
            else
            {
                fuse_ino_t inum = xfs->free_list[--xfs->free_count];
                if (xfs->inode_table[inum] != NULL)
                {
                    LOG_DEVEL(LOG_LEVEL_ERROR, "Unexpected non-NULL value in inode table "
                              "entry %ld", inum);
                }
                xfs->inode_table[inum] = xino;
                xino->pub.inum = inum;
                xino->pub.mode = mode;
                xino->pub.uid = parent->pub.uid;
                xino->pub.gid = parent->pub.gid;
                if (mode & S_IFDIR)
                {
                    xino->pub.size = 4096;
                }
                else
                {
                    xino->pub.size = 0;
                }
                xino->pub.atime = time(0);
                xino->pub.mtime = xino->pub.atime;
                xino->pub.ctime = xino->pub.atime;
                xino->pub.name = cpyname;
                xino->pub.generation = xfs->generation;
                xino->pub.is_redirected = parent->pub.is_redirected;
                xino->pub.device_id = parent->pub.device_id;
                xino->pub.lindex = 0;

                xino->parent = NULL;
                xino->next = NULL;
                xino->previous = NULL;
                link_inode_into_directory_node(parent, xino);
                result = &xino->pub;
            }
        }
    }

    return result;
}

/*  ------------------------------------------------------------------------ */
void
xfs_remove_directory_contents(struct xfs_fs *xfs, fuse_ino_t inum)
{
    XFS_INODE_ALL *xino =  NULL;

    if (inum < xfs->inode_count &&
            ((xino = xfs->inode_table[inum]) != NULL) &&
            ((xino->pub.mode & S_IFDIR) != 0))
    {
        XFS_INODE_ALL *e;
        while ((e = xino->dir.end) != NULL)
        {
            xfs_remove_entry(xfs, e->pub.inum);
        }
    }
}

/*  ------------------------------------------------------------------------ */
void
xfs_remove_entry(struct xfs_fs *xfs, fuse_ino_t inum)
{
    XFS_INODE_ALL *xino =  NULL;

    if (inum < xfs->inode_count &&
            ((xino = xfs->inode_table[inum]) != NULL))
    {
        if ((xino->pub.mode & S_IFDIR) != 0)
        {
            xfs_remove_directory_contents(xfs, inum);
        }

        unlink_inode_from_parent(xino);
        if ((xino->pub.mode & S_IFREG) != 0 && xino->open_count > 0)
        {
            link_inode_into_directory_node(
                xfs->inode_table[DELETE_PENDING_ID], xino);
        }
        else
        {
            xfs->free_list[xfs->free_count++] = inum;
            xfs->inode_table[inum] = NULL;
            /*
             * Bump the generation when we return an inum to the free list,
             * so that the caller can distinguish re-uses of the same inum.
             */
            ++xfs->generation;
            xfs_free_inode(xino);
        }
    }
}

/*  ------------------------------------------------------------------------ */
XFS_INODE *
xfs_get(struct xfs_fs *xfs, fuse_ino_t inum)
{
    return (inum < xfs->inode_count) ? &xfs->inode_table[inum]->pub : NULL;
}

/*  ------------------------------------------------------------------------ */
char *
xfs_get_full_path(struct xfs_fs *xfs, fuse_ino_t inum)
{
    char *result = NULL;
    XFS_INODE_ALL *xino =  NULL;

    if (inum < xfs->inode_count &&
            ((xino = xfs->inode_table[inum]) != NULL))
    {
        if (xino->pub.inum == FUSE_ROOT_ID)
        {
            return strdup("/");
        }
        else
        {
            /*
             * Add up the lengths of all the names up to the root,
             * allowing one extra char for a '/' prefix for each element
             */
            size_t len = 0;
            XFS_INODE_ALL *p;
            for (p = xino ; p && p->pub.inum != FUSE_ROOT_ID ; p = p->parent)
            {
                len += strlen(p->pub.name);
                ++len; /* Allow for '/' prefix */
            }

            result = (char *) malloc(len + 1);
            if (result != NULL)
            {
                /* Construct the path from the end */
                char *end = result + len;
                *end = '\0';

                for (p = xino ; p && p->pub.inum != FUSE_ROOT_ID ; p = p->parent)
                {
                    len = strlen(p->pub.name);
                    end -= (len + 1);
                    *end = '/';
                    memcpy(end + 1, p->pub.name, len);
                }
            }
        }
    }

    return result;
}

/*  ------------------------------------------------------------------------ */
XFS_INODE *
xfs_lookup_in_dir(struct xfs_fs *xfs, fuse_ino_t inum, const char *name)
{
    XFS_INODE_ALL *xino;
    XFS_INODE *result = NULL;
    if (inum < xfs->inode_count &&
            ((xino = xfs->inode_table[inum]) != NULL) &&
            (xino->pub.mode & S_IFDIR) != 0)
    {
        XFS_INODE_ALL *p;
        for (p = xino->dir.begin ; p != NULL; p = p->next)
        {
            if (strcmp(p->pub.name, name) == 0)
            {
                result = &p->pub;
                break;
            }
        }
    }

    return result;
}

/*  ------------------------------------------------------------------------ */
int
xfs_is_dir_empty(struct xfs_fs *xfs, fuse_ino_t inum)
{
    XFS_INODE_ALL *xino =  NULL;
    int result = 0;

    if (inum < xfs->inode_count &&
            ((xino = xfs->inode_table[inum]) != NULL) &&
            (xino->pub.mode & S_IFDIR) != 0)
    {
        result = (xino->dir.begin == NULL);
    }

    return result;
}

/*  ------------------------------------------------------------------------ */
unsigned int
xfs_is_under(struct xfs_fs *xfs, fuse_ino_t dir, fuse_ino_t entry)
{
    unsigned int result = 0;

    XFS_INODE_ALL *dxino =  NULL;
    XFS_INODE_ALL *exino =  NULL;

    if (dir < xfs->inode_count &&
            ((dxino = xfs->inode_table[dir]) != NULL) &&
            (dxino->pub.mode & S_IFDIR) != 0 &&
            entry < xfs->inode_count &&
            ((exino = xfs->inode_table[entry]) != NULL))
    {
        unsigned int count = 0;

        while (exino != NULL && exino != dxino)
        {
            ++count;
            exino = exino->parent;
        }

        if (exino != NULL)
        {
            result = count;
        }
    }

    return result;
}

/*  ------------------------------------------------------------------------ */
struct xfs_dir_handle *
xfs_opendir(struct xfs_fs *xfs, fuse_ino_t dir)
{
    XFS_INODE_ALL *xino =  NULL;
    struct xfs_dir_handle *result = NULL;

    if (dir < xfs->inode_count &&
            ((xino = xfs->inode_table[dir]) != NULL) &&
            (xino->pub.mode & S_IFDIR) != 0)
    {
        result = g_new0(struct xfs_dir_handle, 1);
        if (result)
        {
            result->inum = xino->pub.inum;
            result->generation = xino->pub.generation;
        }
    }

    return result;
}

/*  ------------------------------------------------------------------------ */
XFS_INODE *
xfs_readdir(struct xfs_fs *xfs, struct xfs_dir_handle *handle, off_t *off)
{
    XFS_INODE_ALL *result = NULL;
    XFS_INODE_ALL *dxino = NULL;
    XFS_INODE_ALL *xino = NULL;

    /* Check the directory is still valid */
    if (handle->inum < xfs->inode_count &&
            ((dxino = xfs->inode_table[handle->inum]) != NULL) &&
            (dxino->pub.mode & S_IFDIR) != 0 &&
            handle->generation == dxino->pub.generation)
    {
        fuse_ino_t inum;

        if (*off == (off_t) -1)
        {
            /* We're at the end already */
        }
        else if ((inum = *off) == 0)
        {
            /* First call */
            result = dxino->dir.begin;
        }
        else if (inum < xfs->inode_count &&
                 (xino = xfs->inode_table[inum]) != 0 &&
                 xino->parent == dxino)
        {
            /* The node we're pointing to is still valid */
            result = xino;
        }
        else
        {
            /*
             * The file we wanted has been pulled out from under us.
             * We will look forward in the inode table to try to
             * discover the next inode in the directory. Because
             * files are stored in inode order, this guarantees
             * we'll meet POSIX requirements.
             */
            for (inum = inum + 1 ; inum < xfs->inode_count ; ++inum)
            {
                if ((xino = xfs->inode_table[inum]) != 0 &&
                        xino->parent == dxino)
                {
                    result = xino;
                    break;
                }
            }
        }
    }

    /* Update the offset */
    if (result == NULL || result->next == NULL)
    {
        /* We're done */
        *off = (off_t) -1;
    }
    else
    {
        *off = (off_t)result->next->pub.inum;
    }

    /* Caller only sees public interface to the result */
    return (result) ? &result->pub : NULL;
}

/*  ------------------------------------------------------------------------ */
void
xfs_closedir(struct xfs_fs *xfs, struct xfs_dir_handle *handle)
{
    free(handle);
}

/*  ------------------------------------------------------------------------ */
void
xfs_increment_file_open_count(struct xfs_fs *xfs, fuse_ino_t inum)
{
    XFS_INODE_ALL *xino;
    if (inum < xfs->inode_count &&
            ((xino = xfs->inode_table[inum]) != NULL) &&
            (xino->pub.mode & S_IFREG) != 0)
    {
        ++xino->open_count;
    }
}

/*  ------------------------------------------------------------------------ */
void
xfs_decrement_file_open_count(struct xfs_fs *xfs, fuse_ino_t inum)
{
    XFS_INODE_ALL *xino;
    if (inum < xfs->inode_count &&
            ((xino = xfs->inode_table[inum]) != NULL) &&
            (xino->pub.mode & S_IFREG) != 0)
    {
        if (xino->open_count > 0)
        {
            --xino->open_count;
        }

        if (xino->open_count == 0 &&
                xino->parent == xfs->inode_table[DELETE_PENDING_ID])
        {
            /* We can get rid of this one now */
            xfs_remove_entry(xfs, inum);
        }
    }
}

/*  ------------------------------------------------------------------------ */
unsigned int
xfs_get_file_open_count(struct xfs_fs *xfs, fuse_ino_t inum)
{
    unsigned int result = 0;
    XFS_INODE_ALL *xino;
    if (inum < xfs->inode_count &&
            ((xino = xfs->inode_table[inum]) != NULL) &&
            (xino->pub.mode & S_IFREG) != 0)
    {
        result = xino->open_count;
    }

    return result;
}

/*  ------------------------------------------------------------------------ */
void
xfs_delete_redirected_entries_with_device_id(struct xfs_fs *xfs,
        tui32 device_id)
{
    fuse_ino_t inum;
    XFS_INODE_ALL *xino;

    /* Using xfs_remove_entry() is convenient, but it recurses
     * in to directories. To make sure all entries are removed, set the
     * open_count of all affected files to 0 first
     */
    for (inum = FUSE_ROOT_ID; inum < xfs->inode_count; ++inum)
    {
        if ((xino = xfs->inode_table[inum]) != NULL &&
                xino->pub.is_redirected != 0 &&
                xino->pub.device_id == device_id &&
                (xino->pub.mode & S_IFREG) != 0)
        {
            xino->open_count = 0;
        }
    }

    /* Now we can be sure everything will be deleted correctly */
    for (inum = FUSE_ROOT_ID; inum < xfs->inode_count; ++inum)
    {
        if ((xino = xfs->inode_table[inum]) != NULL &&
                xino->pub.is_redirected != 0 &&
                xino->pub.device_id == device_id)
        {
            xfs_remove_entry(xfs, xino->pub.inum);
        }
    }
}

/*  ------------------------------------------------------------------------ */
int
xfs_check_move_entry(struct xfs_fs *xfs, fuse_ino_t inum,
                     fuse_ino_t new_parent_inum, const char *name)
{
    XFS_INODE_ALL *xino;
    XFS_INODE_ALL *parent;

    return
        (strlen(name) <= XFS_MAXFILENAMELEN &&
         inum < xfs->inode_count &&
         ((xino = xfs->inode_table[inum]) != NULL) &&
         new_parent_inum != DELETE_PENDING_ID &&
         new_parent_inum < xfs->inode_count &&
         ((parent = xfs->inode_table[new_parent_inum]) != NULL) &&
         (parent->pub.mode & S_IFDIR) != 0 &&
         xfs_is_under(xfs, inum, new_parent_inum) == 0);
}

/*  ------------------------------------------------------------------------ */
int
xfs_move_entry(struct xfs_fs *xfs, fuse_ino_t inum,
               fuse_ino_t new_parent_inum, const char *name)
{
    int result = EINVAL;
    XFS_INODE_ALL *xino;
    XFS_INODE_ALL *parent;
    XFS_INODE *dest;
    char *cpyname;

    /* Copy the new name. We'll either end up freeing it, or we'll
     * use it to replace something else (which gets freed instead) */
    if ((cpyname  = strdup(name)) == NULL)
    {
        result = ENOMEM;
    }
    else if (xfs_check_move_entry(xfs, inum, new_parent_inum, name))
    {
        xino = xfs->inode_table[inum];
        parent = xfs->inode_table[new_parent_inum];

        if (xino->parent != parent)
        {
            /* We're moving between directories */

            /* Does the target name already exist in the destination? */
            if ((dest = xfs_lookup_in_dir(xfs, new_parent_inum, name)) != NULL)
            {
                xfs_remove_entry(xfs, dest->inum);
            }

            unlink_inode_from_parent(xino);
            link_inode_into_directory_node(parent, xino);

            /* Swap the copy name and the inode name so we end up with the
             * right name, and the old one gets freed */
            char *t = xino->pub.name;
            xino->pub.name = cpyname;
            cpyname = t;
        }
        else if (strcmp(xino->pub.name, name) != 0)
        {
            /* Same directory, but name has changed */
            if ((dest = xfs_lookup_in_dir(xfs, new_parent_inum, name)) != NULL)
            {
                /* Name collision - remove destination entry */
                xfs_remove_entry(xfs, dest->inum);
            }

            /* Swap the copy name and the inode name so we end up with the
             * right name, and the old one gets freed */
            char *t = xino->pub.name;
            xino->pub.name = cpyname;
            cpyname = t;
        }
        result = 0;
    }
    free (cpyname);

    return result;
}
#endif  /* XRDP_FUSE */
