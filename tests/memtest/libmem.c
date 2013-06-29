
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALIGN_BY 1024
#define ALIGN_BY_M1 (ALIGN_BY - 1)
#define ALIGN(_in) (((_in) + ALIGN_BY_M1) & (~ALIGN_BY_M1))

struct mem_item
{
  unsigned int addr;
  int bytes;
  struct mem_item* next;
  struct mem_item* prev;
};

struct mem_info
{
  unsigned int addr;
  int bytes;
  int flags;
  struct mem_item* free_head;
  struct mem_item* free_tail;
  struct mem_item* used_head;
  struct mem_item* used_tail;
};

/*****************************************************************************/
static int
libmem_free_mem_item(struct mem_info* self, struct mem_item* mi)
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
void*
libmem_init(unsigned int addr, int bytes)
{
  struct mem_info* self;
  struct mem_item* mi;

  self = (struct mem_info*)malloc(sizeof(struct mem_info));
  memset(self, 0, sizeof(struct mem_info));
  self->addr = addr;
  self->bytes = bytes;
  //self->flags = 1;
  mi = (struct mem_item*)malloc(sizeof(struct mem_item));
  memset(mi, 0, sizeof(struct mem_item));
  mi->addr = addr;
  mi->bytes = bytes;
  self->free_head = mi;
  self->free_tail = mi;
  return self;
}

/*****************************************************************************/
void
libmem_deinit(void* aself)
{
  struct mem_info* self;

  self = (struct mem_info*)aself;
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
libmem_add_used_item(struct mem_info* self, unsigned int addr, int bytes)
{
  struct mem_item* mi;
  struct mem_item* new_mi;
  int added;

  if (self == 0 || addr == 0)
  {
    return 1;
  }
  if (self->used_head == 0)
  {
    /* add first item */
    new_mi = (struct mem_item*)malloc(sizeof(struct mem_item));
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
      new_mi = (struct mem_item*)malloc(sizeof(struct mem_item));
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
    new_mi = (struct mem_item*)malloc(sizeof(struct mem_item));
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
libmem_add_free_item(struct mem_info* self, unsigned int addr, int bytes)
{
  struct mem_item* mi;
  struct mem_item* new_mi;
  int added;

  if (self == 0 || addr == 0)
  {
    return 1;
  }
  if (self->free_head == 0)
  {
    /* add first item */
    new_mi = (struct mem_item*)malloc(sizeof(struct mem_item));
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
      /* add before */
      new_mi = (struct mem_item*)malloc(sizeof(struct mem_item));
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
    new_mi = (struct mem_item*)malloc(sizeof(struct mem_item));
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
libmem_pack_free(struct mem_info* self)
{
  struct mem_item* mi;
  int cont;

  cont = 1;
  while (cont)
  {
    cont = 0;
    mi = self->free_head;
    while (mi != 0)
    {
      /* combine */
      if (mi->next != 0)
      {
        if (mi->addr + mi->bytes == mi->next->addr)
        {
          mi->bytes += mi->next->bytes;
          cont = 1;
          libmem_free_mem_item(self, mi->next);
        }
      }
      /* remove empties */
      if (mi->bytes == 0)
      {
        cont = 1;
        libmem_free_mem_item(self, mi);
        mi = self->free_head;
        continue;
      }
      mi = mi->next;
    }
  }
  return 0;
}

/*****************************************************************************/
static int
libmem_print(struct mem_info* self)
{
  struct mem_item* mi;

  printf("libmem_print:\n");
  printf("  used_head %p\n", self->used_head);
  printf("  used_tail %p\n", self->used_tail);
  mi = self->used_head;
  if (mi != 0)
  {
    printf("  used list\n");
    while (mi != 0)
    {
      printf("    ptr %p prev %p next %p addr 0x%8.8x bytes %d\n",
             mi, mi->prev, mi->next, mi->addr, mi->bytes);
      mi = mi->next;
    }
  }
  printf("  free_head %p\n", self->free_head);
  printf("  free_tail %p\n", self->free_tail);
  mi = self->free_head;
  if (mi != 0)
  {
    printf("  free list\n");
    while (mi != 0)
    {
      printf("    ptr %p prev %p next %p addr 0x%8.8x bytes %d\n",
             mi, mi->prev, mi->next, mi->addr, mi->bytes);
      mi = mi->next;
    }
  }
  return 0;
}

/*****************************************************************************/
unsigned int
libmem_alloc(void* obj, int bytes)
{
  struct mem_info* self;
  struct mem_item* mi;
  unsigned int addr;

  if (bytes < 1)
  {
    return 0;
  }
  bytes = ALIGN(bytes);
  self = (struct mem_info*)obj;
  addr = 0;
  if (bytes > 16 * 1024)
  {
    /* big blocks */
    mi = self->free_tail;
    while (mi != 0)
    {
      if (bytes <= mi->bytes)
      {
        addr = mi->addr;
        mi->bytes -= bytes;
        mi->addr += bytes;
        break;
      }
      mi = mi->prev;
    }
  }
  else
  {
    /* small blocks */
    mi = self->free_head;
    while (mi != 0)
    {
      if (bytes <= mi->bytes)
      {
        addr = mi->addr;
        mi->bytes -= bytes;
        mi->addr += bytes;
        break;
      }
      mi = mi->next;
    }
  }
  if (addr != 0)
  {
    libmem_add_used_item(self, addr, bytes);
    libmem_pack_free(self);
    if (self->flags & 1)
    {
      libmem_print(self);
    }
  }
  else
  {
    printf("libmem_alloc: error\n");
  }
  return addr;
}

/*****************************************************************************/
int
libmem_free(void* obj, unsigned int addr)
{
  struct mem_info* self;
  struct mem_item* mi;

  if (addr == 0)
  {
    return 0;
  }
  self = (struct mem_info*)obj;
  mi = self->used_head;
  while (mi != 0)
  {
    if (mi->addr == addr)
    {
      libmem_add_free_item(self, mi->addr, mi->bytes);
      libmem_free_mem_item(self, mi);
      libmem_pack_free(self);
      if (self->flags & 1)
      {
        libmem_print(self);
      }
      return 0;
    }
    mi = mi->next;
  }
  printf("libmem_free: error\n");
  return 1;
}

/*****************************************************************************/
int
libmem_set_flags(void* obj, int flags)
{
  struct mem_info* self;

  self = (struct mem_info*)obj;
  self->flags |= flags;
}

/*****************************************************************************/
int
libmem_clear_flags(void* obj, int flags)
{
  struct mem_info* self;

  self = (struct mem_info*)obj;
  self->flags &= ~flags;
}
