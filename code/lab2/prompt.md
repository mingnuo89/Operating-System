# Lab2 个人部分提示词

整理日期：2026-10-24。对应组员 B 的 Best-Fit、SLUB 和 GDB 调试要求 3、4。

以下四份提示词由 AI 协助，按指导书四段式格式，结合现有接口和实现补充整理，用于复核需求和复现实验。它们不是最初开发对话的逐字记录，也不表示代码最初由这些提示词生成。实现和测试记录仍以报告及 `evidence/` 中的实际材料为准。

格式依据：[指导书“提示词结构”](http://oslab.mobisys.top/lab2026/_book/lab0.5/3_prompt_structure.html)。同一份正文已放入 [个人报告](report-part-b.md) 的对应位置。

## P-BF：Best-Fit 实现

````markdown
[PROMPT]
**任务**：我负责 Lab2 的 Best-Fit。请补全 `kern/mm/best_fit_pmm.c`，并在 `kern/mm/pmm.c` 的 `init_pmm_manager()` 中接入，让默认配置真正使用 Best-Fit。

**操作要求**：必须修改项目里的实际文件。保留框架中的 `basic_check()`、`best_fit_check()` 和官方评分文件，不要通过改测试来让结果通过。保留 First-Fit、Buddy 的切换方式，只修改这个模块需要的位置。

**输出要求**：先核对下面的接口与工程是否一致，再完成实现、编译和检查。说明选块、拆分、合并及空闲页计数如何保持一致，给出实际运行结果；没有运行的检查要说明。最后结合这份链表实现，简要说明 Best-Fit 的改进空间。

[RELY]
`memlayout.h` 中的相关定义是：
```c
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};
typedef struct {
    list_entry_t free_list;
    unsigned int nr_free;
} free_area_t;
```
`Page` 是物理页描述符，连续描述符对应连续物理页，不是可直接读写的页面数据。空闲块的头页设置 `PageProperty`，`property` 记录连续页数。可以使用 `PageReserved`、`PageProperty`、`SetPageProperty`、`ClearPageProperty`、`set_page_ref()`、`le2page()` 和 `list.h` 的链表操作。

`pmm_manager` 在 `pmm.h` 中包含 `name`、`init`、`init_memmap`、`alloc_pages`、`free_pages`、`nr_free_pages`、`check`。现有测试会直接使用模块内的 `free_list` 和 `nr_free`，需要保留兼容的状态定义。

[GUARANTEE]
需要实现的函数及注册对象：
```c
static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);
extern const struct pmm_manager best_fit_pmm_manager;
```
需要修改 `pmm.c` 中的 `static void init_pmm_manager(void)`，默认选择该管理器。`best_fit_pmm_manager.check` 仍指向原有 `best_fit_check()`。可以添加模块内部的静态辅助函数，不改变公开接口。

[SPECIFICATION]
## best_fit_init
**Pre-Condition**：物理页管理器开始初始化，尚未登记可分配页面。
**Post-Condition**：空闲链表为空，空闲页数为 0，可以继续注册空闲区。

## best_fit_init_memmap
**Pre-Condition**：`n > 0`，`base` 起的 n 个描述符有效、连续，且仍为保留状态；这段区域不与已登记区域重叠。
**Post-Condition**：这 n 页解除保留，引用计数清零；以 `base` 为头登记一个 n 页空闲块，空闲页数增加 n。链表按地址递增，已有空闲块不丢失。插入到空表、表头、中间或表尾都应正确结束。

## best_fit_alloc_pages
**Pre-Condition**：`n > 0`；空闲链表和计数有效。
**Post-Condition**：成功时返回至少包含 n 个连续物理页的起始描述符，本模块实际分配 n 页；这些页不再属于空闲块，空闲总数减少 n，其他空闲区保持有效。
**Case 1**：有多个块能满足请求时，选择页数最少的块；大小相同则保留地址靠前的块。不能遇到第一个够大的块就返回。
**Case 2**：选中块恰好为 n 页时移除整个块；大于 n 时取出前 n 页，剩余部分作为空闲块保留，链表仍按地址排序。
**Case 3**：总空闲页不足，或虽然总数足够但没有足够大的连续块时，返回 NULL，链表和计数不变。

## best_fit_free_pages
**Pre-Condition**：`n > 0`；这段连续页由本管理器分配，尚未释放，且不包含保留页。调用者负责传入合法范围。
**Post-Condition**：页面引用计数恢复为 0，空闲页总数增加 n；空闲链表按地址排序，不包含重叠的块。只把实际物理连续的块合并，合并后只保留一个有效块头及正确页数。
**Case 1**：与前后都不连续时独立插入。
**Case 2**：只与一侧连续时合并该侧；两侧都连续时合并成一个块。被合并掉的块头应移出链表，并清除其 `PageProperty` 标志。

## best_fit_nr_free_pages
**Pre-Condition**：管理器已经初始化。
**Post-Condition**：返回全部空闲页数，不改变链表和计数。

## init_pmm_manager 与管理器注册
**Pre-Condition**：物理内存初始化将选择页分配器。
**Post-Condition**：默认配置选择 `best_fit_pmm_manager`，函数指针与上述接口对应；显式选择 First-Fit 或 Buddy 的配置仍可使用。
**Requirements**：运行时核对管理器名称，再执行原有自检和 `make grade`。分别解释通过了哪些检查；不要把官方评分当成 SLUB 和全部调试任务的评分。
````

## P-SLUB-I：SLUB 实现

````markdown
[PROMPT]
**任务**：请在 Lab2 中实现一个简化 SLUB，代码放在 `kern/mm/slub.c`，接口放在 `kern/mm/slub.h`，并在 `kern/init/init.c` 接入初始化。指导书允许简化，只要体现“底层分配页，上层分配对象”这两层关系即可。

**操作要求**：必须修改实际文件，保留现有页分配器、自检和启动逻辑。实现与测试分开，这份先完成分配器，不把测试写进分配函数。按单核实验考虑，不添加完整 Linux SLUB 的机制。

**输出要求**：说明小对象、大对象分别怎么申请和回收，特别检查整页回收及失败时是否泄漏。完成后编译，列出修改的文件和实际检查结果。

[RELY]
工程已经有以下页分配接口：
```c
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
```
还有 `alloc_page()`、`free_page()`、`PGSIZE`（4096）、`page2pa()`、`pa2page()`、`PADDR()`、`va_pa_offset`、链表、断言和 `memset()`。页面数据的内核地址可以由 `page2pa(p) + va_pa_offset` 得到，不能直接把 `struct Page *` 当成对象空间。

底层可切换 Best-Fit、First-Fit、Buddy。Buddy 可能向上取整，所以连续页请求占用的实际页数可能大于参数 n。

[GUARANTEE]
```c
void slub_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
size_t slub_pages_in_use(void);
```
在头文件声明接口；在 `int kern_init(void)` 中于 `pmm_init()` 后调用初始化，保留原有调用顺序。允许增加静态辅助函数和模块内部结构，不改变底层页管理接口。

[SPECIFICATION]
## slub_init
**Pre-Condition**：页管理器已完成初始化；SLUB 尚无存活对象，启动时调用一次。
**Post-Condition**：各规格的可分配页面链表为空，SLUB 持有页数为 0，后续可以调用分配和释放接口。

## kmalloc
**Pre-Condition**：SLUB 已初始化，当前为单核、无并发调用的环境。
**Post-Condition**：成功时返回至少可用 size 字节、按 16 字节对齐的地址，使用区间不与其他存活对象重叠。分配新对象不能破坏已有对象数据。
**Case 1**：size 为 0 时返回 NULL，不申请页面。
**Case 2**：1—2048 字节使用 16、32、64、128、256、512、1024、2048 这八种规格，选择不小于请求的最小规格。已有页面有空槽时先复用，没有空槽时再申请新页。每页容量应扣除管理头占用的空间，不能把元数据也分配给对象。
**Case 3**：超过 2048 字节时申请连续页，预留管理头后仍能容纳请求。记录底层实际分配的页数，释放时能完整归还，兼容 Buddy 的向上取整。
**Case 4**：尺寸计算溢出或页面不足时返回 NULL，不泄漏页面，不改变已有对象和有效计数。

## kfree
**Pre-Condition**：ptr 为 NULL，或是本接口返回且尚未释放的完整对象指针；不要求支持任意非法地址。
**Post-Condition**：释放对象后，它不再占用对象槽位或大对象页面；其他存活对象仍可正常读写，页数统计与实际占用一致。
**Case 1**：NULL 不做操作。
**Case 2**：小对象槽位重新可用；原本满的页面重新进入可分配状态。页面最后一个对象释放后，整页立即归还底层。
**Case 3**：大对象按保存的实际页数一次归还，不能只归还最初请求的页数。
**Requirements**：用必要的标记和分配位图检查对象槽位及重复释放；断言用于暴露调用错误，不宣称分配器能安全处理所有非法指针。

## slub_pages_in_use
**Pre-Condition**：SLUB 已初始化。
**Post-Condition**：返回 SLUB 当前实际占用的物理页数，包括小对象页和大对象页，不改变分配状态；全部对象释放后为 0。

## kern_init 接入
**Pre-Condition**：保留原框架的内存、控制台及页管理初始化。
**Post-Condition**：SLUB 在页管理器可用后初始化，原有页管理检查仍执行；测试可以在此后单独接入。
````

## P-SLUB-T：SLUB 测试

````markdown
[PROMPT]
**任务**：请给 Lab2 的 SLUB 单独写测试，放在 `kern/mm/slub_check.c`，在 `slub.h` 声明并在 `kern_init()` 中于 `slub_init()` 后调用。我需要能说明每组测试检查什么，而不只是看到一次“分配成功”。

**操作要求**：必须修改实际文件。测试通过公开接口检查，不读取分配器私有结构，也不为了通过测试修改 Best-Fit、Buddy、SLUB 实现或官方评分器。发现实现问题时先给出失败位置和原因。

**输出要求**：实际编译运行，保留断言和分组输出。说明尺寸边界、数据隔离、复用、回收、失败这几类用例各能发现什么错误，并分别在三个底层分配器配置下验证。没有执行的配置不要写成通过。

[RELY]
SLUB 的公开接口是：
```c
void slub_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
size_t slub_pages_in_use(void);
```
约定：正尺寸成功时至少有 size 字节、16 字节对齐；`kmalloc(0)` 返回 NULL；`kfree(NULL)` 不做操作；最后一个对象释放后归还所在页面；失败不破坏已有对象。小对象规格为 16 到 2048 的八个 2 的幂规格，较大对象走连续页。

还可以使用 `nr_free_pages()`、`alloc_page()`、`free_page()`、`PGSIZE`、`Page.page_link`、`le2page()`、链表、`assert()`、`cprintf()` 和 `memset()`。测试开始时 SLUB 没有存活对象。

[GUARANTEE]
```c
void slub_check(void);
```
在 `slub.h` 增加声明，在 `int kern_init(void)` 中接入。允许静态辅助函数和静态对象指针数组，避免把几百个指针放到有限的内核栈上。

[SPECIFICATION]
## slub_check
**Pre-Condition**：页管理器和 SLUB 已初始化；没有其他对象分配或并发活动。记录测试前的空闲页数，起始 SLUB 占用为 0。
**Post-Condition**：全部断言通过时，所有测试对象和测试临时占用页均已释放，SLUB 占用回到 0，底层空闲页数恢复到测试前。分组打印结果，最后打印 `slub_check() succeeded!`。

**Case 1：尺寸、对齐、数据隔离。** 使用 1、15、16、17、31、32、33、63、64、65、127、128、129、255、256、257、511、512、513、1023、1024、1025、2047、2048、2049、4096、8193，共 27 种尺寸。同时保留对象，检查非空、16 字节对齐及使用区间两两不重叠；给各对象写入不同模式，全部申请后再核对。释放后检查页数恢复。

**Case 2：多页、复用、回收。** 同时申请 600 个 16 字节对象，确认至少使用 3 页。每个对象写入模式，释放一个已满页面中的槽位后重新申请，检查该槽位能够复用，再检查其他对象的数据。按先偶数、后奇数下标释放全部对象，核对 SLUB 和底层页数恢复。

**Case 3：特殊参数和大请求失败。** 检查零长度、NULL 释放、最大 size_t 导致的尺寸溢出，以及申请超过当前可用物理页的请求。应返回 NULL 或按约定不操作，且不丢失页面。

**Case 4：真正耗尽底层页面。** 先申请一个 16 字节对象保留其数据，再用 `alloc_page()` 申请走剩余全部页，记录这些页以便回收。底层空闲为 0 时，同规格已有页面里的空槽仍应能分配；新规格 32 字节和大对象 8193 字节应失败，原对象数据保持。释放对象，再归还所有临时物理页，核对最终计数恢复。

**Requirements**：切换 Best-Fit、First-Fit、Buddy 前分别清理再编译，运行同一套测试。测试失败不能继续打印成功；不执行会故意触发内核断言、从而中止后续检查的非法释放用例。

## kern_init 测试接入
**Pre-Condition**：保留现有启动流程，已经调用 `slub_init()`。
**Post-Condition**：调用 `slub_check()`，不省略原来的页管理自检；测试结束后维持框架的停留行为。
````

## P-GDB：GDB 调试要求 3、4

````markdown
[PROMPT]
**任务**：我负责指导书 GDB 要求 3、4。请基于带调试符号的 QEMU 4.1.1，帮我实际观察 TLB 查询、未命中后的翻译与填充，以及 Bare 模式下 QEMU 为什么仍使用软件 TLB。用 `kern/mm/mmu_probe.c` 提供可重复的访存，用 `tools/trace_mmu.py` 保存两个 GDB 会话的记录。

**操作要求**：必须修改项目实际文件，在 `kern/init/init.c` 声明并调用探针；保留现有内核和分配器自检。QEMU 使用单独的调试构建，不改它的地址翻译源码，不替换系统安装。只分析指导书要求 3、4 所需路径。

**输出要求**：给出真实命令、日志和相关源码位置；把观察到的值与解释对应起来。分清宿主 GDB 和客体 GDB，说明命中与未命中的差别，以及软件 TLB 和硬件 TLB 的用途差别。不能用写死的打印代替真实断点，也不能预先假定本次一定走完三级页表。

[RELY]
实验在 ARM64 Ubuntu 容器运行，内核为 RISC-V 64 位。宿主 `gdb` 调试 ARM64 的 QEMU 进程，`gdb-multiarch` 通过 QEMU stub 调试 `bin/kernel`。已有 `tools/build_debug_qemu.sh` 可构建 QEMU 4.1.1，默认源码目录为 `/tmp/lab2-qemu/qemu-4.1.1`。

需要查看的 QEMU 4.1.1 源码主要是 `accel/tcg/cputlb.c`、`target/riscv/cpu_helper.c` 和 ARM64 后端 `tcg/aarch64/tcg-target.inc.c`。关注 `load_helper()`、`riscv_cpu_tlb_fill()`、`get_physical_address()`、`tlb_set_page()`、`tlb_set_page_with_attrs()` 和 `tcg_out_tlb_read()`。

工程已有 `pmm_init()`，SLUB 初始化在它之后；内核符号可用 `riscv64-unknown-elf-nm bin/kernel` 读取。`boot_hartid` 在开启分页前写入，可以用来观察 Bare 模式。当前高地址映射使用根层的 1 GiB 叶子项。

[GUARANTEE]
内核接口：
```c
void mmu_probe(void);
int kern_init(void);
```
新增探针数据符号 `lab2_probe_value`，以及三条读取指令的位置符号 `lab2_probe_first`、`lab2_probe_second`、`lab2_probe_third`。

脚本接口：`python3 tools/trace_mmu.py --qemu-root <QEMU源码目录>`。生成 `evidence/host-trace.gdb`、`guest-trace.gdb`，记录 `gdb-host-tlb.log`、`gdb-guest.log`、`gdb-qemu-console.log`。可以增加脚本内部辅助逻辑，不改变原有内核接口。

[SPECIFICATION]
## mmu_probe
**Pre-Condition**：启动映射和页管理初始化已完成，探针数据可访问；当前为单核环境。
**Post-Condition**：对同一个、按页对齐的 `volatile uint64_t` 数据执行三条实际 `ld`，三次结果均为 `0x1122334455667788`，用断言核对并打印结果。
**Case 1**：第一次读取前执行 `sfence.vma`。
**Case 2**：第二次直接读取同一地址，不主动刷新，让它有机会复用已有 TLB 条目。
**Case 3**：第三次之前再执行 `sfence.vma`，观察重新填充。
**Requirements**：通过内联汇编和正确的寄存器约束保留这三次真实读取，避免被编译器合并，也不要让输出寄存器覆盖后续读取所需的地址。

## kern_init 探针接入
**Pre-Condition**：原有初始化和页管理自检不变。
**Post-Condition**：在 `pmm_init()` 后、`slub_init()` 前执行 `mmu_probe()`，便于客体 GDB 在 `slub_init()` 停下时检查三次读取已完成。

## trace_mmu.py
**Pre-Condition**：内核已编译，QEMU 4.1.1 调试构建及相应源码可用，两个 GDB 可执行，回环调试端口可用。
**Post-Condition**：从实际内核符号确定探针和 `boot_hartid` 地址，启动两个 GDB 并留下完整记录；结束或失败时清理自己启动的调试进程。日志与判断都来自本次执行。

**Case 1：Sv39 访存。** 用目标地址条件断点记录 C 查询的标签、索引、mmu_idx，以及填充时的 satp、权限模式、实际读取的 PTE 和翻译结果。按 PTE 的真实叶子层级解释物理地址，不能把根层大页也写成三次页表读取。

**Case 2：命中与刷新。** 核对三次读取结果和目标数据填充次数。本探针预期两次填充，第二次读取可走生成的宿主快路径；结合 `tcg_out_tlb_read()` 解释，不能要求每次 load 都进入 C 慢路径。GDB 自己查看内存造成的调试翻译，不算成内核多执行了一条 load。

**Case 3：Bare 模式。** 观察开启分页前保存 `boot_hartid` 时 `satp=0` 的访问，记录客体地址、物理地址和 TLB 安装，并观察 addend 或对应宿主页地址。说明客体不查 Sv39 页表时，QEMU 仍需定位宿主 RAM；宿主地址随进程变化，不能当作固定常量。

**Case 4：运行不符合预期。** 调试器退出、连接超时或缺少必要记录时报告失败，保留已有日志供检查，不补造输出或把失败写成通过。

**Requirements**：stub 只监听回环地址；断点行号与地址以当前源码、符号为准。说明实测支持到什么程度，保留可复核的命令与输出。
````
