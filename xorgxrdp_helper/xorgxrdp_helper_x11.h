
#ifndef _XORGXRDP_HELPER_X11_H
#define _XORGXRDP_HELPER_X11_H

int
xorgxrdp_helper_x11_init(void);
int
xorgxrdp_helper_x11_get_wait_objs(intptr_t *objs, int *obj_count);
int
xorgxrdp_helper_x11_check_wait_objs(void);
int
xorgxrdp_helper_x11_delete_all_pixmaps(void);
int
xorgxrdp_helper_x11_create_pixmap(int width, int height, int magic,
                                  int con_id, int mon_id);
int
xorgxrdp_helper_x11_encode_pixmap(int width, int height, int mon_id,
                                  int num_crects, struct xh_rect *crects,
                                  void *cdata, int *cdata_bytes);

#endif
