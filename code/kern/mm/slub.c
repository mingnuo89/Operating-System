/* Lab2 Challenge SLUB - 2410849 (member B): simplified single-core object allocator. */
#include <slub.h>
#include <pmm.h>
#include <string.h>

#define CLASS_COUNT 8
#define SLAB_MAGIC 0x534c55424c414232UL

// 小对象 slab 的元数据放在其管理页的起始位置；对象区紧随对齐后的头部。
struct slab {
    uint64_t magic;
    // class_index 表示对应哪个档位类别，page_count 表示 slab 占用的页数，
    // in_use 表示当前已分配的对象数，capacity 表示该 slab 可容纳的最大对象数
    size_t class_index, page_count, in_use, capacity;
    void *free_objects;
    list_entry_t link;
    // 每一位对应一个对象槽位：1 表示已分配，0 表示当前空闲。
    uint64_t allocated[4];
};

// 小对象采用固定大小类别，申请尺寸向上匹配到最小可容纳它的类别。
static const size_t sizes[CLASS_COUNT] = {
    16, 32, 64, 128, 256, 512, 1024, 2048,
};
// 每个类别仅将尚有空闲对象的 slab 放在 partial 链表中。
static list_entry_t partial[CLASS_COUNT];
// SLUB 当前持有的物理页数，包含小对象 slab 页和大对象分配页。
static size_t held_pages;

// slab 头部按 16 字节对齐，确保其后的对象槽位具有合适的对齐起点。
#define HEADER_SIZE ROUNDUP(sizeof(struct slab), 16)

static void *
page_address(struct Page *p) {
    // 将物理页描述符转换为内核可访问的直接映射虚拟地址。
    return (void *)(page2pa(p) + va_pa_offset);
}

void
slub_init(void) {
    // 各对象大小类别分别维护 partial 链表，初始化时均为空。
    for (unsigned i = 0; i < CLASS_COUNT; i++) {
        list_init(&partial[i]);
    }
    // 初始化 SLUB 持有页数统计。
    held_pages = 0;
}

size_t
slub_pages_in_use(void) {
    // 向外提供 SLUB 当前占用的物理页数。
    return held_pages;
}

static struct slab *
new_slab(size_t index) {
    // 一个小对象 slab 由一个物理页承载；页分配失败则无法创建该类别的对象。
    struct Page *p = alloc_page();
    if (p == NULL) {
        return NULL;
    }
    // 页首放置 slab 元数据，先清零以初始化空闲链表、计数和分配位图。
    struct slab *s = page_address(p);
    memset(s, 0, sizeof(*s));
    s->magic = SLAB_MAGIC;
    s->class_index = index;
    s->page_count = 1;
    // 对象区由页剩余空间按当前类别大小切分，得到可容纳的槽位数。
    s->capacity = (PGSIZE - HEADER_SIZE) / sizes[index];
    assert(s->capacity <= 256);
    list_init(&s->link);
    // 将所有槽位串成单向空闲链表。倒序构造使链表头最终指向第 0 个槽位。
    for (size_t i = s->capacity; i > 0; i--) {
        void **obj = (void **)((char *)s + HEADER_SIZE +
                              (i - 1) * sizes[index]);
        *obj = s->free_objects;
        s->free_objects = obj;
    }
    // 新 slab 至少有一个空闲对象，登记到对应类别的 partial 链表并增加页计数。
    list_add(&partial[index], &s->link);
    held_pages++;
    return s;
}

void *
kmalloc(size_t size) {
    // 零字节请求不占用空间，按本分配器约定返回空指针。
    if (size == 0) {
        return NULL;
    }
    // 找到能容纳请求的最小小对象类别；超过最大类别时改用整页分配。
    size_t index = 0;
    while (index < CLASS_COUNT && sizes[index] < size) {
        index++;
    }
    if (index == CLASS_COUNT) {
        // 计算所需页数前检查加法溢出，防止尺寸回绕导致分配过小区域。
        if (size > (~(size_t)0) - HEADER_SIZE - PGSIZE + 1) {
            return NULL;
        }
        // 大对象在分配区域起始处存放头部，余下空间用于返回给调用方。
        size_t n = (size + HEADER_SIZE + PGSIZE - 1) / PGSIZE;
        // 记录分配前后的空闲页数差，以统计底层分配器实际交付的页数；
        // 某些页分配器可能按更大的块粒度满足请求。
        size_t available = nr_free_pages();
        struct Page *p = alloc_pages(n);
        if (p == NULL) {
            return NULL;
        }
        size_t allocated_pages = available - nr_free_pages();
        // 大对象使用 class_index == CLASS_COUNT 与小对象 slab 区分，
        // page_count 保存必须整体释放的实际页数。
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
        // 当前类别没有可用槽位的 slab 时，分配并初始化一个新 slab。
        s = new_slab(index);
        if (s == NULL) {
            return NULL;
        }
    }
    else {
        // partial 链表只包含有空闲对象的 slab，因此取其首节点即可分配。
        s = to_struct(list_next(&partial[index]), struct slab, link);
    }
    // 从对象空闲链表头部弹出一个槽位，并计算它在本 slab 中的槽位编号。
    void *obj = s->free_objects;
    s->free_objects = *(void **)obj;
    size_t slot = ((char *)obj - (char *)s - HEADER_SIZE) / sizes[index];
    // 标记槽位已分配并更新占用数；若 slab 已满，则从 partial 链表移除，
    // 避免后续分配再次选中没有空闲对象的 slab。
    s->allocated[slot / 64] |= 1UL << (slot % 64);
    s->in_use++;
    if (s->in_use == s->capacity) {
        list_del_init(&s->link);
    }
    return obj;
}

void
kfree(void *ptr) {
    // 释放空指针无操作，便于调用方安全处理可选分配结果。
    if (ptr == NULL) {
        return;
    }
    // slab 元数据位于页首；将对象地址向下取整到页边界即可找到所属头部。
    struct slab *s = (struct slab *)ROUNDDOWN((uintptr_t)ptr, PGSIZE);
    assert(s->magic == SLAB_MAGIC);
    if (s->class_index == CLASS_COUNT) {
        // 大对象只允许释放 kmalloc 返回的起始地址，不能释放区域内部指针。
        assert(ptr == (char *)s + HEADER_SIZE);
        size_t n = s->page_count;
        // 先使头部失效并扣减 SLUB 统计，再将整段页归还底层物理页分配器。
        s->magic = 0;
        held_pages -= n;
        free_pages(pa2page(PADDR(s)), n);
        return;
    }

    size_t index = s->class_index;
    assert(index < CLASS_COUNT);
    // 验证指针位于对象区且严格落在某个槽位起点，拒绝内部地址。
    uintptr_t offset = (char *)ptr - (char *)s;
    assert(offset >= HEADER_SIZE &&
           (offset - HEADER_SIZE) % sizes[index] == 0);
    size_t slot = (offset - HEADER_SIZE) / sizes[index];
    // 同时检查槽位范围和分配位图，防止越界释放或重复释放。
    assert(slot < s->capacity &&
           (s->allocated[slot / 64] & (1UL << (slot % 64))));
    s->allocated[slot / 64] &= ~(1UL << (slot % 64));
    // 若 slab 原先已满，它此前不在 partial 链表中；此时释放出槽位，
    // 需要重新登记，供后续 kmalloc 使用。
    if (s->in_use == s->capacity) {
        list_add(&partial[index], &s->link);
    }
    // 将刚释放的对象压回该 slab 的空闲链表，并减少占用槽位数。
    *(void **)ptr = s->free_objects;
    s->free_objects = ptr;
    s->in_use--;
    if (s->in_use == 0) {
        // 整个 slab 已无存活对象：从类别链表摘除、使元数据失效，
        // 并将其物理页归还底层分配器，避免继续保留空 slab。
        list_del(&s->link);
        s->magic = 0;
        held_pages--;
        free_page(pa2page(PADDR(s)));
    }
}
