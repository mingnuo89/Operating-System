#ifndef __KERN_SYNC_SYNC_H__
#define __KERN_SYNC_SYNC_H__

#include <defs.h>
#include <intr.h>
#include <riscv.h>

/*
 * 中文导读：__intr_save
 * 读取本 hart 的 sstatus.SIE。原来开启则先关闭并返回 true，原来关闭则返回 false。
 * 返回值由调用者的栈保存，用于退出当前临界区时恢复原状态。
 *
 * 函数返回的是“进入本层前是否开启”，局部 flag 随所属线程的内核栈保存。
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
 * 中文导读：__intr_restore
 * flag 为 true 才开启监督级中断，false 时维持关闭。
 * 配合保存旧状态支持嵌套临界区；单 hart 的中断屏蔽不提供跨 hart 互斥。
 *
 * 例如 schedule 保存 true，proc_run 保存 false，原线程恢复后先保持关闭，再由外层开启。
 * 线程切出后，本线程的恢复语句暂不执行，要等它重新被选中才接着运行。
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
