# Lab1 提示词

## 内存布局、链接脚本与入口点（练习一）

负责人：2410849-张祖浩。本次按讲义规范整理，非历史对话原文。

````markdown
[PROMPT]
**任务**：分析 Lab1 的内存布局、链接脚本和汇编入口，回答练习一，说明内核进入 C 函数前需要满足哪些运行条件。目标文件为 tools/kernel.ld、kern/init/entry.S、kern/init/init.c、kern/mm/mmu.h、kern/mm/memlayout.h。
**操作要求**：只阅读、分析和验证已有框架，不改动内核源码、Makefile、链接脚本或工具配置。以当前工程为准，不按其他 uCore 版本补出不存在的代码。运行验证前说明命令、预期观察点和退出方法；没有实际执行的结果必须明确标为未验证。
**输出要求**：使用下面四段信息，交付源码位置、模块职责、练习答案和可核对的验证依据；区分已有实现、当前观察和未验证的推断。不要生成虚构的截图、提示词历史或迭代次数。

[RELY]
以下定义来自当前工程，路径相对于 code/lab1：
```c
#define PGSIZE 4096
#define PGSHIFT 12
#define KSTACKPAGE 2
#define KSTACKSIZE (KSTACKPAGE * PGSIZE)
int kern_init(void) __attribute__((noreturn));
void *memset(void *s, char c, size_t n);
extern char edata[], end[];
```
```ld
OUTPUT_ARCH(riscv)
ENTRY(kern_entry)
BASE_ADDRESS = 0x80200000;
```
kern_entry 为汇编标签；bootstack 与 bootstacktop 分别标记启动栈的低、高地址边界。当前 entry.S 使用 .text，而非单独的 .text.kern_entry。可用 readelf、nm、objdump 与 GDB 检查生成文件和运行状态，不能将原报告中的符号地址当作以后所有构建的固定值。

[GUARANTEE]
本任务是既有代码分析，不要求实现或修改函数。必须核对以下现有接口及对应行为：
```c
int kern_init(void) __attribute__((noreturn));
void *memset(void *s, char c, size_t n);
```
必须分析的非 C 接口：kern_entry、bootstack、bootstacktop、edata、end，以及 ENTRY 和各段布局。
交付练习一两条伪指令的操作与目的、链接布局检查、进入 C 后栈与清零区间的解释。不新增辅助函数。

[SPECIFICATION]
## kern_entry（汇编入口）
**Pre-Condition**:
- 固件已将执行位置转到内核入口；内核镜像的加载位置与链接约定相符。
- 启动栈空间已由镜像布局预留，此时不能假定 sp 已指向内核自己的栈。
**Post-Condition**:
- sp 指向 bootstacktop，栈向低地址使用，控制权进入 kern_init。
- 入口跳转不为返回汇编入口设置新的返回地址。
**Requirements**:
- 根据当前反汇编区分 la、tail 伪指令与机器指令，不假定一行汇编只对应一次 si。
- 用栈边界差验证空间大小，用跳转前后的 pc、sp、ra 验证入口行为。

## kern_init
**Pre-Condition**:
- 入口已建立可用栈，链接符号 edata、end 可用于确定需清零的区间。
**Post-Condition**:
- 需清零的区间完成初始化，随后输出启动信息，函数不返回而停留在无限循环。
**Case 1**:
- edata 小于 end 时，解释该区间清零与 C 中未显式初始化全局数据的关系；只有实际观察到的变化才可写为实测。
**Case 2**:
- edata 等于 end 时，指出清零长度为零，不声称观察到了非空 BSS 数据被清零。

## memset
**Pre-Condition**:
- s 指向至少 n 字节的可写区域；本次由 kern_init 提供需清零的区间。
**Post-Condition**:
- 前 n 字节被设为指定字节值，返回原始地址 s；n 为零时不改写该区间。
**Requirements**:
- 不将链接符号声明理解为另行分配了两个普通数组；不把链接脚本布局等同于页表或虚拟内存管理。
````

## 从 SBI 到 stdio

负责人：2410878-吕明诺。以下保留报告中的提示词原文。

````markdown
[PROMPT]
**任务**：阅读 kern/libs/stdio.c、kern/driver/console.c、libs/sbi.c、libs/printfmt.c
四个文件，梳理从 sbi_call 到 cprintf 的完整调用链，说明每一层封装解决的问题。
**操作要求**：只阅读和分析，不修改任何代码。所有结论必须以这四个文件的真实代码为准，
若某函数在文件中不存在，必须明确指出，不得根据其他版本推测。
**输出要求**：给出完整调用链（含函数签名）、每层职责、以及 C 语言为什么必须用内联汇编实现。

[RELY]
// 文件路径
libs/sbi.c、libs/sbi.h、kern/driver/console.c、kern/libs/stdio.c、libs/printfmt.c、libs/stdio.h
// 关键系统约束
内核运行在 S 模式（Supervisor），OpenSBI 固件运行在 M 模式（Machine）
内核不能依赖宿主操作系统的运行时库
// 已知的功能号定义（来自 libs/sbi.c）
SBI_SET_TIMER = 0、SBI_CONSOLE_PUTCHAR = 1、SBI_CONSOLE_GETCHAR = 2

[GUARANTEE]
本任务不涉及代码实现，只需输出分析结论。

[SPECIFICATION]
## 调用链分析
**Pre-Condition**:
- 已读取上述四个文件的完整内容
**Post-Condition**:
- 给出一条从 cprintf 到 ecall 的完整调用链，标注每层的函数签名
- 逐层说明该层封装的目的
- 对 sbi_call 的内联汇编逐行解释，包括四条 mv、ecall、
  输出/输入操作数约束、以及 "memory" clobber 的作用
**Case 1**:
- 若某个函数在文件中不存在，输出"该函数不存在"，不得臆测其实现
**Case 2**:
- 若发现讲义描述与实际代码不一致，单独列出差异点
**Requirements**:
- 引用代码时给出真实函数名与签名，不得使用近似的记忆版本
````

## Just make it

负责人：2410878-吕明诺。以下保留报告中的提示词原文。

````markdown
[PROMPT]
**任务**：分析 ucore lab1 的 Makefile 与 tools/function.mk，说明从源码到
bin/ucore.img 的完整构建流程，以及 make qemu 目标具体做了什么。
**操作要求**：只阅读和分析，不修改任何文件。结论必须以真实的 Makefile、
tools/function.mk、tools/kernel.ld 为准。
**输出要求**：给出构建链路各阶段（使用的工具与关键选项）、make 的依赖触发规则、
make qemu 各参数的作用、以及 function.mk 中宏函数的分工。

[RELY]
// 文件路径
Makefile、tools/function.mk、tools/kernel.ld
// 关键变量（来自 Makefile）
GCCPREFIX := riscv64-unknown-elf-
QEMU      := qemu-system-riscv64
OBJDIR    := obj
BINDIR    := bin
UCOREIMG  := $(call totarget,ucore.img)
// 关键系统约束
内核入口必须位于物理地址 0x80200000（由 kernel.ld 与 QEMU 加载参数共同保证）
内核不能依赖标准库，编译时使用 -nostdlib -nostdinc

[GUARANTEE]
本任务不涉及代码实现，只需输出分析结论。

[SPECIFICATION]
## 构建流程分析
**Pre-Condition**:
- 已读取 Makefile、tools/function.mk、tools/kernel.ld 的完整内容
**Post-Condition**:
- 给出完整构建链路，标注每一阶段使用的工具与关键编译/链接选项
- 说明 make 的依赖触发规则（目标与依赖的时间戳比较）
- 逐项说明 make qemu 目标中每个参数的作用
**Case 1**:
- 若 Makefile 中的变量、目标或链接脚本与讲义描述不一致，必须单独指出差异
**Requirements**:
- 严格区分"讲义中的写法"与"实际代码中的写法"，不得混为一谈
````
