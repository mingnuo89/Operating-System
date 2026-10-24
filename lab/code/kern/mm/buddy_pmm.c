/* Standalone Buddy implementation for integration; group C owns this task. */
#include <buddy_pmm.h>
#include <stdio.h>
#define MAX_ORDER 15
#define ALLOCATED (1UL << 2)
static list_entry_t blocks[MAX_ORDER + 1];
static size_t free_count;

static unsigned order_for(size_t n) {
    unsigned order = 0;
    while (order < MAX_ORDER && (1UL << order) < n) order++;
    return order;
}
static void add_block(struct Page *p, unsigned order) {
    p->property = order;
    SetPageProperty(p);
    list_add(&blocks[order], &p->page_link);
}
static void buddy_init(void) {
    for (unsigned i = 0; i <= MAX_ORDER; i++) list_init(&blocks[i]);
    free_count = 0;
}
static void release_block(struct Page *p, unsigned order) {
    while (order < MAX_ORDER) {
        size_t index = p - pages;
        size_t other = index ^ (1UL << order);
        if (other >= npage - nbase) break;
        struct Page *b = pages + other;
        if (!PageProperty(b) || b->property != order) break;
        list_del(&b->page_link);
        ClearPageProperty(b);
        if (b < p) p = b;
        order++;
    }
    add_block(p, order);
}
static void buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    for (size_t i = 0; i < n; i++) {
        assert(PageReserved(base + i));
        base[i].flags = base[i].property = 0;
        set_page_ref(base + i, 0);
    }
    free_count += n;
    while (n) {
        unsigned order = MAX_ORDER;
        size_t index = base - pages;
        while ((1UL << order) > n || (index & ((1UL << order) - 1))) order--;
        release_block(base, order);
        base += 1UL << order;
        n -= 1UL << order;
    }
}
static struct Page *buddy_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > (1UL << MAX_ORDER) || n > free_count) return NULL;
    unsigned order = order_for(n), found = order;
    while (found <= MAX_ORDER && list_empty(&blocks[found])) found++;
    if (found > MAX_ORDER) return NULL;
    struct Page *p = le2page(list_next(&blocks[found]), page_link);
    list_del(&p->page_link);
    ClearPageProperty(p);
    while (found > order) {
        found--;
        add_block(p + (1UL << found), found);
    }
    p->property = order;
    p->flags |= ALLOCATED;
    free_count -= 1UL << order;
    return p;
}
static void buddy_free_pages(struct Page *p, size_t n) {
    assert(n > 0 && n <= (1UL << MAX_ORDER));
    unsigned order = order_for(n);
    assert(p >= pages && p + (1UL << order) <= pages + npage - nbase);
    assert((p->flags & ALLOCATED) && p->property == order);
    p->flags &= ~ALLOCATED;
    free_count += 1UL << order;
    release_block(p, order);
}
static size_t buddy_nr_free_pages(void) { return free_count; }
static void buddy_check(void) {
    size_t before = free_count;
    struct Page *a = alloc_pages(3), *b = alloc_pages(8);
    assert(a && b && a != b);
    assert(((a - pages) & 3) == 0 && ((b - pages) & 7) == 0);
    assert(free_count == before - 12);
    free_pages(a, 3);
    free_pages(b, 8);
    assert(free_count == before);
    struct Page *tiny[32];
    for (unsigned i = 0; i < 32; i++) {
        tiny[i] = alloc_page();
        assert(tiny[i]);
        for (unsigned j = 0; j < i; j++) assert(tiny[i] != tiny[j]);
    }
    for (unsigned i = 0; i < 32; i += 2) free_page(tiny[i]);
    for (unsigned i = 1; i < 32; i += 2) free_page(tiny[i]);
    assert(free_count == before);
    size_t total = 0;
    for (unsigned order = 0; order <= MAX_ORDER; order++) {
        list_entry_t *le = &blocks[order];
        while ((le = list_next(le)) != &blocks[order]) {
            struct Page *p = le2page(le, page_link);
            assert(PageProperty(p) && p->property == order);
            size_t index = p - pages, other = index ^ (1UL << order);
            assert((index & ((1UL << order) - 1)) == 0);
            if (order < MAX_ORDER && other < npage - nbase)
                assert(!PageProperty(pages + other) || pages[other].property != order);
            total += 1UL << order;
        }
    }
    assert(total == before);
    assert(alloc_pages(before + 1) == NULL);
    cprintf("buddy_check() succeeded! (rounding, split, merge, count)\n");
}
const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager", .init = buddy_init,
    .init_memmap = buddy_init_memmap, .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages, .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
