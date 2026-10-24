#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <buddy_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
#if defined(PMM_BUDDY)
    pmm_manager = &buddy_pmm_manager;
#elif defined(PMM_FIRST_FIT)
    pmm_manager = &default_pmm_manager;
#else
    pmm_manager = &best_fit_pmm_manager;
#endif
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
}

//lab2 function1 2410878
/* page_init - 物理内存探测与页描述符初始化
 *
 * 1. 从设备树得到物理内存范围：dtb_init() 已把结果存进 memory_base/memory_size
 * 2. 确定内核可管理的物理页号上界 npage，并在内核镜像 end 之后按页对齐
 *    放置 Page 描述符数组
 * 3. 先把描述符全部置为保留，再把“描述符数组之后 ~ 可管理内存上界”之间的
 *    完整页面交给当前 pmm_manager
 */
static void page_init(void) {
    // 内核虚拟地址 = 物理地址 + 固定偏移，这个偏移要最先确定
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;

    uint64_t mem_begin = get_memory_base();
    uint64_t mem_size = get_memory_size();
    if (mem_size == 0) {
        // 设备树信息无效，后续布局无从谈起
        panic("DTB memory info not available");
    }
    if (mem_begin < DRAM_BASE) {
        // 起始地址低于 DRAM_BASE 时，页号换算的前提（nbase）不成立
        panic("invalid physical memory range");
    }
    uint64_t mem_end = mem_begin + mem_size;

    cprintf("physcial memory map:\n");
    cprintf("  memory: 0x%016lx, [0x%016lx, 0x%016lx].\n", mem_size, mem_begin,
            mem_end - 1);

    // 内核最多管理到 PHYSICAL_MEMORY_END（QEMU 默认 128MiB 上是 0x88000000）
    uint64_t maxpa = mem_end < PHYSICAL_MEMORY_END ? mem_end : PHYSICAL_MEMORY_END;
    if (maxpa <= DRAM_BASE) {
        panic("no manageable physical memory");
    }

    // npage 是“可管理物理页号的上界”，不是页数；页数是 npage - nbase
    npage = maxpa / PGSIZE;
    size_t total_pages = npage - nbase;

    /*
     * Page 描述符数组紧跟在BSS段末尾之后，这里 end 是内核链接符号，给的是
     * 虚拟地址，而描述符数组本身要占物理内存，所以下面计算数组末尾时
     * 必须先用 PADDR 转成物理地址。数组起点向上按页对齐。
     */
    extern char end[];
    pages = (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE);
    uintptr_t pages_pa = PADDR(pages);

    // 描述符数组本身不能越过可管理内存上界，否则后面必然越界写
    if (pages_pa >= maxpa ||
        total_pages > (maxpa - pages_pa) / sizeof(struct Page)) {
        panic("not enough memory for Page descriptors");
    }

    // 布局尚未完成，先把 [nbase, npage) 的所有描述符都标记为保留，
    // 防止这些页面在建立空闲区之前被分配出去
    for (size_t i = 0; i < total_pages; i++) {
        SetPageReserved(pages + i);
    }

    // 描述符数组自身也必须排除在空闲区之外：数组末尾 -> 物理地址
    uintptr_t freemem = pages_pa + sizeof(struct Page) * total_pages;

    /*
     * 空闲区两端都要按页对齐：起点向上取整；终点用 maxpa 而不是 mem_end，
     * 否则 qemu -m 大于 128MiB 时，会把超出 npage 的页面交给分配器，
     * 造成描述符数组越界（default_init_memmap 里的 assert(PageReserved) 会挂）。
     */
    uint64_t free_begin = ROUNDUP(freemem, PGSIZE);
    uint64_t free_end = ROUNDDOWN(maxpa, PGSIZE);

    // 没有完整的空闲页就不要调用 init_memmap
    if (free_begin < free_end) {
        init_memmap(pa2page(free_begin), (free_end - free_begin) / PGSIZE);
    }
}

/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
