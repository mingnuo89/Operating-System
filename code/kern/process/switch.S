//LAB4 function3 2410625
#include <riscv.h>

    .text
    .globl switch_to
    .type switch_to, @function
switch_to:
    # 调用示例：switch_to(&A->context, &B->context)，调用时 a0 属于 A，a1 属于 B。
    # STORE ra 保存的继续地址来自本次函数调用，B 将来切回 A 后就能从该地址接着执行。
    # 中文导读：a0/a1 指向旧/新 context；RV64 每槽 8 字节，与 proc.h 严格对应。
    # 全部保存旧现场后才恢复新现场，让旧线程以后能接续这次挂起的调用。
    // a0 points to the outgoing context, a1 to the incoming context.
    STORE ra, 0 * REGBYTES(a0)
    STORE sp, 1 * REGBYTES(a0)
    STORE s0, 2 * REGBYTES(a0)
    STORE s1, 3 * REGBYTES(a0)
    STORE s2, 4 * REGBYTES(a0)
    STORE s3, 5 * REGBYTES(a0)
    STORE s4, 6 * REGBYTES(a0)
    STORE s5, 7 * REGBYTES(a0)
    STORE s6, 8 * REGBYTES(a0)
    STORE s7, 9 * REGBYTES(a0)
    STORE s8, 10 * REGBYTES(a0)
    STORE s9, 11 * REGBYTES(a0)
    STORE s10, 12 * REGBYTES(a0)
    STORE s11, 13 * REGBYTES(a0)

    # ra 决定 ret 的落点，sp 切换内核栈；后续读取仍用 a1 寻址，因此不受换栈影响。
    # 从恢复 sp 开始，当前栈已经属于 B；汇编在这里不建立额外栈帧，避免干扰目标栈布局。
    LOAD ra, 0 * REGBYTES(a1)
    LOAD sp, 1 * REGBYTES(a1)
    LOAD s0, 2 * REGBYTES(a1)
    LOAD s1, 3 * REGBYTES(a1)
    LOAD s2, 4 * REGBYTES(a1)
    LOAD s3, 5 * REGBYTES(a1)
    LOAD s4, 6 * REGBYTES(a1)
    LOAD s5, 7 * REGBYTES(a1)
    LOAD s6, 8 * REGBYTES(a1)
    LOAD s7, 9 * REGBYTES(a1)
    LOAD s8, 10 * REGBYTES(a1)
    LOAD s9, 11 * REGBYTES(a1)
    LOAD s10, 12 * REGBYTES(a1)
    LOAD s11, 13 * REGBYTES(a1)
    # 首次运行返回 forkret，已有线程返回上次 switch_to 调用之后。
    # ret 等价于跳到 ra；它接续的是目标线程的控制流，源线程的调用链留在原栈等待恢复。
    ret
    .size switch_to, . - switch_to
