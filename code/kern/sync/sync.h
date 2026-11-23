#ifndef __KERN_SYNC_SYNC_H__
#define __KERN_SYNC_SYNC_H__

#include <defs.h>
#include <intr.h>
#include <riscv.h>

/*
 * __intr_save 的实现说明
 * 这个函数负责保存进入临界区前的中断状态。我会读取 SIE，只有原来开启时才关闭，并把原状态返回给调用者。
 * 读取本 hart 的 sstatus.SIE。原来开启则先关闭并返回 true，原来关闭则返回 false。
 *
 * 具体来说，返回值由调用者的栈保存，用于退出当前临界区时恢复原状态。
 *
 * 顺着代码往下看，函数返回的是“进入本层前是否开启”，局部 flag 随所属线程的内核栈保存。
 * 外层已关中断时内层得到 false，因此内层退出不会提前把中断打开。
 */
static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        intr_disable();
        return 1;
    }
    return 0;
}

/*
 * __intr_restore 的实现说明
 * 这个函数负责恢复本层临界区进入前的中断状态。我会依据之前保存的标志决定是否开启 SIE，从而支持嵌套保护。
 * flag 为 true 才开启监督级中断，false 时维持关闭。
 *
 * 具体来说，配合保存旧状态支持嵌套临界区；单 hart 的中断屏蔽不提供跨 hart 互斥。
 *
 * 顺着代码往下看，例如 schedule 保存 true，proc_run 保存 false，原线程恢复后先保持关闭，再由外层开启。
 * 线程切出后，本线程的恢复语句暂不执行，要等它重新被选中才接着运行。
 *
 * 关中断只保护当前 hart。若系统扩展到多核，其他 hart 仍可运行，因此还要用锁等机制保护共享调度数据，不能把这段局部保护直接推广成跨核互斥。
 */
static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();
    }
}

#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
#define local_intr_restore(x) __intr_restore(x);

#endif /* !__KERN_SYNC_SYNC_H__ */
