/* Lab2 task 3 (member C): power-of-two Buddy physical page allocator. */
#include <buddy_pmm.h>
#include <stdio.h>

#define BUDDY_MAX_ORDER 15
#define BUDDY_ALLOCATED (1UL << 2)

// order 为 k 的块包含 2^k 个连续页面；每种大小分别维护一条空闲链表。
static list_entry_t free_lists[BUDDY_MAX_ORDER + 1];
// 记录所有空闲块覆盖的页面总数，而不是空闲块的个数。
static size_t buddy_free_count;

static size_t
order_size(unsigned order) {
    // 伙伴算法的块大小始终是 2 的幂。
    return 1UL << order;
}

static unsigned
order_for(size_t n) {
    // 将任意正整数向上取整为能够容纳它的最小 2 的幂阶数。
    // 例如申请 3 页会得到 order=2，即实际分配 4 页。
    unsigned order = 0;
    while (order < BUDDY_MAX_ORDER && order_size(order) < n) {
        order++;
    }
    return order;
}

static void
add_free_block(struct Page *base, unsigned order) {
    // 空闲块只在块首页记录阶数并设置 PageProperty 标志，
    // 再根据阶数放入对应链表；清除分配标记表示该块可再次分配。
    base->property = order;
    base->flags &= ~BUDDY_ALLOCATED;
    SetPageProperty(base);
    list_add(&free_lists[order], &base->page_link);
}

static void
buddy_init(void) {
    // 每个阶数各自维护一条空闲链表，初始化时所有链表均为空。
    for (unsigned order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_init(&free_lists[order]);
    }
    // 实际可用页数会在 buddy_init_memmap 中累计。
    buddy_free_count = 0;
}

// 插入一个块，并在可能时不断与同阶的空闲伙伴合并，直至无法继续合并。
static void
release_block(struct Page *base, unsigned order) {
    while (order < BUDDY_MAX_ORDER) {
        // 伙伴块的页索引只在当前阶对应的那一位上不同，
        // 因此用异或可以在 O(1) 时间从块首索引算出伙伴块首索引。
        size_t index = base - pages;
        size_t buddy_index = index ^ order_size(order);
        // 伙伴地址超出当前管理的物理页范围时，无法与之合并。
        if (buddy_index >= npage - nbase) {
            break;
        }

        struct Page *buddy = pages + buddy_index;
        // 只有伙伴也处于空闲状态且阶数完全相同，才能合并为更高一阶。
        if (!PageProperty(buddy) || buddy->property != order) {
            break;
        }
        // 从原阶数链表摘除伙伴，并清除其块首元数据；
        // 合并后只需要较低地址的页面作为新块首。
        list_del(&buddy->page_link);
        ClearPageProperty(buddy);
        buddy->property = 0;
        if (buddy < base) {
            base = buddy;
        }
        // 两个同阶伙伴合并后块大小翻倍，阶数随之增加一。
        order++;
    }
    // 循环结束后，将最终合并得到的块登记到对应阶数的链表中。
    add_free_block(base, order);
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    // 清理新加入内存区域中每页的状态，并初始化链表节点，
    // 使这些页面可以安全地成为不同大小空闲块的组成部分。
    for (size_t i = 0; i < n; i++) {
        assert(PageReserved(base + i));
        base[i].flags = 0;
        base[i].property = 0;
        set_page_ref(base + i, 0);
        list_init(&base[i].page_link);
    }

    buddy_free_count += n;
    // 将区域切分为若干满足对齐要求的最大 2 的幂块。
    // 每轮同时受剩余页数和当前起始索引对齐约束，避免生成未对齐块。
    while (n > 0) {
        unsigned order = BUDDY_MAX_ORDER;
        size_t index = base - pages;
        while (order_size(order) > n ||
               (index & (order_size(order) - 1)) != 0) {
            order--;
        }
        // 登记当前块；release_block 还会尝试与此前加入的同阶伙伴合并。
        release_block(base, order);
        base += order_size(order);
        n -= order_size(order);
    }
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    // 超过最大可表示块或当前空闲页总量的请求必然无法满足。
    if (n > order_size(BUDDY_MAX_ORDER) || n > buddy_free_count) {
        return NULL;
    }

    // 将申请量向上取整为目标阶数，然后从目标阶开始向更大的阶数查找，
    // 找到第一个非空链表即得到能够满足请求的最小可用块阶数。
    unsigned requested_order = order_for(n);
    unsigned found_order = requested_order;
    while (found_order <= BUDDY_MAX_ORDER &&
           list_empty(&free_lists[found_order])) {
        found_order++;
    }
    if (found_order > BUDDY_MAX_ORDER) {
        return NULL;
    }

    // 取出找到的空闲块，并先清除其空闲块首状态；之后可能将它逐级拆分。
    struct Page *base = le2page(list_next(&free_lists[found_order]),
                                page_link);
    list_del(&base->page_link);
    ClearPageProperty(base);
    base->property = 0;

    // 每降一阶便将当前块二分：保留低地址的一半继续拆分，
    // 将高地址的一半作为伙伴块放回对应阶数的空闲链表。
    while (found_order > requested_order) {
        found_order--;
        add_free_block(base + order_size(found_order), found_order);
    }

    // 返回的块大小为 2^requested_order；标记块首已分配，
    // 空闲计数按实际占用的整块页数扣减，而非按原始请求 n 扣减。
    base->property = requested_order;
    base->flags |= BUDDY_ALLOCATED;
    buddy_free_count -= order_size(requested_order);
    return base;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0 && n <= order_size(BUDDY_MAX_ORDER));
    // 分配时按 2 的幂向上取整，因此释放时也根据原始请求 n 还原阶数，
    // 并以该阶实际块大小进行范围、分配状态及元数据校验。
    unsigned order = order_for(n);
    size_t allocated_pages = order_size(order);
    assert(base >= pages &&
           base + allocated_pages <= pages + npage - nbase);
    assert((base->flags & BUDDY_ALLOCATED) != 0);
    assert(base->property == order);

    // 清除分配标记并先将整块计入空闲页数，再交给统一合并逻辑；
    // 合并只改变块的组织方式，不改变空闲页总数。
    base->flags &= ~BUDDY_ALLOCATED;
    base->property = 0;
    buddy_free_count += allocated_pages;
    release_block(base, order);
}

static size_t
buddy_nr_free_pages(void) {
    // 返回分配器维护的空闲页面总数。
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
