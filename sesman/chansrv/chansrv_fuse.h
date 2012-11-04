
#if !defined(CHANSRV_FUSE_H)
#define CHANSRV_FUSE_H

int APP_CC
fuse_clear_clip_dir(void);
int APP_CC
fuse_add_clip_dir_item(char *filename, int flags, int size);
int APP_CC
fuse_get_wait_objs(tbus *objs, int *count, int *timeout);
int APP_CC
fuse_check_wait_objs(void);
int APP_CC
fuse_init(void);
int APP_CC
fuse_deinit(void);

#endif
