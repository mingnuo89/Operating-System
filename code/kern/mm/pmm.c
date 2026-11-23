#include <default_pmm.h>
#include <defs.h>
#include <error.h>
#include <kmalloc.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <sync.h>
#include <vmm.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// The kernel image is mapped at VA=KERNBASE and PA=info.base
uint_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
pde_t *boot_pgdir_va = NULL;
// physical address of boot-time page directory
uintptr_t boot_pgdir_pa;

// physical memory management
const struct pmm_manager *pmm_manager;

static void check_alloc_page(void);
static void check_pgdir(void);
static void check_boot_pgdir(void);

// init_pmm_manager - initialize a pmm_manager instance
/*
 * 中文导读：init_pmm_manager
 * 选择物理页分配器并调用其初始化入口。接口名为 default，当前实际绑定 Best-Fit 实现。
 *
 * pmm_manager 是统一函数表指针，后续申请释放通过它调用具体算法。
 * 先调用 init 建空链表，再由 page_init 登记可用内存，因此两步顺序不可颠倒。
 */
static void init_pmm_manager(void)
{
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
/*
 * 中文导读：init_memmap
 * 把 base 起始的 n 个可用页登记到当前分配器。调用者先排除内核和描述符占用区。
 *
 * base 指向区间首页的描述符，n 是页数；具体块长与链表节点由所选算法维护。
 * 输入范围必须完整处于描述符数组管理范围内，不能把尚在使用的内核页登记为空闲。
 */
static void init_memmap(struct Page *base, size_t n)
{
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
/*
 * 中文导读：alloc_pages
 * 申请 n 个连续物理页，返回块首页的 Page 指针，失败返回 NULL。
 * 保存并关闭本 hart 中断后调用分配器，避免本 hart 的中断路径同时改动空闲链表。
 *
 * 调用成功后返回连续块首页，连续 n 页可以作为内核栈或较大的内核缓冲区。
 * 本函数只获得物理页所有权，清零、映射权限与对象初始化由上层按用途完成。
 */
struct Page *alloc_pages(size_t n)
{
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag);
    return page;
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
/*
 * 中文导读：free_pages
 * 归还 base 起始的 n 个连续页。通过中断保护调用分配器，后者负责插回并合并相邻空闲块。
 *
 * base 和 n 必须与实际持有的区间一致，错误范围会破坏分配器账目。
 * 调用结束后原指针指向的内存可能被复用，调用者应先清除仍会使用它的映射或引用。
 */
void free_pages(struct Page *base, size_t n)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
/*
 * 中文导读：nr_free_pages
 * 读取当前空闲物理页总数。读取期间保护本 hart 中断状态，返回的是页数。
 *
 * 返回值以页为单位，乘 PGSIZE 才得到空闲字节数。
 * 自检会保存操作前的页数，再在释放资源后比较，用于发现页面泄漏。
 */
size_t nr_free_pages(void)
{
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_manager->nr_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
}

//lab2 请补充你在lab2中的代码
/*
 * 中文导读：page_init
 * 根据设备树内存范围建立 Page 描述符数组，数组紧随内核末尾并按页对齐。
 * 先将页面标为保留，排除内核、描述符和超出内核管理上限的区间，再登记剩余完整空闲页。
 * 注意 end 是虚拟地址，计算物理布局时使用 PADDR，避免描述符和空闲区重叠。
 *
 * mem_begin 与 mem_size 来自设备树，maxpa 被内核可映射上限截断。
 * npage 是物理页号上界，描述符个数用 npage-nbase 计算，不能直接把 npage 当数组长度。
 * 先为描述符数组本身预留空间，再按页向上对齐得到 free_begin，避免分配器覆盖自己的管理数据。
 * 只有 free_begin<free_end 时才登记空闲区，避免产生长度为零或越界的区间。
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
    const uint64_t physical_limit = KERNTOP - PHYSICAL_MEMORY_OFFSET;
    uint64_t maxpa = mem_end < physical_limit ? mem_end : physical_limit;
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

/*
 * 中文导读：enable_paging
 * 将启动页表物理基址编码为 Sv39 的 satp 值。低位存根页表物理页号，高四位为模式 8。
 * 调用时必须保证当前取指地址和后续访问地址在目标页表中有有效映射。
 *
 * pgdir 存物理根地址，satp 中写入的是物理页号，所以需要右移 RISCV_PGSHIFT。
 * 写 satp 与刷新 TLB 是不同操作，移植调用流程时还需保证刷新和当前取指映射。
 */
static void enable_paging(void)
{
    write_csr(satp, 0x8000000000000000 | (boot_pgdir_pa >> RISCV_PGSHIFT));
}

// boot_map_segment - setup&enable the paging mechanism
// parameters
//  la:   linear address of this memory need to map (after x86 segment map)
//  size: memory size
//  pa:   physical address of this memory
//  perm: permission of this memory
/*
 * 中文导读：boot_map_segment
 * 为 la 开始、size 字节的区间逐页建立到 pa 的映射，perm 指定权限。
 * 先处理起始地址的页内偏移，通过 get_pte 创建所需下级表，再写入各页的物理页号。
 *
 * 逐页写 PTE 前先把 la/pa 对齐，处理原地址的页内偏移，使 size 覆盖所需全部页面。
 * get_pte 失败时触发断言，因此该启动辅助函数要求内存足够建立映射。
 */
static void boot_map_segment(pde_t *pgdir, uintptr_t la, size_t size,
                             uintptr_t pa, uint32_t perm)
{
    assert(PGOFF(la) == PGOFF(pa));
    size_t n = ROUNDUP(size + PGOFF(la), PGSIZE) / PGSIZE;
    la = ROUNDDOWN(la, PGSIZE);
    pa = ROUNDDOWN(pa, PGSIZE);
    for (; n > 0; n--, la += PGSIZE, pa += PGSIZE)
    {
        pte_t *ptep = get_pte(pgdir, la, 1);
        assert(ptep != NULL);
        *ptep = pte_create(pa >> PGSHIFT, PTE_V | perm);
    }
}

// boot_alloc_page - allocate one page using pmm->alloc_pages(1)
// return value: the kernel virtual address of this allocated page
// note: this function is used to get the memory for PDT(Page Directory
// Table)&PT(Page Table)
/*
 * 中文导读：boot_alloc_page
 * 启动阶段申请一页并返回其内核虚拟地址。分配失败直接 panic，供启动代码建立必要结构。
 *
 * alloc_page 返回描述符，page2kva 把它变成内核可以直接访问的指针。
 * 启动必需结构无法分配时无法安全继续，因此该函数直接报告致命失败。
 */
static void *boot_alloc_page(void)
{
    struct Page *p = alloc_page();
    if (p == NULL)
    {
        panic("boot_alloc_page failed.\n");
    }
    return page2kva(p);
}

// pmm_init - setup a pmm to manage physical memory, build PDT&PT to setup
// paging mechanism
//         - check the correctness of pmm & paging mechanism, print PDT&PT
/*
 * 中文导读：pmm_init
 * 串起物理内存初始化。选择分配器、登记物理页、运行分配自检，取得启动页表地址。
 * 接着检查页表及内核映射，最后初始化 kmalloc，为创建 PCB 等小对象提供内存。
 *
 * 先有可用物理页，get_pte 才能申请页表；先有小块分配器，alloc_proc 才能申请 PCB。
 * boot_pgdir_va 用于内核读写页表，boot_pgdir_pa 用于 satp 硬件装载，二者不能混用。
 */
void pmm_init(void)
{
    // We need to alloc/free the physical memory (granularity is 4KB or other
    // size).
    // So a framework of physical memory manager (struct pmm_manager)is defined
    // in pmm.h
    // First we should init a physical memory manager(pmm) based on the
    // framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a
    // pmm
    check_alloc_page();
    // create boot_pgdir, an initial page directory(Page Directory Table, PDT)
    extern char boot_page_table_sv39[];
    boot_pgdir_va = (pte_t *)boot_page_table_sv39;
    boot_pgdir_pa = PADDR(boot_pgdir_va);

    check_pgdir();

    static_assert(KERNBASE % PTSIZE == 0 && KERNTOP % PTSIZE == 0);

    // now the basic virtual memory map(see memalyout.h) is established.
    // check the correctness of the basic virtual memory map.
    check_boot_pgdir();

    kmalloc_init();
}

// get_pte - get pte and return the kernel virtual address of this pte for la
//        - if the PT contians this pte didn't exist, alloc a page for PT
// parameter:
//  pgdir:  the kernel virtual base address of PDT
//  la:     the linear address need to map
//  create: a logical value to decide if alloc a page for PT
// return vaule: the kernel virtual address of this pte
/*
 * 中文导读：get_pte
 * 查找线性地址 la 对应的最后一级页表项，pgdir 是根页表虚拟地址。
 * create 为真时允许申请缺失的两级下级表，为假时缺表返回 NULL。
 * 新中间表暂存到后续申请成功才发布，第二次申请失败会释放本次新表；目录项只带合法目录标志。
 * 返回的是页表项地址，调用者还需检查 PTE_V 判断该虚拟页是否已经映射。
 *
 * PDX1、PDX0、PTX 分别取虚拟地址的三级索引，每一级选择一个表项。
 * 目录项保存物理页号，用 PDE_ADDR 还原物理基址，再通过 KADDR 转换后才能在 C 中解引用。
 * 返回非空仅说明末级槽位可访问，即使 *ptep=0 也可以返回该槽位，供 page_insert 写入。
 * create=true 仅在缺表时申请，并不直接给 la 分配数据页；本实现面向路径上的普通下级表，不展开巨大页叶项。
 */
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    pde_t *pdep1 = &pgdir[PDX1(la)];
    struct Page *new_table = NULL;
    pde_t directory = *pdep1;
    if (!(*pdep1 & PTE_V))
    {
        if (!create || (new_table = alloc_page()) == NULL)
        {
            return NULL;
        }
        set_page_ref(new_table, 1);
        memset(page2kva(new_table), 0, PGSIZE);
        directory = ptd_create(page2ppn(new_table));
    }
    pde_t *pdep0 = &((pte_t *)KADDR(PDE_ADDR(directory)))[PDX0(la)];
    if (!(*pdep0 & PTE_V))
    {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
        {
            if (new_table != NULL)
            {
                set_page_ref(new_table, 0);
                free_page(new_table);
            }
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep0 = ptd_create(page2ppn(page));
        if (new_table != NULL)
        {
            *pdep1 = directory;
        }
        flush_tlb();
    }
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
}

// get_page - get related Page struct for linear address la using PDT pgdir
/*
 * 中文导读：get_page
 * 查找 la 已映射的物理页，缺表或无有效映射时返回 NULL。
 * ptep_store 非空时同时返回页表项地址，便于调用者清除或修改该项。
 *
 * pgdir 和 la 指定待查映射，ptep_store 是可选的输出参数，类型为 pte_t **。
 * 通过 get_pte(...,0) 查询，不会因为读取映射而新建页表；有效位为真后才用 pte2page 还原物理页描述符。
 */
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep_store != NULL)
    {
        *ptep_store = ptep;
    }
    if (ptep != NULL && *ptep & PTE_V)
    {
        return pte2page(*ptep);
    }
    return NULL;
}

//LAB4 function1 2410849
/*
 * 中文导读：page_insert
 * 将物理页 page 映射到 la，perm 指定权限，成功返回 0，页表分配失败返回 -E_NO_MEM。
 * 新旧物理页不同时增加新页引用，再写新映射并刷新地址翻译，随后减少旧页引用并按需释放。
 * 映射同一页只更新权限，不改变引用数；pte_create 会自动设置有效位 PTE_V。
 *
 * 三种情况分别是空槽首次映射、旧页替换成另一页、同页只调整权限。
 * 例如原来映射 A，现在改成 B：B.ref 加一，PTE 写 B，失效旧翻译，A.ref 减一，归零才释放 A。
 * 同页分支跳过引用增减，避免把仍在映射的页意外释放；分配下级表失败时在改写原映射前返回。
 * perm 可组合 U/R/W/X 等位，pte_create 自动加 V；软件测试所用某些权限组合只检查账目，不代表可实际访存。
 */
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)
{
    pte_t *ptep = get_pte(pgdir, la, 1);
    if (ptep == NULL)
    {
        return -E_NO_MEM;
    }

    struct Page *old_page = (*ptep & PTE_V) ? pte2page(*ptep) : NULL;
    if (old_page != page)
    {
        page_ref_inc(page);
    }
    *ptep = pte_create(page2ppn(page), perm);
    tlb_invalidate(pgdir, la);
    if (old_page != NULL && old_page != page && page_ref_dec(old_page) == 0)
    {
        free_page(old_page);
    }
    return 0;
}

/*
 * 中文导读：page_remove
 * 撤销 la 的有效映射，未映射时直接结束，因此重复移除也安全。
 * 先清页表项并刷新对应 TLB，再减少物理页引用，引用归零才将物理页交回分配器。
 *
 * 例如 A 被两个地址映射，移除一个后 A.ref 从 2 变 1，保留物理页；最后一个移除后才回收。
 * 清 PTE 后先失效 TLB，再释放页面，可以避免缓存旧翻译继续访问已经被复用的物理页。
 * 此函数回收数据页，空的中间页表仍由相应管理或清理流程处理。
 */
void page_remove(pde_t *pgdir, uintptr_t la)
{
    pte_t *ptep;
    struct Page *page = get_page(pgdir, la, &ptep);
    if (page == NULL)
    {
        return;
    }
    *ptep = 0;
    tlb_invalidate(pgdir, la);
    if (page_ref_dec(page) == 0)
    {
        free_page(page);
    }
}

// invalidate a TLB entry, but only if the page tables being
// edited are the ones currently in use by the processor.
/*
 * 中文导读：tlb_invalidate
 * 让本 hart 丢弃 la 对应的缓存翻译，x0 表示涵盖全部 ASID。
 * memory clobber 约束编译器重排相关内存操作；pgdir 参数当前未用于筛选正在使用的页表。
 *
 * la 是需要失效的虚拟地址，硬件会按地址对应的映射处理缓存。
 * sfence.vma 作用于当前 hart，本实验单 hart 使用；跨核缓存同步需独立机制。
 */
void tlb_invalidate(pde_t *pgdir, uintptr_t la)
{
    // flush_tlb();
    // The flush_tlb flush the entire TLB, is there any better way?
    asm volatile("sfence.vma %0, x0" : : "r"(la) : "memory");
}

/*
 * 中文导读：check_alloc_page
 * 执行当前分配器的自检，断言覆盖申请、释放和空闲块管理，完成后输出成功标志。
 *
 * 实际测试函数来自 pmm_manager->check，因此换分配器后这项入口可以复用。
 * 输出成功信息表示测试执行到了末尾，具体检查范围仍由该分配器测试决定。
 */
static void check_alloc_page(void)
{
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}

/*
 * 中文导读：check_pgdir
 * 检查页表查找、创建、替换映射及引用数变化，再回收本次测试使用的页表。
 * 同时检查非叶目录项没有 U/A/D 标志，符合这里使用的 Sv39 目录格式。
 *
 * 测试通过 get_pte、page_insert、page_remove 来检查接口间的配合。
 * 映射同页和不同页的引用数是重点，最后释放测试页表并比较空闲页总数。
 */
static void check_pgdir(void)
{
    // assert(npage <= KMEMSIZE / PGSIZE);
    // The memory starts at 2GB in RISC-V
    // so npage is always larger than KMEMSIZE / PGSIZE
    size_t nr_free_store;

    nr_free_store = nr_free_pages();

    assert(npage <= KERNTOP / PGSIZE);
    assert(boot_pgdir_va != NULL && (uint32_t)PGOFF(boot_pgdir_va) == 0);
    assert(get_page(boot_pgdir_va, 0x0, NULL) == NULL);

    struct Page *p1, *p2;
    p1 = alloc_page();
    assert(page_insert(boot_pgdir_va, p1, 0x0, 0) == 0);

    pte_t *ptep;
    assert((ptep = get_pte(boot_pgdir_va, 0x0, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert(page_ref(p1) == 1);

    ptep = (pte_t *)KADDR(PDE_ADDR(boot_pgdir_va[0]));
    ptep = (pte_t *)KADDR(PDE_ADDR(ptep[0])) + 1;
    assert(get_pte(boot_pgdir_va, PGSIZE, 0) == ptep);

    p2 = alloc_page();
    assert(page_insert(boot_pgdir_va, p2, PGSIZE, PTE_U | PTE_W) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(*ptep & PTE_U);
    assert(*ptep & PTE_W);
    assert((boot_pgdir_va[0] & (PTE_U | PTE_A | PTE_D)) == 0);
    assert(page_ref(p2) == 1);

    assert(page_insert(boot_pgdir_va, p1, PGSIZE, 0) == 0);
    assert(page_ref(p1) == 2);
    assert(page_ref(p2) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert((*ptep & PTE_U) == 0);

    page_remove(boot_pgdir_va, 0x0);
    assert(page_ref(p1) == 1);
    assert(page_ref(p2) == 0);

    page_remove(boot_pgdir_va, PGSIZE);
    assert(page_ref(p1) == 0);
    assert(page_ref(p2) == 0);

    assert(page_ref(pde2page(boot_pgdir_va[0])) == 1);

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();

    assert(nr_free_store == nr_free_pages());

    cprintf("check_pgdir() succeeded!\n");
}

/*
 * 中文导读：check_boot_pgdir
 * 通过两个低地址别名映射同一物理页，实际写入和读取字符串验证地址翻译。
 * 最后回收测试页与下级页表、刷新 TLB，并比较空闲页数是否恢复。
 *
 * 把同一页映射到 0x100 和 0x100+PGSIZE，一处写入后从另一处应读到相同内容。
 * 这是实际访存检查，目录项、叶项权限和 TLB 都会影响结果，比单纯读取 PTE 覆盖更多硬件路径。
 */
static void check_boot_pgdir(void)
{
    size_t nr_free_store;
    pte_t *ptep;
    int i;

    nr_free_store = nr_free_pages();

    for (i = ROUNDDOWN(KERNBASE, PGSIZE); i < npage * PGSIZE; i += PGSIZE)
    {
        assert((ptep = get_pte(boot_pgdir_va, (uintptr_t)KADDR(i), 0)) != NULL);
        assert(PTE_ADDR(*ptep) == i);
    }

    assert(boot_pgdir_va[0] == 0);

    struct Page *p;
    p = alloc_page();
    assert(page_insert(boot_pgdir_va, p, 0x100, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 1);
    assert(page_insert(boot_pgdir_va, p, 0x100 + PGSIZE, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 2);

    const char *str = "ucore: Hello world!!";
    strcpy((void *)0x100, str);
    assert(strcmp((void *)0x100, (void *)(0x100 + PGSIZE)) == 0);

    *(char *)(page2kva(p) + 0x100) = '\0';
    assert(strlen((const char *)0x100) == 0);

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(p);
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();

    assert(nr_free_store == nr_free_pages());

    cprintf("check_boot_pgdir() succeeded!\n");
}

// perm2str - use string 'u,r,w,-' to present the permission
static const char *perm2str(int perm)
{
    static char str[4];
    str[0] = (perm & PTE_U) ? 'u' : '-';
    str[1] = 'r';
    str[2] = (perm & PTE_W) ? 'w' : '-';
    str[3] = '\0';
    return str;
}

// get_pgtable_items - In [left, right] range of PDT or PT, find a continuous
// linear addr space
//                  - (left_store*X_SIZE~right_store*X_SIZE) for PDT or PT
//                  - X_SIZE=PTSIZE=4M, if PDT; X_SIZE=PGSIZE=4K, if PT
// paramemters:
//  left:        no use ???
//  right:       the high side of table's range
//  start:       the low side of table's range
//  table:       the beginning addr of table
//  left_store:  the pointer of the high side of table's next range
//  right_store: the pointer of the low side of table's next range
//  return value: 0 - not a invalid item range, perm - a valid item range with
//  perm permission
static int get_pgtable_items(size_t left, size_t right, size_t start,
                             uintptr_t *table, size_t *left_store,
                             size_t *right_store)
{
    if (start >= right)
    {
        return 0;
    }
    while (start < right && !(table[start] & PTE_V))
    {
        start++;
    }
    if (start < right)
    {
        if (left_store != NULL)
        {
            *left_store = start;
        }
        int perm = (table[start++] & PTE_USER);
        while (start < right && (table[start] & PTE_USER) == perm)
        {
            start++;
        }
        if (right_store != NULL)
        {
            *right_store = start;
        }
        return perm;
    }
    return 0;
}
