#ifndef LAB2_SLUB_H
#define LAB2_SLUB_H

#include <defs.h>

void slub_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
size_t slub_pages_in_use(void);
void slub_check(void);

#endif
