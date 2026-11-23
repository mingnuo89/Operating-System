#include <assert.h>
#include <clock.h>
#include <console.h>
#include <defs.h>
#include <kdebug.h>
#include <memlayout.h>
#include <mmu.h>
#include <riscv.h>
#include <stdio.h>
#include <trap.h>
#include <vmm.h>

#define TICK_NUM 100

/*
 * 中文导读：print_ticks
 * 输出一次时钟统计。DEBUG_GRADE 构建还会触发测试结束 panic，因此其行为受构建选项影响。
 *
 * TICK_NUM=100 表示输出周期，ticks 本身仍记录累计中断次数。
 * DEBUG_GRADE 下会结束执行，阅读运行日志时应同时核对编译选项。
 */
static void print_ticks()
{
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S
 */
/*
 * 中文导读：idt_init
 * 设置 stvec 指向统一汇编入口，清 sscratch，并开启 S 态访问用户页的 SUM 位。
 * 该函数准备 Trap 入口，中断总开关由 intr_enable 单独控制。
 *
 * sscratch 清零表达启动时位于内核的约定，stvec 提供硬件进入汇编的地址。
 * SUM 只影响 S 态访问 U 页条件，页面权限及其他硬件检查仍然生效。
 */
void idt_init(void)
{
    extern void __alltraps(void);
    /* Set sscratch register to 0, indicating to exception vector that we are
     * presently executing in the kernel */
    write_csr(sscratch, 0);
    /* Set the exception vector address */
    write_csr(stvec, &__alltraps);
    /* Allow kernel to access user memory */
    set_csr(sstatus, SSTATUS_SUM);
}

/*
 * 中文导读：print_trapframe
 * 输出 TrapFrame 中的通用寄存器和异常 CSR，帮助定位出错地址、原因及现场。
 *
 * tf 必须指向已保存好的现场；epc 帮助定位出错指令，cause 表示事件类型，badvaddr 提供故障地址。
 * 这里的打印用于诊断，打印完成本身不会修复导致异常的映射。
 */
void print_trapframe(struct trapframe *tf)
{
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%08x\n", tf->status);
    cprintf("  epc      0x%08x\n", tf->epc);
    cprintf("  badvaddr 0x%08x\n", tf->badvaddr);
    cprintf("  cause    0x%08x\n", tf->cause);
}

/*
 * 中文导读：print_regs
 * 逐项打印保存的通用寄存器。参数是 TrapFrame 内 gpr 字段的地址。
 *
 * 按 TrapFrame 的布局逐项读出寄存器，有助于比对传入参数和栈是否正确。
 * 输出保存的 sp 与当前执行打印函数的 sp 可能不同，前者属于陷入前的现场。
 */
void print_regs(struct pushregs *gpr)
{
    cprintf("  zero     0x%08x\n", gpr->zero);
    cprintf("  ra       0x%08x\n", gpr->ra);
    cprintf("  sp       0x%08x\n", gpr->sp);
    cprintf("  gp       0x%08x\n", gpr->gp);
    cprintf("  tp       0x%08x\n", gpr->tp);
    cprintf("  t0       0x%08x\n", gpr->t0);
    cprintf("  t1       0x%08x\n", gpr->t1);
    cprintf("  t2       0x%08x\n", gpr->t2);
    cprintf("  s0       0x%08x\n", gpr->s0);
    cprintf("  s1       0x%08x\n", gpr->s1);
    cprintf("  a0       0x%08x\n", gpr->a0);
    cprintf("  a1       0x%08x\n", gpr->a1);
    cprintf("  a2       0x%08x\n", gpr->a2);
    cprintf("  a3       0x%08x\n", gpr->a3);
    cprintf("  a4       0x%08x\n", gpr->a4);
    cprintf("  a5       0x%08x\n", gpr->a5);
    cprintf("  a6       0x%08x\n", gpr->a6);
    cprintf("  a7       0x%08x\n", gpr->a7);
    cprintf("  s2       0x%08x\n", gpr->s2);
    cprintf("  s3       0x%08x\n", gpr->s3);
    cprintf("  s4       0x%08x\n", gpr->s4);
    cprintf("  s5       0x%08x\n", gpr->s5);
    cprintf("  s6       0x%08x\n", gpr->s6);
    cprintf("  s7       0x%08x\n", gpr->s7);
    cprintf("  s8       0x%08x\n", gpr->s8);
    cprintf("  s9       0x%08x\n", gpr->s9);
    cprintf("  s10      0x%08x\n", gpr->s10);
    cprintf("  s11      0x%08x\n", gpr->s11);
    cprintf("  t3       0x%08x\n", gpr->t3);
    cprintf("  t4       0x%08x\n", gpr->t4);
    cprintf("  t5       0x%08x\n", gpr->t5);
    cprintf("  t6       0x%08x\n", gpr->t6);
}

extern struct mm_struct *check_mm_struct;

//lab3 function2 2410625 (merged from lab3; 适配 lab4：本地 ticks 计数，去掉自动关机)

/*
 * 中文导读：interrupt_handler
 * 从 cause 屏蔽最高中断标志后分类。S 态时钟中断先安排下一事件，再计数并每百次输出。
 * 其他已知中断输出类型，未知编号打印完整现场；这里的时钟分支未实现时间片调度。
 *
 * 最高位表示异步中断，屏蔽后才可与 IRQ_S_TIMER 等小编号比较。
 * 本函数处理时钟时保留 tf->epc，以便统一返回后继续被中断的执行流。
 * 需要连续安排定时事件，单次初始化只负责第一次截止时间。
 */
void interrupt_handler(struct trapframe *tf)
{
    const uintptr_t interrupt_bit = (uintptr_t)1 << (sizeof(uintptr_t) * 8 - 1);
    const uintptr_t cause = tf->cause & ~interrupt_bit;

    switch (cause)
    {
    case IRQ_U_SOFT:  cprintf("User software interrupt\n"); break;
    case IRQ_S_SOFT:  cprintf("Supervisor software interrupt\n"); break;
    case IRQ_H_SOFT:  cprintf("Hypervisor software interrupt\n"); break;
    case IRQ_M_SOFT:  cprintf("Machine software interrupt\n"); break;
    case IRQ_U_TIMER: cprintf("User timer interrupt\n"); break;
    case IRQ_S_TIMER:
        clock_set_next_event();
        if (++ticks % TICK_NUM == 0)
        {
            print_ticks();
        }
        break;
    case IRQ_H_TIMER: cprintf("Hypervisor timer interrupt\n"); break;
    case IRQ_M_TIMER: cprintf("Machine timer interrupt\n"); break;
    case IRQ_U_EXT:   cprintf("User external interrupt\n"); break;
    case IRQ_S_EXT:   cprintf("Supervisor external interrupt\n"); break;
    case IRQ_H_EXT:   cprintf("Hypervisor external interrupt\n"); break;
    case IRQ_M_EXT:   cprintf("Machine external interrupt\n"); break;
    default:
        cprintf("Unknown interrupt: cause %lu\n", (unsigned long)cause);
        print_trapframe(tf);
        break;
    }
}

// lab3 function2 2410849：异常处理
/*
 * 中文导读：exception_handler
 * 处理实验中的非法指令和断点，先打印原地址，再读取指令前半字判定跳过 2 或 4 字节。
 * 读取依赖 epc 地址可访问这一前提；其他异常只打印现场并保留 epc。
 *
 * epc 在内存中是 sepc 的副本，修改它后由 RESTORE_ALL 写回 CSR。
 * 本实验只判断 16/32 位指令：低两位为 11 前进 4 字节，其余前进 2 字节。
 * 缺页等异常通常需要修复后重试原指令，本默认分支只打印，没有实现这种修复。
 */
void exception_handler(struct trapframe *tf)
{
    switch (tf->cause)
    {
    case CAUSE_ILLEGAL_INSTRUCTION:
        cprintf("Illegal instruction at 0x%lx\n", tf->epc);
        break;
    case CAUSE_BREAKPOINT:
        cprintf("Breakpoint at 0x%lx\n", tf->epc);
        break;
    default:
        print_trapframe(tf);
        return;
    }

    // 本实验目标平台使用 16 位压缩指令和 32 位指令
    uint16_t instruction = *(const uint16_t *)tf->epc;
    tf->epc += (instruction & 3) == 3 ? 4 : 2;
}



/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap()
 * returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
/*
 * 中文导读：trap
 * C 层统一分发入口。将 cause 按有符号数解释，最高位为 1 时走中断处理，否则走异常处理。
 * 返回后汇编恢复现场；修改 tf->epc 会影响最终 sret 的返回位置。
 *
 * tf 由入口汇编保存并通过 a0 传入，两条分支都对同一份现场操作。
 * 处理返回后仍需 RESTORE_ALL 与 sret，C 函数返回本身不会完成特权状态恢复。
 */
void trap(struct trapframe *tf)
{
    // dispatch based on what type of trap occurred
    if ((intptr_t)tf->cause < 0)
    {
        // interrupts
        interrupt_handler(tf);
    }
    else
    {
        // exceptions
        exception_handler(tf);
    }
}
