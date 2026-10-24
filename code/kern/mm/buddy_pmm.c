/* Lab2 task 3 (member C): power-of-two Buddy physical page allocator. */
#include <buddy_pmm.h>
#include <stdio.h>

#define BUDDY_MAX_ORDER 15
#define BUDDY_ALLOCATED (1UL << 2)

static list_entry_t free_lists[BUDDY_MAX_ORDER + 1];
static size_t buddy_free_count;

static size_t
order_size(unsigned order) {
    return 1UL << order;
}

static unsigned
order_for(size_t n) {
    unsigned order = 0;
    while (order < BUDDY_MAX_ORDER && order_size(order) < n) {
        order++;
    }
    return order;
}

static void
add_free_block(struct Page *base, unsigned order) {
    base->property = order;
    base->flags &= ~BUDDY_ALLOCATED;
    SetPageProperty(base);
    list_add(&free_lists[order], &base->page_link);
}

static void
buddy_init(void) {
    for (unsigned order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_init(&free_lists[order]);
    }
    buddy_free_count = 0;
}

/* Insert one aligned block and repeatedly merge it with its free buddy. */
static void
release_block(struct Page *base, unsigned order) {
    while (order < BUDDY_MAX_ORDER) {
        size_t index = base - pages;
        size_t buddy_index = index ^ order_size(order);
        if (buddy_index >= npage - nbase) {
            break;
        }

        struct Page *buddy = pages + buddy_index;
        if (!PageProperty(buddy) || buddy->property != order) {
            break;
        }
        list_del(&buddy->page_link);
        ClearPageProperty(buddy);
        buddy->property = 0;
        if (buddy < base) {
            base = buddy;
        }
        order++;
    }
    add_free_block(base, order);
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    for (size_t i = 0; i < n; i++) {
        assert(PageReserved(base + i));
        base[i].flags = 0;
        base[i].property = 0;
        set_page_ref(base + i, 0);
        list_init(&base[i].page_link);
    }

    buddy_free_count += n;
    while (n > 0) {
        unsigned order = BUDDY_MAX_ORDER;
        size_t index = base - pages;
        while (order_size(order) > n ||
               (index & (order_size(order) - 1)) != 0) {
            order--;
        }
        release_block(base, order);
        base += order_size(order);
        n -= order_size(order);
    }
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > order_size(BUDDY_MAX_ORDER) || n > buddy_free_count) {
        return NULL;
    }

    unsigned requested_order = order_for(n);
    unsigned found_order = requested_order;
    while (found_order <= BUDDY_MAX_ORDER &&
           list_empty(&free_lists[found_order])) {
        found_order++;
    }
    if (found_order > BUDDY_MAX_ORDER) {
        return NULL;
    }

    struct Page *base = le2page(list_next(&free_lists[found_order]),
                                page_link);
    list_del(&base->page_link);
    ClearPageProperty(base);
    base->property = 0;

    while (found_order > requested_order) {
        found_order--;
        add_free_block(base + order_size(found_order), found_order);
    }

    base->property = requested_order;
    base->flags |= BUDDY_ALLOCATED;
    buddy_free_count -= order_size(requested_order);
    return base;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0 && n <= order_size(BUDDY_MAX_ORDER));
    unsigned order = order_for(n);
    size_t allocated_pages = order_size(order);
    assert(base >= pages &&
           base + allocated_pages <= pages + npage - nbase);
    assert((base->flags & BUDDY_ALLOCATED) != 0);
    assert(base->property == order);

    base->flags &= ~BUDDY_ALLOCATED;
    base->property = 0;
    buddy_free_count += allocated_pages;
    release_block(base, order);
}

static size_t
buddy_nr_free_pages(void) {
    return buddy_free_count;
}

/*
 * Check rounding, alignment, splitting, coalescing, uniqueness, accounting,
 * and an impossible request without relying on Buddy's private state outside
 * this module.
 */
static void
buddy_check(void) {
    size_t before = buddy_free_count;

    struct Page *rounded = alloc_pages(3);
    struct Page *exact = alloc_pages(8);
    assert(rounded != NULL && exact != NULL && rounded != exact);
    assert(((rounded - pages) & 3) == 0);
    assert(((exact - pages) & 7) == 0);
    assert(buddy_free_count == before - 12);
    free_pages(rounded, 3);
    free_pages(exact, 8);
    assert(buddy_free_count == before);

    struct Page *single[32];
    for (unsigned i = 0; i < 32; i++) {
        single[i] = alloc_page();
        assert(single[i] != NULL);
        for (unsigned j = 0; j < i; j++) {
            assert(single[i] != single[j]);
        }
    }
    for (unsigned i = 0; i < 32; i += 2) {
        free_page(single[i]);
    }
    for (unsigned i = 1; i < 32; i += 2) {
        free_page(single[i]);
    }
    assert(buddy_free_count == before);

    size_t listed_pages = 0;
    for (unsigned order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_entry_t *le = &free_lists[order];
        while ((le = list_next(le)) != &free_lists[order]) {
            struct Page *block = le2page(le, page_link);
            size_t index = block - pages;
            size_t buddy_index = index ^ order_size(order);
            assert(PageProperty(block) && block->property == order);
            assert((index & (order_size(order) - 1)) == 0);
            if (order < BUDDY_MAX_ORDER &&
                buddy_index < npage - nbase) {
                assert(!PageProperty(pages + buddy_index) ||
                       pages[buddy_index].property != order);
            }
            listed_pages += order_size(order);
        }
    }
    assert(listed_pages == before);
    assert(alloc_pages(before + 1) == NULL);
    assert(buddy_free_count == before);
    cprintf("buddy_check() succeeded! (rounding, split, merge, count)\n");
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
