#ifndef __KERN_PROCESS_PROC_H__
#define __KERN_PROCESS_PROC_H__

#include <defs.h>
#include <list.h>
#include <trap.h>
#include <memlayout.h>

// process's state in his life cycle
enum proc_state
{
    // 创建时 UNINIT，资源准备后 wakeup_proc 设为 RUNNABLE。
    // 等待资源的线程可设为 SLEEPING；结束且等待回收时用 ZOMBIE，当前 do_exit 尚未完成此流程。
    // 本框架没有单独的 RUNNING 枚举，当前实际运行者由 current 指针区分。
    PROC_UNINIT = 0, // uninitialized
    PROC_SLEEPING,   // sleeping
    PROC_RUNNABLE,   // runnable(maybe running)
    PROC_ZOMBIE,     // almost dead, and wait parent proc to reclaim his resource
};

struct context
{
    // RV64 下 14 个字段共 112 字节：ra 在 0，sp 在 8，s0 在 16，s11 在 104。
    // 通过普通函数调用进入 switch_to，编译器负责保存调用方仍需保留的临时值。
    // 跨调用必须保留的 s 寄存器连同 sp/ra 由汇编保存，线程的其余局部变量随内核栈保留。
    // 普通调用边界的切换现场，与 switch.S 槽位严格对应。
    // ra 记录接续位置，sp 记录内核调用栈，s0~s11 按 ABI 跨调用保留。
    uintptr_t ra;
    uintptr_t sp;
    uintptr_t s0;
    uintptr_t s1;
    uintptr_t s2;
    uintptr_t s3;
    uintptr_t s4;
    uintptr_t s5;
    uintptr_t s6;
    uintptr_t s7;
    uintptr_t s8;
    uintptr_t s9;
    uintptr_t s10;
    uintptr_t s11;
};

#define PROC_NAME_LEN 15
#define MAX_PROCESS 4096
#define MAX_PID (MAX_PROCESS * 2)

extern list_entry_t proc_list;

struct proc_struct
{
    // 阅读顺序建议：先看身份 pid/parent，再看状态 state/need_resched，最后看执行现场。
    // kstack 是栈的低地址基址，context.sp 是切换恢复位置，tf 是 TrapFrame 地址，含义各不相同。
    // mm 管理用户虚拟内存区域，本实验内核线程为 NULL；pgdir 是硬件需要的根页表物理地址。
    // list_link 用于总链表上的调度遍历，hash_link 用于按 PID 查找，两个节点可同时挂接。
    // PCB 保存线程管理信息；运行者也处于 RUNNABLE，current 标识实际运行者。
    // context 用于常规切换，tf 用于 Trap 和首次启动，内核线程共用页表且各有内核栈。
    enum proc_state state;        // Process state
    int pid;                      // Process ID
    int runs;                     // the running times of Proces
    uintptr_t kstack;             // Process kernel stack
    volatile bool need_resched;   // bool value: need to be rescheduled to release CPU?
    struct proc_struct *parent;   // the parent process
    struct mm_struct *mm;         // Process's memory management field
    struct context context;       // Switch here to run process
    struct trapframe *tf;         // Trap frame for current interrupt
    uintptr_t pgdir;              // the base addr of Page Directroy Table(PDT)
    uint32_t flags;               // Process flag
    char name[PROC_NAME_LEN + 1]; // Process name
    list_entry_t list_link;       // Process link list
    list_entry_t hash_link;       // Process hash list
};

#define le2proc(le, member) \
    to_struct((le), struct proc_struct, member)

extern struct proc_struct *idleproc, *initproc, *current;

void proc_init(void);
void proc_run(struct proc_struct *proc);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);

char *set_proc_name(struct proc_struct *proc, const char *name);
char *get_proc_name(struct proc_struct *proc);
void cpu_idle(void) __attribute__((noreturn));

struct proc_struct *find_proc(int pid);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
int do_exit(int error_code);

#endif /* !__KERN_PROCESS_PROC_H__ */
