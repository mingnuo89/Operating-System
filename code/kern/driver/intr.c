#include <intr.h>
#include <riscv.h>

/* intr_enable - enable irq interrupt */
/*
 * 中文导读：intr_enable
 * 设置 sstatus 的 SIE 位，允许本 hart 接收已使能且满足条件的监督级中断。
 *
 * 设置总开关后，具体中断源还须在 sie 等寄存器里使能，事件才可能被接收。
 * 用于启动流程或恢复先前开启的临界区，不应在切换现场尚未一致时提前调用。
 */
void intr_enable(void) { set_csr(sstatus, SSTATUS_SIE); }

/* intr_disable - disable irq interrupt */
/*
 * 中文导读：intr_disable
 * 清除 sstatus 的 SIE 位，暂时屏蔽本 hart 的监督级中断，供临界区保护使用。
 *
 * 只清 SIE，不清除各中断源配置，因此后续恢复 SIE 后仍可继续接收对应事件。
 * 与保存旧状态的封装配合使用，避免嵌套代码退出时破坏外层关中断要求。
 */
void intr_disable(void) { clear_csr(sstatus, SSTATUS_SIE); }
