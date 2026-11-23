#include <intr.h>
#include <riscv.h>

/* intr_enable - enable irq interrupt */
/*
 * 中文导读：intr_enable
 * 设置 sstatus 的 SIE 位，允许本 hart 接收已使能且满足条件的监督级中断。
 */
void intr_enable(void) { set_csr(sstatus, SSTATUS_SIE); }

/* intr_disable - disable irq interrupt */
/*
 * 中文导读：intr_disable
 * 清除 sstatus 的 SIE 位，暂时屏蔽本 hart 的监督级中断，供临界区保护使用。
 */
void intr_disable(void) { clear_csr(sstatus, SSTATUS_SIE); }
