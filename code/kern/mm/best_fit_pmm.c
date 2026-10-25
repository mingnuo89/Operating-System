#include <pmm.h>
#include <list.h>
#include <best_fit_pmm.h>
#include <stdio.h>

// lab2 function2-2 2410849: Best-Fit physical page allocator (member B)
static free_area_t free_area;

// 空闲链表中的每个节点代表一段连续空闲页，节点对应块首页的 property
// 保存该块页数；nr_free 记录全部空闲块中的页数总和。
#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void
best_fit_init(void) {
    // 初始化空闲块链表及计数；可用物理内存稍后由 best_fit_init_memmap 加入。
    list_init(&free_list);
    nr_free = 0;
}

static void
best_fit_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    // 初始化这段内存的页描述符。刚进入内存管理器的页面应处于保留状态，
    // 这里清除旧标记并归零引用计数，为其作为空闲页做好准备。
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    // 只在连续空闲区间的首页保存块长并设置块首标记。
    base->property = n;
    SetPageProperty(base);
    nr_free += n;

    // 链表按物理地址递增排列。地址有序让释放时可以直接检查前后节点，
    // 并通过页地址相邻关系判断是否能合并连续空闲块。
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *page = le2page(le, page_link);
        if (base < page) {
            list_add_before(le, &(base->page_link));
            return;
        }
    }
    list_add_before(&free_list, &(base->page_link));
}

static struct Page *
best_fit_alloc_pages(size_t n) {
    assert(n > 0);
    // 总空闲页数不足时可以提前失败，无需遍历每个空闲块。
    if (n > nr_free) {
        return NULL;
    }

    struct Page *best = NULL;   // best 是最优块候选
    list_entry_t *le = &free_list;  // le 用于遍历空闲块链表
    // 扫描所有空闲块，寻找长度不小于 n 的最小块。
    // 挑选最接近申请大小的块可减少分配后剩余的空闲页碎片。
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n &&
            (best == NULL || p->property < best->property)) {
            best = p;
            // 完全匹配时余量为零，已经不存在更优候选，可以结束扫描。
            if (p->property == n) {
                break;
            }
        }
    }
    if (best == NULL) {
        return NULL;
    }

    // 保存块首节点的前驱和原块大小，然后从空闲链表摘下整个候选块。
    // 前驱用于在原位置插回拆分后剩余的空闲块，以维持链表地址顺序。
    list_entry_t *prev = list_prev(&(best->page_link));
    size_t block_size = best->property;
    list_del(&(best->page_link));
    if (block_size > n) {
        // 从候选块的起始位置分配 n 页；其余尾部页面仍连续，作为新空闲块。
        struct Page *remainder = best + n;
        remainder->property = block_size - n;
        SetPageProperty(remainder);
        list_add(prev, &(remainder->page_link));
    }
    // 返回的页面不再是空闲块首，因此清除标记并清零块长；
    // 同时从全局计数中扣除本次分配的页数。
    ClearPageProperty(best);
    best->property = 0;
    nr_free -= n;
    return best;
}

static void
best_fit_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    // 确认待释放页不是保留页，也不是已有空闲块的块首，避免重复释放；
    // 重置每页标志、块长与引用计数，使页面回到初始空闲状态。
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;

    // 将新释放的区间插入地址有序链表，le 最终指向第一个地址大于 base
    // 的节点；若不存在这样的节点，则指向链表头，表示插入末尾。
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *page = le2page(le, page_link);
        if (base < page) {
            break;
        }
    }
    list_add_before(le, &(base->page_link));

    // 先尝试与前一个空闲块合并：只有前块末尾页的下一页正好是 base，
    // 两个块在物理内存中才相邻。合并后由前块继续作为新块首。
    le = list_prev(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p;
        }
    }

    // 再检查后一个空闲块。此时 base 可能已经指向前向合并后的块首，
    // 因此可继续将其与后块合并，合并操作只保留一个块首链表节点。
    le = list_next(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}

static size_t
best_fit_nr_free_pages(void) {
    // 返回分配器维护的空闲页总数；分配成功和释放时分别扣减、增加该值。
    return nr_free;
}

// Framework tests: students must not modify basic_check or best_fit_check.
static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

static void
best_fit_check(void) {
    int score = 0, sumscore = 6;
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
    #endif
    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
    #endif
    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
    #endif
    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    // * - - * -
    free_pages(p0 + 1, 2);
    free_pages(p0 + 4, 1);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 1) && p0[1].property == 2);
    // * - - * *
    assert((p1 = alloc_pages(1)) != NULL);
    assert(alloc_pages(2) != NULL);
    assert(p0 + 4 == p1);

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
    #endif
    p2 = p0 + 1;
    free_pages(p0, 5);
    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
    #endif
    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count--, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
    #endif
}

const struct pmm_manager best_fit_pmm_manager = {
    .name = "best_fit_pmm_manager",
    .init = best_fit_init,
    .init_memmap = best_fit_init_memmap,
    .alloc_pages = best_fit_alloc_pages,
    .free_pages = best_fit_free_pages,
    .nr_free_pages = best_fit_nr_free_pages,
    .check = best_fit_check,
};
