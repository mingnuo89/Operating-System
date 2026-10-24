/* Lab2 Challenge SLUB (member B): simplified single-core object allocator. */
#include <slub.h>
#include <pmm.h>
#include <string.h>

#define CLASS_COUNT 8
#define SLAB_MAGIC 0x534c55424c414232UL

struct slab {
    uint64_t magic;
    size_t class_index, page_count, in_use, capacity;
    void *free_objects;
    list_entry_t link;
    uint64_t allocated[4];
};

static const size_t sizes[CLASS_COUNT] = {
    16, 32, 64, 128, 256, 512, 1024, 2048,
};
static list_entry_t partial[CLASS_COUNT];
static size_t held_pages;

#define HEADER_SIZE ROUNDUP(sizeof(struct slab), 16)

static void *
page_address(struct Page *p) {
    return (void *)(page2pa(p) + va_pa_offset);
}

void
slub_init(void) {
    for (unsigned i = 0; i < CLASS_COUNT; i++) {
        list_init(&partial[i]);
    }
    held_pages = 0;
}

size_t
slub_pages_in_use(void) {
    return held_pages;
}

static struct slab *
new_slab(size_t index) {
    struct Page *p = alloc_page();
    if (p == NULL) {
        return NULL;
    }
    struct slab *s = page_address(p);
    memset(s, 0, sizeof(*s));
    s->magic = SLAB_MAGIC;
    s->class_index = index;
    s->page_count = 1;
    s->capacity = (PGSIZE - HEADER_SIZE) / sizes[index];
    assert(s->capacity <= 256);
    list_init(&s->link);
    for (size_t i = s->capacity; i > 0; i--) {
        void **obj = (void **)((char *)s + HEADER_SIZE +
                              (i - 1) * sizes[index]);
        *obj = s->free_objects;
        s->free_objects = obj;
    }
    list_add(&partial[index], &s->link);
    held_pages++;
    return s;
}

void *
kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    size_t index = 0;
    while (index < CLASS_COUNT && sizes[index] < size) {
        index++;
    }
    if (index == CLASS_COUNT) {
        if (size > (~(size_t)0) - HEADER_SIZE - PGSIZE + 1) {
            return NULL;
        }
        size_t n = (size + HEADER_SIZE + PGSIZE - 1) / PGSIZE;
        size_t available = nr_free_pages();
        struct Page *p = alloc_pages(n);
        if (p == NULL) {
            return NULL;
        }
        size_t allocated_pages = available - nr_free_pages();
        struct slab *s = page_address(p);
        memset(s, 0, sizeof(*s));
        s->magic = SLAB_MAGIC;
        s->class_index = CLASS_COUNT;
        s->page_count = allocated_pages;
        held_pages += allocated_pages;
        return (char *)s + HEADER_SIZE;
    }

    struct slab *s;
    if (list_empty(&partial[index])) {
        s = new_slab(index);
        if (s == NULL) {
            return NULL;
        }
    }
    else {
        s = to_struct(list_next(&partial[index]), struct slab, link);
    }
    void *obj = s->free_objects;
    s->free_objects = *(void **)obj;
    size_t slot = ((char *)obj - (char *)s - HEADER_SIZE) / sizes[index];
    s->allocated[slot / 64] |= 1UL << (slot % 64);
    s->in_use++;
    if (s->in_use == s->capacity) {
        list_del_init(&s->link);
    }
    return obj;
}

void
kfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    struct slab *s = (struct slab *)ROUNDDOWN((uintptr_t)ptr, PGSIZE);
    assert(s->magic == SLAB_MAGIC);
    if (s->class_index == CLASS_COUNT) {
        assert(ptr == (char *)s + HEADER_SIZE);
        size_t n = s->page_count;
        s->magic = 0;
        held_pages -= n;
        free_pages(pa2page(PADDR(s)), n);
        return;
    }

    size_t index = s->class_index;
    assert(index < CLASS_COUNT);
    uintptr_t offset = (char *)ptr - (char *)s;
    assert(offset >= HEADER_SIZE &&
           (offset - HEADER_SIZE) % sizes[index] == 0);
    size_t slot = (offset - HEADER_SIZE) / sizes[index];
    assert(slot < s->capacity &&
           (s->allocated[slot / 64] & (1UL << (slot % 64))));
    s->allocated[slot / 64] &= ~(1UL << (slot % 64));
    if (s->in_use == s->capacity) {
        list_add(&partial[index], &s->link);
    }
    *(void **)ptr = s->free_objects;
    s->free_objects = ptr;
    s->in_use--;
    if (s->in_use == 0) {
        list_del(&s->link);
        s->magic = 0;
        held_pages--;
        free_page(pa2page(PADDR(s)));
    }
}
