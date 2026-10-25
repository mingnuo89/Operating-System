# 成员 C：Buddy System 提示词汇总

作者：2410625－吴宇轩  
AI 工具：Codex  
底层模型：GPT-5.6 Sol

## 提示词一：实现并接入 Buddy System

````markdown
[PROMPT]
**任务**：在 Lab2 的 `kern/mm/` 中从零实现 Buddy System 物理页分配模块，并通过现有 `struct pmm_manager` 接入内核。Buddy 以 2 的幂页数管理空闲块；非 2 的幂请求向上取整。新增代码不能替换已有 First-Fit、Best-Fit、物理内存探测或测试代码。

**操作要求**：必须修改项目实际文件。新增 `buddy_pmm.c`、`buddy_pmm.h`，并只在 `pmm.c` 的管理器选择处添加 Buddy 分支。保持默认 Best-Fit，通过 `PMM_BUDDY` 编译宏显式选择 Buddy。不得修改框架公开接口，不得通过修改已有测试规避错误。

**输出要求**：完成真实文件修改、交叉编译和 QEMU 运行。说明空闲块如何按阶组织、任意起点和长度的初始空闲区如何分解、分配如何拆分、释放如何计算伙伴并递归合并，以及空闲页计数为何包含向上取整后的实际页数。

[RELY]
可依赖工程中的真实定义：

```c
struct pmm_manager {
    const char *name;
    void (*init)(void);
    void (*init_memmap)(struct Page *base, size_t n);
    struct Page *(*alloc_pages)(size_t n);
    void (*free_pages)(struct Page *base, size_t n);
    size_t (*nr_free_pages)(void);
    void (*check)(void);
};

extern struct Page *pages;
extern size_t npage;
extern const size_t nbase;
```

还可使用 `PageReserved`、`PageProperty`、`SetPageProperty`、`ClearPageProperty`、`list_entry_t`、`list_init`、`list_add`、`list_del`、`le2page`、`set_page_ref`、`assert` 和现有页面分配包装函数。`Page.property` 可在 Buddy 模块内记录块阶；额外分配标记必须使用不与 `PG_reserved`、`PG_property` 冲突的标志位。

[GUARANTEE]
必须新增或实现：

```c
extern const struct pmm_manager buddy_pmm_manager;

static void buddy_init(void);
static void buddy_init_memmap(struct Page *base, size_t n);
static struct Page *buddy_alloc_pages(size_t n);
static void buddy_free_pages(struct Page *base, size_t n);
static size_t buddy_nr_free_pages(void);
```

允许添加只在模块内部使用的 `static` 辅助函数、分阶空闲链表和计数变量，不改变公开 PMM 接口。

[SPECIFICATION]
## buddy_init
**Pre-Condition**：Buddy 管理器尚未登记空闲区。
**Post-Condition**：所有阶的空闲链表为空，总空闲页数为 0。

## buddy_init_memmap
**Pre-Condition**：`base` 指向连续的 `n > 0` 个保留页描述符；范围位于 `pages` 管理区内。
**Post-Condition**：清理这些描述符的引用和状态，将整个范围无遗漏、无重叠地拆成满足全局页索引对齐的 2 的幂块；总空闲页数增加 `n`。
**Requirements**：初始区域的起点和长度都不保证是 2 的幂，不能把边界外页面加入空闲链表。

## buddy_alloc_pages
**Pre-Condition**：`n > 0`，管理器已初始化。
**Post-Condition**：成功时返回一段至少包含 `n` 页的连续块，实际占用页数为不小于 `n` 的最小 2 的幂；相应空闲页计数按实际页数减少。失败返回 `NULL` 且状态不变。
**Case 1**：目标阶有块时直接取块。
**Case 2**：目标阶为空时，从更高阶取块并逐级二分，把未使用的一半放回对应阶链表。
**Case 3**：请求超过最大阶、总空闲页数或没有可拆分块时返回 `NULL`。

## buddy_free_pages
**Pre-Condition**：`base` 是 Buddy 返回且尚未释放的块头，`n` 与原请求对应。
**Post-Condition**：按向上取整后的实际块大小恢复计数；若同阶伙伴空闲，则移除伙伴、合成高一阶块并继续，直到不能合并或达到最大阶。
**Requirements**：伙伴由全局页描述符索引异或块大小得到；必须检查范围、分配标志和记录的阶，防止重复释放或错误大小破坏链表。

## buddy_nr_free_pages
**Pre-Condition**：管理器已初始化。
**Post-Condition**：返回所有阶空闲块所包含的总页数，不修改任何状态。

## 管理器接入
**Post-Condition**：定义完整的 `buddy_pmm_manager`；`PMM_BUDDY` 配置选择 Buddy，First-Fit 与默认 Best-Fit 仍可构建运行。
````

## 提示词二：设计并运行 Buddy 测试

````markdown
[PROMPT]
**任务**：为刚实现的 Buddy System 设计 `buddy_check()`，通过 `buddy_pmm_manager.check` 在内核启动时执行。测试必须验证算法行为，不能只检查一次申请成功。

**操作要求**：必须修改实际文件。测试可以读取 Buddy 模块内部的分阶链表和计数，但不得修改 First-Fit、Best-Fit、SLUB、官方评分器或被测函数来迎合断言。每组临时分配必须完整释放，使测试结束状态恢复。

**输出要求**：交叉编译，以 `PMM_BUDDY` 启动 QEMU；同时运行现有 SLUB 测试以验证两层分配器兼容性。在默认 128 MiB 之外，再以 64 MiB 和 256 MiB 启动。逐项说明断言覆盖的错误类型和真实输出。

[RELY]
可以调用：

```c
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
```

还可使用 `alloc_page`、`free_page`、`pages`、`npage`、`nbase`、`PageProperty`、链表遍历、`assert` 与 `cprintf`。Buddy 对非 2 的幂请求按 2 的幂向上取整，因此申请 3 页应实际扣除 4 页。

[GUARANTEE]
必须实现并注册：

```c
static void buddy_check(void);
```

[SPECIFICATION]
## buddy_check
**Pre-Condition**：Buddy 已完成空闲区登记，没有测试遗留分配。
**Post-Condition**：全部断言通过；所有临时块被释放；空闲页计数和分阶链表恢复到测试前状态；打印明确成功信息。

**Case 1：向上取整和对齐。** 同时申请 3 页和 8 页，检查地址分别按 4 页和 8 页边界对齐，总空闲页数减少 12；释放时使用原请求大小并检查计数恢复。

**Case 2：拆分、唯一性与乱序合并。** 连续申请 32 个单页，确保地址两两不同；先释放偶数下标，再释放奇数下标，检查伙伴能跨释放顺序最终合并，计数完全恢复。

**Case 3：链表不变量。** 遍历所有阶：块头具有空闲属性、记录阶与链表一致、起点按块大小对齐；同阶的一对伙伴不能同时残留，因为它们应已合并。各链表块大小之和必须等于全局空闲页计数。

**Case 4：失败边界。** 请求超过当前总空闲页数，必须返回 `NULL`，空闲页数不变。

**Requirements**：Buddy 自检后继续运行 SLUB 的尺寸、复用、整页回收、耗尽恢复测试；64/128/256 MiB 三种配置均不得出现 panic 或 assertion failure。
````
