#include <riscv.h>

    .macro SAVE_ALL

    csrw sscratch, sp

    addi sp, sp, -36 * REGBYTES
    # save x registers
    STORE x0, 0*REGBYTES(sp)
    STORE x1, 1*REGBYTES(sp)
    STORE x3, 3*REGBYTES(sp)
    STORE x4, 4*REGBYTES(sp)
    STORE x5, 5*REGBYTES(sp)
    STORE x6, 6*REGBYTES(sp)
    STORE x7, 7*REGBYTES(sp)
    STORE x8, 8*REGBYTES(sp)
    STORE x9, 9*REGBYTES(sp)
    STORE x10, 10*REGBYTES(sp)
    STORE x11, 11*REGBYTES(sp)
    STORE x12, 12*REGBYTES(sp)
    STORE x13, 13*REGBYTES(sp)
    STORE x14, 14*REGBYTES(sp)
    STORE x15, 15*REGBYTES(sp)
    STORE x16, 16*REGBYTES(sp)
    STORE x17, 17*REGBYTES(sp)
    STORE x18, 18*REGBYTES(sp)
    STORE x19, 19*REGBYTES(sp)
    STORE x20, 20*REGBYTES(sp)
    STORE x21, 21*REGBYTES(sp)
    STORE x22, 22*REGBYTES(sp)
    STORE x23, 23*REGBYTES(sp)
    STORE x24, 24*REGBYTES(sp)
    STORE x25, 25*REGBYTES(sp)
    STORE x26, 26*REGBYTES(sp)
    STORE x27, 27*REGBYTES(sp)
    STORE x28, 28*REGBYTES(sp)
    STORE x29, 29*REGBYTES(sp)
    STORE x30, 30*REGBYTES(sp)
    STORE x31, 31*REGBYTES(sp)

    # get sr, epc, badvaddr, cause
    # Set sscratch register to 0, so that if a recursive exception
    # occurs, the exception vector knows it came from the kernel
    csrrw s0, sscratch, x0
    csrr s1, sstatus
    csrr s2, sepc
    csrr s3, stval
    csrr s4, scause

    STORE s0, 2*REGBYTES(sp)
    STORE s1, 32*REGBYTES(sp)
    STORE s2, 33*REGBYTES(sp)
    STORE s3, 34*REGBYTES(sp)
    STORE s4, 35*REGBYTES(sp)
    .endm

    .macro RESTORE_ALL

    #lab3 function1 2410878
    # SAVE_ALL 把 4 个 CSR 放在 trapframe 的 32..35 槽位，
    # 先用 s1~s4 读出来写回 CSR，再把 s1~s4 当普通寄存器恢复。
    LOAD s1, 32*REGBYTES(sp)        # sstatus
    LOAD s2, 33*REGBYTES(sp)        # sepc
    LOAD s3, 34*REGBYTES(sp)        # stval
    LOAD s4, 35*REGBYTES(sp)        # scause

    csrw sstatus, s1
    csrw sepc, s2
    csrw stval, s3
    csrw scause, s4

    # 按 SAVE_ALL 的顺序恢复通用寄存器（x0 恒为 0，不用恢复）
    LOAD x1, 1*REGBYTES(sp)
    LOAD x3, 3*REGBYTES(sp)
    LOAD x4, 4*REGBYTES(sp)
    LOAD x5, 5*REGBYTES(sp)
    LOAD x6, 6*REGBYTES(sp)
    LOAD x7, 7*REGBYTES(sp)
    LOAD x8, 8*REGBYTES(sp)
    LOAD x9, 9*REGBYTES(sp)
    LOAD x10, 10*REGBYTES(sp)
    LOAD x11, 11*REGBYTES(sp)
    LOAD x12, 12*REGBYTES(sp)
    LOAD x13, 13*REGBYTES(sp)
    LOAD x14, 14*REGBYTES(sp)
    LOAD x15, 15*REGBYTES(sp)
    LOAD x16, 16*REGBYTES(sp)
    LOAD x17, 17*REGBYTES(sp)
    LOAD x18, 18*REGBYTES(sp)
    LOAD x19, 19*REGBYTES(sp)
    LOAD x20, 20*REGBYTES(sp)
    LOAD x21, 21*REGBYTES(sp)
    LOAD x22, 22*REGBYTES(sp)
    LOAD x23, 23*REGBYTES(sp)
    LOAD x24, 24*REGBYTES(sp)
    LOAD x25, 25*REGBYTES(sp)
    LOAD x26, 26*REGBYTES(sp)
    LOAD x27, 27*REGBYTES(sp)
    LOAD x28, 28*REGBYTES(sp)
    LOAD x29, 29*REGBYTES(sp)
    LOAD x30, 30*REGBYTES(sp)
    LOAD x31, 31*REGBYTES(sp)

    # x2(sp) 需要特殊处理。
    # SAVE_ALL 第一条指令 csrw sscratch, sp 已经把陷入前的 sp 写进 sscratch，
    # 之后的 csrrw s0, sscratch, x0 又把它读出来存到 TrapFrame 第 2 个槽位，
    # 所以 2 号槽里存的是陷入前的 sp（不是 0）。
    # 本实验内核态陷入用的是同一个内核栈，因此直接释放 TrapFrame 占用的空间
    # 就能回到 Trap 发生前的 sp；效果等价于 LOAD x2, 2*REGBYTES(sp)，但 sp
    # 只有在其它通用寄存器都恢复完之后才能动。
    addi sp, sp, 36*REGBYTES

    .endm

    .globl __alltraps
    .align(2)
__alltraps:
    SAVE_ALL

    #lab3 function1 2410878
    # sp should be the same as before "jal trap"
    # 此时 sp 指着 trapframe 基址，正好是 trap() 要的 (struct trapframe *tf)，
    # 按 RISC-V 调用约定放进 a0。jal 覆盖 ra，但 ra 已存进 trapframe，
    # RESTORE_ALL 会把它恢复回来。
    mv a0, sp
    jal trap

    .globl __trapret
__trapret:
    RESTORE_ALL
    # return from supervisor call
    sret
