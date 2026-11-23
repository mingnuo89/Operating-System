#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <console.h>
#include <kdebug.h>
#include <picirq.h>
#include <trap.h>
#include <clock.h>
#include <intr.h>
#include <pmm.h>
#include <vmm.h>
#include <proc.h>
#include <kmonitor.h>
#include <dtb.h>

int kern_init(void) __attribute__((noreturn));
void grade_backtrace(void);

/*
 * 中文导读：kern_init
 * 内核 C 入口，先清 BSS，再初始化设备树、输出、内存和 Trap 支持。
 * proc_init 建立 idle/init 后设置时钟并开启中断，最后进入 cpu_idle，后续由调度器交接执行流。
 */
int kern_init(void)
{
    extern char edata[], end[];
    memset(edata, 0, end - edata);
    dtb_init();
    cons_init(); // init the console

    const char *message = "(THU.CST) os is loading ...";
    cprintf("%s\n\n", message);

    print_kerninfo();

    // grade_backtrace();

    pmm_init(); // init physical memory management

    pic_init(); // init interrupt controller
    idt_init(); // init interrupt descriptor table

    vmm_init();  // init virtual memory management
    proc_init(); // init process table

    clock_init();  // init clock interrupt
    intr_enable(); // enable irq interrupt

    cpu_idle(); // run idle process
}

static void
lab1_print_cur_status(void)
{
    static int round = 0;
    round++;
}
