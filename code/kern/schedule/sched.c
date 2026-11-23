#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

/*
 * 中文导读：wakeup_proc
 * 把未初始化或睡眠的线程变为 PROC_RUNNABLE，使调度器能够选中它。
 * 断言拒绝唤醒僵尸和已经可运行的线程；具体执行时间由后续 schedule 决定。
 *
 * 参数 proc 指向已建立资源的目标 PCB，本函数没有返回值，也不会直接跳到目标代码。
 * 创建者调用后，线程状态从 UNINIT 进入 RUNNABLE；以后调度器遍历到它才可能运行。
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
 *
 * 例如表头→A→B→C，当前为 C，则扫描表头、A、B、C，遇到哨兵只绕回，不提前结束。
 * 循环回到 start 才完成一圈，当前线程依旧可运行时可以最后选回自己。
 * next 预置 idle 能覆盖空表及所有普通线程睡眠的情况；runs 增加代表被选次数。
 * 本函数可能随 switch_to 暂停，之后同一线程恢复时，才执行末尾的 local_intr_restore。
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
