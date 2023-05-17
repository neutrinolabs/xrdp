/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
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
 * cache
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdp.h"
#include "log.h"



/*****************************************************************************/
static int
xrdp_cache_reset_lru(struct xrdp_cache *self)
{
    int index;
    int jndex;
    struct xrdp_lru_item *lru;

    for (index = 0; index < XRDP_MAX_BITMAP_CACHE_ID; index++)
    {
        /* fist item */
        lru = &(self->bitmap_lrus[index][0]);
        lru->next = 1;
        lru->prev = -1;
        /* middle items */
        for (jndex = 1; jndex < XRDP_MAX_BITMAP_CACHE_IDX - 1; jndex++)
        {
            lru = &(self->bitmap_lrus[index][jndex]);
            lru->next = jndex + 1;
            lru->prev = jndex - 1;
        }
        /* last item */
        lru = &(self->bitmap_lrus[index][XRDP_MAX_BITMAP_CACHE_IDX - 1]);
        lru->next = -1;
        lru->prev = XRDP_MAX_BITMAP_CACHE_IDX - 2;

        self->lru_head[index] = 0;
        self->lru_tail[index] = XRDP_MAX_BITMAP_CACHE_IDX - 1;

        self->lru_reset[index] = 1;
    }
    return 0;
}

/*****************************************************************************/
static int
xrdp_cache_reset_crc(struct xrdp_cache *self)
{
    int index;
    int jndex;

    for (index = 0; index < XRDP_MAX_BITMAP_CACHE_ID; index++)
    {
        for (jndex = 0; jndex < 64 * 1024; jndex++)
        {
            /* it's ok to deinit a zeroed out struct list16 */
            list16_deinit(&(self->crc16[index][jndex]));
            list16_init(&(self->crc16[index][jndex]));
        }
    }
    return 0;
}

/*****************************************************************************/
struct xrdp_cache *
xrdp_cache_create(struct xrdp_wm *owner,
                  struct xrdp_session *session,
                  struct xrdp_client_info *client_info)
{
    struct xrdp_cache *self;

    self = (struct xrdp_cache *)g_malloc(sizeof(struct xrdp_cache), 1);
    self->wm = owner;
    self->session = session;
    self->use_bitmap_comp = client_info->use_bitmap_comp;

    self->cache1_entries = MIN(XRDP_MAX_BITMAP_CACHE_IDX,
                               client_info->cache1_entries);
    self->cache1_entries = MAX(self->cache1_entries, 0);
    self->cache1_size = client_info->cache1_size;

    self->cache2_entries = MIN(XRDP_MAX_BITMAP_CACHE_IDX,
                               client_info->cache2_entries);
    self->cache2_entries = MAX(self->cache2_entries, 0);
    self->cache2_size = client_info->cache2_size;

    self->cache3_entries = MIN(XRDP_MAX_BITMAP_CACHE_IDX,
                               client_info->cache3_entries);
    self->cache3_entries = MAX(self->cache3_entries, 0);
    self->cache3_size = client_info->cache3_size;

    self->bitmap_cache_persist_enable = client_info->bitmap_cache_persist_enable;
    self->bitmap_cache_version = client_info->bitmap_cache_version;
    self->pointer_cache_entries = client_info->pointer_cache_entries;
    self->xrdp_os_del_list = list_create();
    xrdp_cache_reset_lru(self);
    xrdp_cache_reset_crc(self);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_cache_create: 0 %d 1 %d 2 %d",
              self->cache1_entries, self->cache2_entries, self->cache3_entries);
    return self;
}

/*****************************************************************************/
static void
clear_all_cached_items(struct xrdp_cache *self)
{
    int i;
    int j;

    if (self == 0)
    {
        return;
    }

    /* free all the cached bitmaps */
    for (i = 0; i < XRDP_MAX_BITMAP_CACHE_ID; i++)
    {
        for (j = 0; j < XRDP_MAX_BITMAP_CACHE_IDX; j++)
        {
            xrdp_bitmap_delete(self->bitmap_items[i][j].bitmap);
        }
    }

    /* free all the cached font items */
    for (i = 0; i < 12; i++)
    {
        for (j = 0; j < 256; j++)
        {
            g_free(self->char_items[i][j].font_item.data);
        }
    }

    /* free all the off screen bitmaps */
    for (i = 0; i < 2000; i++)
    {
        xrdp_bitmap_delete(self->os_bitmap_items[i].bitmap);
    }

    list_delete(self->xrdp_os_del_list);

    /* free all crc lists */
    for (i = 0; i < XRDP_MAX_BITMAP_CACHE_ID; i++)
    {
        for (j = 0; j < 64 * 1024; j++)
        {
            list16_deinit(&(self->crc16[i][j]));
        }
    }
}

/*****************************************************************************/
void
xrdp_cache_delete(struct xrdp_cache *self)
{
    clear_all_cached_items(self);
    g_free(self);
}

/*****************************************************************************/
int
xrdp_cache_reset(struct xrdp_cache *self,
                 struct xrdp_client_info *client_info)
{
    struct xrdp_wm *wm;
    struct xrdp_session *session;

    /* save these */
    wm = self->wm;
    session = self->session;
    /* De-allocate any allocated memory */
    clear_all_cached_items(self);
    /* set whole struct to zero */
    g_memset(self, 0, sizeof(struct xrdp_cache));
    /* set some stuff back */
    self->wm = wm;
    self->session = session;
    self->use_bitmap_comp = client_info->use_bitmap_comp;
    self->cache1_entries = client_info->cache1_entries;
    self->cache1_size = client_info->cache1_size;
    self->cache2_entries = client_info->cache2_entries;
    self->cache2_size = client_info->cache2_size;
    self->cache3_entries = client_info->cache3_entries;
    self->cache3_size = client_info->cache3_size;
    self->bitmap_cache_persist_enable = client_info->bitmap_cache_persist_enable;
    self->bitmap_cache_version = client_info->bitmap_cache_version;
    self->pointer_cache_entries = client_info->pointer_cache_entries;
    xrdp_cache_reset_lru(self);
    xrdp_cache_reset_crc(self);
    return 0;
}

#define COMPARE_WITH_CRC32(_b1, _b2) \
    ((_b1->crc32 == _b2->crc32) && \
     (_b1->bpp == _b2->bpp) && \
     (_b1->width == _b2->width) && (_b1->height == _b2->height))

/*****************************************************************************/
static int
xrdp_cache_update_lru(struct xrdp_cache *self, int cache_id, int lru_index)
{
    int tail_index;
    struct xrdp_lru_item *nextlru;
    struct xrdp_lru_item *prevlru;
    struct xrdp_lru_item *thislru;
    struct xrdp_lru_item *taillru;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_cache_update_lru: lru_index %d", lru_index);
    if ((lru_index < 0) || (lru_index >= XRDP_MAX_BITMAP_CACHE_IDX))
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_cache_update_lru: error");
        return 1;
    }
    if (self->lru_tail[cache_id] == lru_index)
    {
        /* nothing to do */
        return 0;
    }
    else if (self->lru_head[cache_id] == lru_index)
    {
        /* moving head item to tail */

        thislru = &(self->bitmap_lrus[cache_id][lru_index]);
        nextlru = &(self->bitmap_lrus[cache_id][thislru->next]);
        tail_index = self->lru_tail[cache_id];
        taillru = &(self->bitmap_lrus[cache_id][tail_index]);

        /* unhook old */
        nextlru->prev = -1;

        /* set head to next */
        self->lru_head[cache_id] = thislru->next;

        /* move to tail and hook up */
        taillru->next = lru_index;
        thislru->prev = tail_index;
        thislru->next = -1;

        /* update tail */
        self->lru_tail[cache_id] = lru_index;

    }
    else
    {
        /* move middle item */

        thislru = &(self->bitmap_lrus[cache_id][lru_index]);
        prevlru = &(self->bitmap_lrus[cache_id][thislru->prev]);
        nextlru = &(self->bitmap_lrus[cache_id][thislru->next]);
        tail_index = self->lru_tail[cache_id];
        taillru = &(self->bitmap_lrus[cache_id][tail_index]);

        /* unhook old */
        prevlru->next = thislru->next;
        nextlru->prev = thislru->prev;

        /* move to tail and hook up */
        taillru->next = lru_index;
        thislru->prev = tail_index;
        thislru->next = -1;

        /* update tail */
        self->lru_tail[cache_id] = lru_index;
    }
    return 0;
}

/*****************************************************************************/
/* returns cache id */
int
xrdp_cache_add_bitmap(struct xrdp_cache *self, struct xrdp_bitmap *bitmap,
                      int hints)
{
    int index;
    int jndex;
    int cache_id;
    int cache_idx;
    int bmp_size;
    int e;
    int Bpp;
    int crc16;
    int iig;
    int found;
    int cache_entries;
    int lru_index;
    struct list16 *ll;
    struct xrdp_bitmap *lbm;
    struct xrdp_lru_item *llru;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_cache_add_bitmap:");
    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_cache_add_bitmap: crc16 0x%4.4x",
              bitmap->crc16);

    e = (4 - (bitmap->width % 4)) & 3;
    found = 0;
    cache_id = 0;
    cache_entries = 0;

    /* client Bpp, bmp_size */
    Bpp = (bitmap->bpp + 7) / 8;
    bmp_size = (bitmap->width + e) * bitmap->height * Bpp;
    self->bitmap_stamp++;

    if (bmp_size <= self->cache1_size)
    {
        cache_id = 0;
        cache_entries = self->cache1_entries;
    }
    else if (bmp_size <= self->cache2_size)
    {
        cache_id = 1;
        cache_entries = self->cache2_entries;
    }
    else if (bmp_size <= self->cache3_size)
    {
        cache_id = 2;
        cache_entries = self->cache3_entries;
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "error in xrdp_cache_add_bitmap, "
            "too big(%d) bpp %d", bmp_size, bitmap->bpp);
        return 0;
    }

    crc16 = bitmap->crc16;
    ll = &(self->crc16[cache_id][crc16]);
    for (jndex = 0; jndex < ll->count; jndex++)
    {
        cache_idx = list16_get_item(ll, jndex);
        lbm = self->bitmap_items[cache_id][cache_idx].bitmap;
        if ((lbm != NULL) && COMPARE_WITH_CRC32(lbm, bitmap))
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "found bitmap at %d %d", cache_idx, jndex);
            found = 1;
            break;
        }
    }
    if (found)
    {
        lru_index = self->bitmap_items[cache_id][cache_idx].lru_index;
        self->bitmap_items[cache_id][cache_idx].stamp = self->bitmap_stamp;
        xrdp_bitmap_delete(bitmap);

        /* update lru to end */
        xrdp_cache_update_lru(self, cache_id, lru_index);

        return MAKELONG(cache_idx, cache_id);
    }

    /* find lru */

    /* check for reset */
    if (self->lru_reset[cache_id])
    {
        self->lru_reset[cache_id] = 0;
        LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_cache_add_bitmap: reset detected cache_id %d",
                  cache_id);
        self->lru_tail[cache_id] = cache_entries - 1;
        index = self->lru_tail[cache_id];
        llru = &(self->bitmap_lrus[cache_id][index]);
        llru->next = -1;
    }

    /* lru is item at head */
    lru_index = self->lru_head[cache_id];
    cache_idx = lru_index;

    /* update lru to end */
    xrdp_cache_update_lru(self, cache_id, lru_index);

    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_cache_add_bitmap: oldest %d %d", cache_id, cache_idx);

    LOG_DEVEL(LOG_LEVEL_DEBUG, "adding bitmap at %d %d old ptr %p new ptr %p",
              cache_id, cache_idx,
              self->bitmap_items[cache_id][cache_idx].bitmap,
              bitmap);

    /* remove old, about to be deleted, from crc16 list */
    lbm = self->bitmap_items[cache_id][cache_idx].bitmap;
    if (lbm != 0)
    {
        crc16 = lbm->crc16;
        ll = &(self->crc16[cache_id][crc16]);
        iig = list16_index_of(ll, cache_idx);
        if (iig == -1)
        {
            LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_cache_add_bitmap: error removing cache_idx");
        }
        LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_cache_add_bitmap: removing index %d from crc16 %d",
                  iig, crc16);
        list16_remove_item(ll, iig);
        xrdp_bitmap_delete(lbm);
    }

    /* set, send bitmap and return */

    self->bitmap_items[cache_id][cache_idx].bitmap = bitmap;
    self->bitmap_items[cache_id][cache_idx].stamp = self->bitmap_stamp;
    self->bitmap_items[cache_id][cache_idx].lru_index = lru_index;

    /* add to crc16 list */
    crc16 = bitmap->crc16;
    ll = &(self->crc16[cache_id][crc16]);
    list16_add_item(ll, cache_idx);
    if (ll->count > 1)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_cache_add_bitmap: count %d", ll->count);
    }

    if (self->use_bitmap_comp)
    {
        if (self->bitmap_cache_version & 4)
        {
            if (libxrdp_orders_send_bitmap3(self->session, bitmap->width,
                                            bitmap->height, bitmap->bpp,
                                            bitmap->data, cache_id, cache_idx,
                                            hints) == 0)
            {
                return MAKELONG(cache_idx, cache_id);
            }
        }

        if (self->bitmap_cache_version & 2)
        {
            libxrdp_orders_send_bitmap2(self->session, bitmap->width,
                                        bitmap->height, bitmap->bpp,
                                        bitmap->data, cache_id, cache_idx,
                                        hints);
        }
        else if (self->bitmap_cache_version & 1)
        {
            libxrdp_orders_send_bitmap(self->session, bitmap->width,
                                       bitmap->height, bitmap->bpp,
                                       bitmap->data, cache_id, cache_idx);
        }
    }
    else
    {
        if (self->bitmap_cache_version & 2)
        {
            libxrdp_orders_send_raw_bitmap2(self->session, bitmap->width,
                                            bitmap->height, bitmap->bpp,
                                            bitmap->data, cache_id, cache_idx);
        }
        else if (self->bitmap_cache_version & 1)
        {
            libxrdp_orders_send_raw_bitmap(self->session, bitmap->width,
                                           bitmap->height, bitmap->bpp,
                                           bitmap->data, cache_id, cache_idx);
        }
    }

    return MAKELONG(cache_idx, cache_id);
}

/*****************************************************************************/
/* not used */
/* not sure how to use a palette in rdp */
int
xrdp_cache_add_palette(struct xrdp_cache *self, int *palette)
{
    int i;
    int oldest;
    int index;

    if (self == 0)
    {
        return 0;
    }

    if (palette == 0)
    {
        return 0;
    }

    if (self->wm->screen->bpp > 8)
    {
        return 0;
    }

    self->palette_stamp++;

    /* look for match */
    for (i = 0; i < 6; i++)
    {
        if (g_memcmp(palette, self->palette_items[i].palette,
                     256 * sizeof(int)) == 0)
        {
            self->palette_items[i].stamp = self->palette_stamp;
            return i;
        }
    }

    /* look for oldest */
    index = 0;
    oldest = 0x7fffffff;

    for (i = 0; i < 6; i++)
    {
        if (self->palette_items[i].stamp < oldest)
        {
            oldest = self->palette_items[i].stamp;
            index = i;
        }
    }

    /* set, send palette and return */
    g_memcpy(self->palette_items[index].palette, palette, 256 * sizeof(int));
    self->palette_items[index].stamp = self->palette_stamp;
    libxrdp_orders_send_palette(self->session, palette, index);
    return index;
}

/*****************************************************************************/
int
xrdp_cache_add_char(struct xrdp_cache *self,
                    struct xrdp_font_char *font_item)
{
    int i;
    int j;
    int oldest;
    int f;
    int c;
    int datasize;
    struct xrdp_font_char *fi;

    self->char_stamp++;

    /* look for match */
    for (i = 7; i < 12; i++)
    {
        for (j = 0; j < 250; j++)
        {
            if (xrdp_font_item_compare(&self->char_items[i][j].font_item, font_item))
            {
                self->char_items[i][j].stamp = self->char_stamp;
                LOG_DEVEL(LOG_LEVEL_TRACE, "found font at %d %d", i, j);
                return MAKELONG(j, i);
            }
        }
    }

    /* look for oldest */
    f = 0;
    c = 0;
    oldest = 0x7fffffff;

    for (i = 7; i < 12; i++)
    {
        for (j = 0; j < 250; j++)
        {
            if (self->char_items[i][j].stamp < oldest)
            {
                oldest = self->char_items[i][j].stamp;
                f = i;
                c = j;
            }
        }
    }

    LOG_DEVEL(LOG_LEVEL_TRACE, "adding char at %d %d", f, c);
    /* set, send char and return */
    fi = &self->char_items[f][c].font_item;
    g_free(fi->data);
    datasize = FONT_DATASIZE(font_item);
    fi->data = (char *)g_malloc(datasize, 1);
    g_memcpy(fi->data, font_item->data, datasize);
    fi->offset = font_item->offset;
    fi->baseline = font_item->baseline;
    fi->width = font_item->width;
    fi->height = font_item->height;
    self->char_items[f][c].stamp = self->char_stamp;
    libxrdp_orders_send_font(self->session, fi, f, c);
    return MAKELONG(c, f);
}

/*****************************************************************************/
/* added the pointer to the cache and send it to client, it also sets the
   client if it finds it
   returns the index in the cache
   does not take ownership of pointer_item */
int
xrdp_cache_add_pointer(struct xrdp_cache *self,
                       struct xrdp_pointer_item *pointer_item)
{
    int i;
    int oldest;
    int index;

    if (self == 0)
    {
        return 0;
    }

    self->pointer_stamp++;

    /* look for match */
    for (i = 2; i < self->pointer_cache_entries; i++)
    {
        if (self->pointer_items[i].x == pointer_item->x &&
                self->pointer_items[i].y == pointer_item->y &&
                g_memcmp(self->pointer_items[i].data,
                         pointer_item->data, 96 * 96 * 4) == 0 &&
                g_memcmp(self->pointer_items[i].mask,
                         pointer_item->mask, 96 * 96 / 8) == 0 &&
                self->pointer_items[i].bpp == pointer_item->bpp &&
                self->pointer_items[i].width == pointer_item->width &&
                self->pointer_items[i].height == pointer_item->height)
        {
            self->pointer_items[i].stamp = self->pointer_stamp;
            xrdp_wm_set_pointer(self->wm, i);
            self->wm->current_pointer = i;
            LOG_DEVEL(LOG_LEVEL_TRACE, "found pointer at %d", i);
            return i;
        }
    }

    /* look for oldest */
    index = 2;
    oldest = 0x7fffffff;

    for (i = 2; i < self->pointer_cache_entries; i++)
    {
        if (self->pointer_items[i].stamp < oldest)
        {
            oldest = self->pointer_items[i].stamp;
            index = i;
        }
    }

    self->pointer_items[index].x = pointer_item->x;
    self->pointer_items[index].y = pointer_item->y;
    g_memcpy(self->pointer_items[index].data,
             pointer_item->data, 96 * 96 * 4);
    g_memcpy(self->pointer_items[index].mask,
             pointer_item->mask, 96 * 96 / 8);
    self->pointer_items[index].stamp = self->pointer_stamp;
    self->pointer_items[index].bpp = pointer_item->bpp;
    self->pointer_items[index].width = pointer_item->width;
    self->pointer_items[index].height = pointer_item->height;
    xrdp_wm_send_pointer(self->wm, index,
                         self->pointer_items[index].data,
                         self->pointer_items[index].mask,
                         self->pointer_items[index].x,
                         self->pointer_items[index].y,
                         self->pointer_items[index].bpp,
                         self->pointer_items[index].width,
                         self->pointer_items[index].height);
    self->wm->current_pointer = index;
    LOG_DEVEL(LOG_LEVEL_TRACE, "adding pointer at %d", index);
    return index;
}

/*****************************************************************************/
/* this does not take ownership of pointer_item, it makes a copy */
int
xrdp_cache_add_pointer_static(struct xrdp_cache *self,
                              struct xrdp_pointer_item *pointer_item,
                              int index)
{

    if (self == 0)
    {
        return 0;
    }

    self->pointer_items[index].x = pointer_item->x;
    self->pointer_items[index].y = pointer_item->y;
    g_memcpy(self->pointer_items[index].data,
             pointer_item->data, 96 * 96 * 4);
    g_memcpy(self->pointer_items[index].mask,
             pointer_item->mask, 96 * 96 / 8);
    self->pointer_items[index].stamp = self->pointer_stamp;
    self->pointer_items[index].bpp = pointer_item->bpp;
    xrdp_wm_send_pointer(self->wm, index,
                         self->pointer_items[index].data,
                         self->pointer_items[index].mask,
                         self->pointer_items[index].x,
                         self->pointer_items[index].y,
                         self->pointer_items[index].bpp,
                         self->pointer_items[index].width,
                         self->pointer_items[index].height);
    self->wm->current_pointer = index;
    LOG_DEVEL(LOG_LEVEL_TRACE, "adding pointer at %d", index);
    return index;
}

/*****************************************************************************/
/* this does not take ownership of brush_item_data, it makes a copy */
int
xrdp_cache_add_brush(struct xrdp_cache *self,
                     char *brush_item_data)
{
    int i;
    int oldest;
    int index;

    if (self == 0)
    {
        return 0;
    }

    self->brush_stamp++;

    /* look for match */
    for (i = 0; i < 64; i++)
    {
        if (g_memcmp(self->brush_items[i].pattern,
                     brush_item_data, 8) == 0)
        {
            self->brush_items[i].stamp = self->brush_stamp;
            LOG_DEVEL(LOG_LEVEL_TRACE, "found brush at %d", i);
            return i;
        }
    }

    /* look for oldest */
    index = 0;
    oldest = 0x7fffffff;

    for (i = 0; i < 64; i++)
    {
        if (self->brush_items[i].stamp < oldest)
        {
            oldest = self->brush_items[i].stamp;
            index = i;
        }
    }

    g_memcpy(self->brush_items[index].pattern,
             brush_item_data, 8);
    self->brush_items[index].stamp = self->brush_stamp;
    libxrdp_orders_send_brush(self->session, 8, 8, 1, 0x81, 8,
                              self->brush_items[index].pattern, index);
    LOG_DEVEL(LOG_LEVEL_TRACE, "adding brush at %d", index);
    return index;
}

/*****************************************************************************/
/* returns error */
int
xrdp_cache_add_os_bitmap(struct xrdp_cache *self, struct xrdp_bitmap *bitmap,
                         int rdpindex)
{
    struct xrdp_os_bitmap_item *bi;

    if ((rdpindex < 0) || (rdpindex >= 2000))
    {
        return 1;
    }

    bi = self->os_bitmap_items + rdpindex;
    bi->bitmap = bitmap;
    return 0;
}

/*****************************************************************************/
/* returns error */
int
xrdp_cache_remove_os_bitmap(struct xrdp_cache *self, int rdpindex)
{
    struct xrdp_os_bitmap_item *bi;
    int index;

    if ((rdpindex < 0) || (rdpindex >= 2000))
    {
        return 1;
    }

    bi = self->os_bitmap_items + rdpindex;

    if (bi->bitmap->tab_stop)
    {
        index = list_index_of(self->xrdp_os_del_list, rdpindex);

        if (index == -1)
        {
            list_add_item(self->xrdp_os_del_list, rdpindex);
        }
    }

    xrdp_bitmap_delete(bi->bitmap);
    g_memset(bi, 0, sizeof(struct xrdp_os_bitmap_item));
    return 0;
}

/*****************************************************************************/
struct xrdp_os_bitmap_item *
xrdp_cache_get_os_bitmap(struct xrdp_cache *self, int rdpindex)
{
    struct xrdp_os_bitmap_item *bi;

    if ((rdpindex < 0) || (rdpindex >= 2000))
    {
        return 0;
    }

    bi = self->os_bitmap_items + rdpindex;
    return bi;
}
