
#if !defined(CHANSRV_FUSE_H)
#define CHANSRV_FUSE_H

struct file_item
{
    char filename[256];
    int flags;
    char data[256];
    int data_bytes;
    int ino;
};

int APP_CC
fuse_set_dir_item(int index, char *filename, int flags, char *data,
                  int data_bytes, int ino);
int APP_CC
fuse_get_wait_objs(tbus *objs, int *count, int *timeout);
int APP_CC
fuse_check_wait_objs(void);
int APP_CC
fuse_init(void);
int APP_CC
fuse_deinit(void);

#endif
