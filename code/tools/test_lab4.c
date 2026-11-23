#include <defs.h>
#include <assert.h>
#include <pmm.h>
#include <proc.h>
#include <sched.h>
#include <stdio.h>

/*
 * 中文导读：lab4_pass
 * 统一打印一个测试的 PASS 标志，评分脚本按这些字符串识别通过结果。
 */
static void lab4_pass(const char *name)
{
    cprintf("[Test %s] PASS\n", name);
}

/*
 * 中文导读：test_lab4_pmm_basic
 * 验证首次映射、覆盖旧页、同页改权限及重复删除，检查引用计数和空闲页数。
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