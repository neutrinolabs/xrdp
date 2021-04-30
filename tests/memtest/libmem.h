
#ifndef _LIBMEM_C
#define _LIBMEM_C

void *
libmem_init(unsigned int addr, int bytes);
void
libmem_deinit(void *aself);
unsigned int
libmem_alloc(void *obj, int bytes);
int
libmem_free(void *obj, unsigned int addr);
int
libmem_set_flags(void *obj, int flags);
int
libmem_clear_flags(void *obj, int flags);
int
libmem_get_alloced_bytes(void *obj);

#endif
