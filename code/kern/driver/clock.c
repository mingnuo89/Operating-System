#include <clock.h>
#include <defs.h>
#include <sbi.h>
#include <stdio.h>
#include <riscv.h>

volatile size_t ticks;

/*
 * 中文导读：get_cycles
 * 读取硬件 time 计数器。RV64 一次读取，RV32 重读高半部保证跨低位溢出时结果一致。
 *
 * rdtime 读的是时间基准计数，函数名虽然含 cycles，此处并非读取 rdcycle。
 * RV32 先后两次读高位，若不相等就重试，避免拼接成跨越溢出的错误时间。
 */
static inline uint64_t get_cycles(void) {
#if __riscv_xlen == 64
    uint64_t n;
    __asm__ __volatile__("rdtime %0" : "=r"(n));
    return n;
#else
    uint32_t lo, hi, tmp;
    __asm__ __volatile__(
        "1:\n"
        "rdtimeh %0\n"
        "rdtime %1\n"
        "rdtimeh %2\n"
        "bne %0, %2, 1b"
        : "=&r"(hi), "=&r"(lo), "=&r"(tmp));
    return ((uint64_t)hi << 32) | lo;
#endif
}

static uint64_t timebase;

/* *
 * clock_init - initialize 8253 clock to interrupt 100 times per second,
 * and then enable IRQ_TIMER.
 * */
/*
 * 中文导读：clock_init
 * 按 QEMU 的 10MHz 时基设置约 100Hz 的定时事件，安排首次截止时间并使能时钟中断源。
 * 清零 ticks 后输出初始化信息；本 hart 的 SIE 总开关由启动流程另外开启。
 *
 * timebase=100000 表示每次截止间隔的硬件计数；在假定 10MHz 时基下约为 10ms。
 * 先设置下次事件，再使能监督级定时源并清零软件计数，启动过程还需打开 SIE。
 */
void clock_init(void) {
    // divided by 500 when using Spike(2MHz)
    // divided by 100 when using QEMU(10MHz)
    timebase = 1e7 / 100;
    clock_set_next_event();
    set_csr(sie, MIP_STIP);

    // initialize time counter 'ticks' to zero
    ticks = 0;

    cprintf("++ setup timer interrupts\n");
}

/*
 * 中文导读：clock_set_next_event
 * 用当前硬件时间加 timebase 设置下一次 SBI 定时截止时间。
 * 每次时钟中断重新调用，才能持续收到后续时钟事件。
 *
 * 无返回值，由 sbi_set_timer 向固件请求定时中断。
 * 使用当前时间加间隔，处理耗时会影响连续输出相对于绝对时间的累计节奏。
 */
void clock_set_next_event(void) { sbi_set_timer(get_cycles() + timebase); }
