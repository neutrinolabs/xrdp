
#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

#define ALIGN_BY 32
#define ALIGN_BY_M1 (ALIGN_BY - 1)
#define ALIGN(_in) (((_in) + ALIGN_BY_M1) & (~ALIGN_BY_M1))


struct mem_item
{
    unsigned int addr;
    int bytes;
    struct mem_item *next;
    struct mem_item *prev;
};

struct mem_info
{
    unsigned int addr;
    int bytes;
    int flags;
    struct mem_item *free_head;
    struct mem_item *free_tail;
    struct mem_item *used_head;
    struct mem_item *used_tail;
    int total_bytes;
};

/*****************************************************************************/
static int
libmem_free_mem_item(struct mem_info *self, struct mem_item *mi)
{
    if (self == 0 || mi == 0)
    {
        return 0;
    }
    if (mi->prev != 0)
    {
        mi->prev->next = mi->next;
    }
    if (mi->next != 0)
    {
        mi->next->prev = mi->prev;
    }
    if (mi == self->free_head)
    {
        self->free_head = mi->next;
    }
    if (mi == self->free_tail)
    {
        self->free_tail = mi->prev;
    }
    if (mi == self->used_head)
    {
        self->used_head = mi->next;
    }
    if (mi == self->used_tail)
    {
        self->used_tail = mi->prev;
    }
    free(mi);
    return 0;
}

/*****************************************************************************/
void *
libmem_init(unsigned int addr, int bytes)
{
    struct mem_info *self;
    struct mem_item *mi;

    self = (struct mem_info *)malloc(sizeof(struct mem_info));
    memset(self, 0, sizeof(struct mem_info));
    self->addr = addr;
    self->bytes = bytes;
    //self->flags = 1;
    mi = (struct mem_item *)malloc(sizeof(struct mem_item));
    memset(mi, 0, sizeof(struct mem_item));
    mi->addr = addr;
    mi->bytes = bytes;
    self->free_head = mi;
    self->free_tail = mi;
    return self;
}

/*****************************************************************************/
void
libmem_deinit(void *aself)
{
    struct mem_info *self;

    self = (struct mem_info *)aself;
    if (self == 0)
    {
        return;
    }
    while (self->free_head != 0)
    {
        libmem_free_mem_item(self, self->free_head);
    }
    while (self->used_head != 0)
    {
        libmem_free_mem_item(self, self->used_head);
    }
    free(self);
}

/****************************************************************************/
static int
libmem_add_used_item(struct mem_info *self, unsigned int addr, int bytes)
{
    struct mem_item *mi;
    struct mem_item *new_mi;
    int added;

    if (self == 0 || addr == 0)
    {
        return 1;
    }
    if (self->used_head == 0)
    {
        /* add first item */
        new_mi = (struct mem_item *)malloc(sizeof(struct mem_item));
        memset(new_mi, 0, sizeof(struct mem_item));
        new_mi->addr = addr;
        new_mi->bytes = bytes;
        self->used_head = new_mi;
        self->used_tail = new_mi;
        return 0;
    }
    added = 0;
    mi = self->used_head;
    while (mi != 0)
    {
        if (mi->addr > addr)
        {
            /* add before */
            new_mi = (struct mem_item *)malloc(sizeof(struct mem_item));
            memset(new_mi, 0, sizeof(struct mem_item));
            new_mi->addr = addr;
            new_mi->bytes = bytes;
            new_mi->prev = mi->prev;
            new_mi->next = mi;
            if (mi->prev != 0)
            {
                mi->prev->next = new_mi;
            }
            mi->prev = new_mi;
            if (self->used_head == mi)
            {
                self->used_head = new_mi;
            }
            added = 1;
            break;
        }
        mi = mi->next;
    }
    if (!added)
    {
        /* add last */
        new_mi = (struct mem_item *)malloc(sizeof(struct mem_item));
        memset(new_mi, 0, sizeof(struct mem_item));
        new_mi->addr = addr;
        new_mi->bytes = bytes;
        self->used_tail->next = new_mi;
        new_mi->prev = self->used_tail;
        self->used_tail = new_mi;
    }
    return 0;
}

/****************************************************************************/
static int
libmem_add_free_item(struct mem_info *self, unsigned int addr, int bytes)
{
    struct mem_item *mi;
    struct mem_item *new_mi;
    int added;

    if (self == 0 || addr == 0)
    {
        return 1;
    }
    if (self->free_head == 0)
    {
        /* add first item */
        new_mi = (struct mem_item *)malloc(sizeof(struct mem_item));
        memset(new_mi, 0, sizeof(struct mem_item));
        new_mi->addr = addr;
        new_mi->bytes = bytes;
        self->free_head = new_mi;
        self->free_tail = new_mi;
        return 0;
    }
    added = 0;
    mi = self->free_head;
    while (mi != 0)
    {
        if (mi->addr > addr)
        {
            if (mi->prev != 0)
            {
                if (mi->prev->addr + mi->prev->bytes == addr)
                {
                    /* don't need to add, just make prev bigger */
                    mi->prev->bytes += bytes;
                    if (mi->prev->addr + mi->prev->bytes == mi->addr)
                    {
                        /* here we can remove one */
                        mi->prev->bytes += mi->bytes;
                        libmem_free_mem_item(self, mi);
                    }
                    return 0;
                }
            }
            if (addr + bytes == mi->addr)
            {
                /* don't need to add here either */
                mi->addr = addr;
                mi->bytes += bytes;
                return 0;
            }
            /* add before */
            new_mi = (struct mem_item *)malloc(sizeof(struct mem_item));
            memset(new_mi, 0, sizeof(struct mem_item));
            new_mi->addr = addr;
            new_mi->bytes = bytes;
            new_mi->prev = mi->prev;
            new_mi->next = mi;
            if (mi->prev != 0)
            {
                mi->prev->next = new_mi;
            }
            mi->prev = new_mi;
            if (self->free_head == mi)
            {
                self->free_head = new_mi;
            }
            added = 1;
            break;
        }
        mi = mi->next;
    }
    if (!added)
    {
        /* add last */
        new_mi = (struct mem_item *)malloc(sizeof(struct mem_item));
        memset(new_mi, 0, sizeof(struct mem_item));
        new_mi->addr = addr;
        new_mi->bytes = bytes;
        self->free_tail->next = new_mi;
        new_mi->prev = self->free_tail;
        self->free_tail = new_mi;
    }
    return 0;
}

/*****************************************************************************/
static int
libmem_print(struct mem_info *self)
{
    struct mem_item *mi;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "libmem_print:");
    LOG_DEVEL(LOG_LEVEL_DEBUG, "  used_head %p", self->used_head);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "  used_tail %p", self->used_tail);
    mi = self->used_head;
    if (mi != 0)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "  used list");
        while (mi != 0)
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "    ptr %p prev %p next %p addr 0x%8.8x bytes %d",
                      mi, mi->prev, mi->next, mi->addr, mi->bytes);
            mi = mi->next;
        }
    }
    LOG_DEVEL(LOG_LEVEL_DEBUG, "  free_head %p", self->free_head);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "  free_tail %p", self->free_tail);
    mi = self->free_head;
    if (mi != 0)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "  free list");
        while (mi != 0)
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "    ptr %p prev %p next %p addr 0x%8.8x bytes %d",
                      mi, mi->prev, mi->next, mi->addr, mi->bytes);
            mi = mi->next;
        }
    }
    return 0;
}

/*****************************************************************************/
unsigned int
libmem_alloc(void *obj, int bytes)
{
    struct mem_info *self;
    struct mem_item *mi;
    unsigned int addr;

    if (bytes < 1)
    {
        return 0;
    }
    bytes = ALIGN(bytes);
    self = (struct mem_info *)obj;
    addr = 0;
    mi = self->free_head;
    while (mi != 0)
    {
        if (bytes <= mi->bytes)
        {
            addr = mi->addr;
            mi->bytes -= bytes;
            mi->addr += bytes;
            if (mi->bytes < 1)
            {
                libmem_free_mem_item(self, mi);
            }
            break;
        }
        mi = mi->next;
    }
    if (addr != 0)
    {
        self->total_bytes += bytes;
        libmem_add_used_item(self, addr, bytes);
        if (self->flags & 1)
        {
            libmem_print(self);
        }
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "libmem_alloc: error");
    }
    return addr;
}

/*****************************************************************************/
int
libmem_free(void *obj, unsigned int addr)
{
    struct mem_info *self;
    struct mem_item *mi;

    if (addr == 0)
    {
        return 0;
    }
    self = (struct mem_info *)obj;
    mi = self->used_tail;
    while (mi != 0)
    {
        if (mi->addr == addr)
        {
            self->total_bytes -= mi->bytes;
            libmem_add_free_item(self, mi->addr, mi->bytes);
            libmem_free_mem_item(self, mi);
            if (self->flags & 1)
            {
                libmem_print(self);
            }
            return 0;
        }
        mi = mi->prev;
    }
    LOG_DEVEL(LOG_LEVEL_ERROR, "libmem_free: error");
    return 1;
}

/*****************************************************************************/
int
libmem_set_flags(void *obj, int flags)
{
    struct mem_info *self;

    self = (struct mem_info *)obj;
    self->flags |= flags;
    return 0;
}

/*****************************************************************************/
int
libmem_clear_flags(void *obj, int flags)
{
    struct mem_info *self;

    self = (struct mem_info *)obj;
    self->flags &= ~flags;
    return 0;
}

/*****************************************************************************/
int
libmem_get_alloced_bytes(void *obj)
{
    struct mem_info *self;

    self = (struct mem_info *)obj;
    return self->total_bytes;
}
