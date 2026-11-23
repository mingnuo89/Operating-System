#include <defs.h>
#include <assert.h>
#include <pmm.h>
#include <proc.h>
#include <sched.h>
#include <stdio.h>

/*
 * 中文导读：lab4_pass
 * 统一打印一个测试的 PASS 标志，评分脚本按这些字符串识别通过结果。
 *
 * name 是测试名称，输出格式统一为 [Test 名称] PASS。
 * 调用发生在断言之后，只有前面断言全部通过才会打印该标志。
 */
static void lab4_pass(const char *name)
{
    cprintf("[Test %s] PASS\n", name);
}

/*
 * 中文导读：test_lab4_pmm_basic
 * 验证首次映射、覆盖旧页、同页改权限及重复删除，检查引用计数和空闲页数。
 *
 * 三个物理页分别覆盖新映射、替换和同页更新，删除后断言引用数归零。
 * free_before 用于比较同页改权限前后的空闲数，避免误释放再分配导致账目变化。
 */
void test_lab4_pmm_basic(void)
{
    struct Page *p1 = alloc_page();
    struct Page *p2 = alloc_page();
    struct Page *p3 = alloc_page();
    pte_t *ptep;
    size_t free_before;

    assert(p1 != NULL && p2 != NULL && p3 != NULL);

    assert(page_insert(boot_pgdir_va, p1, 0, 0) == 0);
    assert((ptep = get_pte(boot_pgdir_va, 0, 0)) != NULL);
    assert((*ptep & PTE_V) && pte2page(*ptep) == p1 && page_ref(p1) == 1);

    assert(page_insert(boot_pgdir_va, p2, PGSIZE, PTE_U | PTE_W) == 0);
    assert(page_insert(boot_pgdir_va, p1, PGSIZE, 0) == 0);
    assert(page_ref(p1) == 2 && page_ref(p2) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(pte2page(*ptep) == p1 && (*ptep & (PTE_U | PTE_W)) == 0);

    assert(page_insert(boot_pgdir_va, p3, 2 * PGSIZE, 0) == 0);
    free_before = nr_free_pages();
    assert(page_insert(boot_pgdir_va, p3, 2 * PGSIZE, PTE_W) == 0);
    assert(page_ref(p3) == 1 && nr_free_pages() == free_before);

    page_remove(boot_pgdir_va, 0);
    page_remove(boot_pgdir_va, PGSIZE);
    page_remove(boot_pgdir_va, 2 * PGSIZE);
    page_remove(boot_pgdir_va, 2 * PGSIZE);
    assert(page_ref(p1) == 0 && page_ref(p2) == 0 && page_ref(p3) == 0);

    lab4_pass("PMM");
}

/*
 * 中文导读：test_lab4_process_create
 * 在首次调度前检查 idle/init 的 PID、状态、亲缘关系、栈和初始上下文。
 *
 * 测试在 proc_init 结束处运行，此时 current 应仍为 idle，init 已建立但尚未执行。
 * init 的 context.ra/sp 非零说明创建过程已经布置首次启动所需入口与栈。
 */
void test_lab4_process_create(void)
{
    assert(current == idleproc);
    assert(idleproc != NULL && idleproc->pid == 0);
    assert(idleproc->state == PROC_RUNNABLE);
    assert(idleproc->kstack == (uintptr_t)bootstack);
    assert(idleproc->parent == NULL && idleproc->mm == NULL);
    assert(idleproc->tf == NULL && idleproc->runs == 0);
    assert(idleproc->flags == 0 && idleproc->context.ra == 0);

    assert(initproc != NULL && initproc->pid == 1);
    assert(initproc->state == PROC_RUNNABLE);
    assert(initproc->parent == idleproc && initproc->mm == NULL);
    assert(initproc->context.ra != 0 && initproc->context.sp != 0);

    lab4_pass("Process Create");
}

static volatile int test_thread1_ran;
static volatile int test_thread2_ran;

/*
 * 中文导读：test_lab4_thread
 * 用 arg 区分两个工作线程，先设置各自运行标志，然后反复主动调用 schedule。
 * 永久循环避免进入框架 do_exit，让其他线程能够继续获得调度机会。
 *
 * 两个线程通过各自的 id 更新独立运行标志，这些标志用于证明它们真正获得过 CPU。
 * 显式设置 need_resched 后调度，构成主动让出的用例，不依赖时钟时间片。
 */
static void test_lab4_thread(void *arg)
{
    int id = (int)(intptr_t)arg;
    int i;
    for (i = 0; i < 3; i++)
    {
        if (id == 1) test_thread1_ran = 1;
        else test_thread2_ran = 1;
        current->need_resched = 1;
        schedule();
    }
    while (1)
    {
        current->need_resched = 1;
        schedule();
    }
}

/*
 * 中文导读：test_lab4_schedule
 * 创建两个工作线程并让 init 多次让出 CPU，最后断言两者均运行过。
 * init 能执行最终断言也说明其先前挂起的调度调用能够恢复。
 *
 * init 自身调用 schedule 八次，让工作线程交替获得执行机会。
 * 最终断言同时检查两个运行标志；init 执行到断言也验证它能从挂起的调度调用恢复。
 */
void test_lab4_schedule(void)
{
    int i;
    test_thread1_ran = test_thread2_ran = 0;
    assert(kernel_thread(test_lab4_thread, (void *)(intptr_t)1, 0) > 0);
    assert(kernel_thread(test_lab4_thread, (void *)(intptr_t)2, 0) > 0);

    for (i = 0; i < 8; i++)
    {
        current->need_resched = 1;
        schedule();
    }
    assert(test_thread1_ran && test_thread2_ran);
    lab4_pass("Schedule");
}