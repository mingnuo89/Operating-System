#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

/*
 * 中文导读：wakeup_proc
 * 把未初始化或睡眠的线程变为 PROC_RUNNABLE，使调度器能够选中它。
 * 断言拒绝唤醒僵尸和已经可运行的线程；具体执行时间由后续 schedule 决定。
 */
void
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    proc->state = PROC_RUNNABLE;
}

//LAB4 function3 2410625
/*
 * 中文导读：schedule
 * 完成一次线程选择。关中断后清除当前请求，从当前节点的后继扫描；idle 从总链表头之后开始。
 * 跳过哨兵，最多绕一圈，只选择 RUNNABLE；没有候选时回退到 idle。
 * 目标 runs 统计被选次数，包括选回自己；不同目标通过 proc_run 交接，返回时恢复本层中断状态。
 */
void
schedule(void)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    current->need_resched = 0;

    // idle 未挂入普通进程链表，所以以表头为起点；普通线程从自身后继开始轮转。
    list_entry_t *start = current == idleproc ? &proc_list : &current->list_link;
    list_entry_t *entry = start;
    struct proc_struct *next = idleproc;
    do
    {
        entry = list_next(entry);
        if (entry != &proc_list)
        {
            // 表头是哨兵，没有对应 PCB，跳过后才能安全使用 le2proc。
            struct proc_struct *candidate = le2proc(entry, list_link);
            if (candidate->state == PROC_RUNNABLE)
            {
                next = candidate;
                break;
            }
        }
    } while (entry != start);

    next->runs++;
    // 允许选回仍可运行的自己，此时计数但省去实际换栈。
    if (next != current)
    {
        proc_run(next);
    }
    local_intr_restore(intr_flag);
}
