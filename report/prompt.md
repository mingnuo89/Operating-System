# Lab1 提示词

## 内存布局、链接脚本与入口点（练习一）

负责人：2410849-张祖浩。

````markdown
[PROMPT]
**任务**：分析 Lab1 的内存布局、链接脚本和汇编入口，回答练习一，说明内核进入 C 函数前需要满足哪些运行条件。目标文件为 tools/kernel.ld、kern/init/entry.S、kern/init/init.c、kern/mm/mmu.h、kern/mm/memlayout.h。
**操作要求**：只阅读、分析和验证已有框架，不改动内核源码、Makefile、链接脚本或工具配置。以当前工程为准，不按其他 uCore 版本补出不存在的代码。运行验证前说明命令、预期观察点和退出方法；没有实际执行的结果必须明确标为未验证。
**输出要求**：使用下面四段信息，交付源码位置、模块职责、练习答案和可核对的验证依据；区分已有实现、当前观察和未验证的推断。不要生成虚构的截图、提示词历史或迭代次数。

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
## 练习二，OpenSBI, bin, elf

负责人：2410625-吴宇轩。以下保留报告中的提示词原文。

````markdown
 吴宇轩：Lab1 提示词汇总


````markdown
[PROMPT]
**任务**：阅读 Lab1 工程中的 `Makefile`、`tools/kernel.ld`、`kern/init/entry.S`、`kern/init/init.c`、`libs/sbi.c`、`kern/libs/stdio.c` 和 `kern/driver/console.c`，分析最小 RISC-V 内核从构建到启动并输出信息的完整逻辑。

**操作要求**：必须以工程中的真实代码为依据，不凭记忆虚构宏、符号或调用关系。本任务不修改内核源代码，只整理阅读结果。阅读问题和练习二的 GDB 调试记录必须分开。

**输出要求**：说明项目整体逻辑、核心文件职责、三个关键启动地址、内核栈建立、BSS 清零、SBI 输出链、与 OS 原理的对应关系，以及实验尚未覆盖的重要 OS 知识。

[RELY]
链接脚本中的定义：
```ld
OUTPUT_ARCH(riscv)
ENTRY(kern_entry)
BASE_ADDRESS = 0x80200000;
```

内核汇编入口：
```asm
.globl kern_entry
kern_entry:
    la sp, bootstacktop
    tail kern_init
```

C 语言入口与主要操作：
```c
int kern_init(void) __attribute__((noreturn));
memset(edata, 0, end - edata);
cprintf("%s\n\n", message);
```

SBI 接口：
```c
uint64_t sbi_call(uint64_t sbi_type, uint64_t arg0,
                  uint64_t arg1, uint64_t arg2);
void sbi_console_putchar(unsigned char ch);
```

关键地址：
- QEMU MROM 复位地址：`0x1000`
- OpenSBI 固件入口：`0x80000000`
- ucore 内核入口：`0x80200000`

[GUARANTEE]
本任务不新增或修改 C 接口。必须准确分析：
```c
kern_entry
int kern_init(void) __attribute__((noreturn));
uint64_t sbi_call(uint64_t sbi_type, uint64_t arg0,
                  uint64_t arg1, uint64_t arg2);
void sbi_console_putchar(unsigned char ch);
```

必须交付：
- 项目构建与链接过程；
- `kernel.ld` 对入口和段布局的作用；
- `entry.S` 建立栈并进入 C 代码的过程；
- `kern_init()` 清零 BSS 和输出信息的作用；
- SBI 控制台输出调用链；
- 完整启动执行流；
- 实验内容与 OS 原理的对应；
- 实验未覆盖的重要 OS 知识。

[SPECIFICATION]
## 构建与链接分析
**Pre-Condition**:
- Lab1 源码可读取。
- 以实际 Makefile 和链接脚本为准。

**Post-Condition**:
- 解释源文件如何生成目标文件、ELF 内核和原始二进制镜像。
- 区分 `bin/kernel` 与 `bin/ucore.img` 的用途。

## 启动入口分析
**Pre-Condition**:
- 已读取 `kernel.ld`、`entry.S` 和 `init.c`。

**Post-Condition**:
- 说明 `0x1000`、`0x80000000` 和 `0x80200000` 对应的阶段。
- 解释 `la sp, bootstacktop` 和 `tail kern_init` 的目的。
- 说明 BSS 清零与 C 语言运行环境的关系。

## SBI 输出分析
**Pre-Condition**:
- 已读取 `stdio.c`、`console.c` 和 `sbi.c`。

**Post-Condition**:
- 给出从 `cprintf()` 到 OpenSBI 和串口的调用链。
- 说明 S-mode 内核通过 `ecall` 请求 M-mode 固件服务的权限关系。

**Requirements**:
- 不混淆 QEMU、OpenSBI 和内核的职责。
- 不混淆复位地址、固件入口和内核入口。
- 阅读回答独立成节，不混入 GDB 操作过程。

## 提示词二：使用 GDB 验证 RISC-V 启动流程，教会我gdb相关命令

````markdown
[PROMPT]
**任务**：在 Ubuntu WSL 中编译 Lab1，使用 QEMU 和 GDB 跟踪 RISC-V 从 `0x1000` 复位地址开始，经过 `0x80000000` 的 OpenSBI，最终进入 `0x80200000` 内核入口的过程；验证内核栈建立和 `kern_init()` 跳转。

**操作要求**：必须实际执行编译和调试，不能只根据理论编写答案。保留现有源代码；如果当前 QEMU 或 GDB 与原始 Makefile 不兼容，只对 `qemu`、`debug` 和 `gdb` 目标做最小必要调整。保存可复现的 GDB 命令和原始输出。

**输出要求**：直接操作 `/home/siamese/oslab/lab1`。交付可运行的实验副本、GDB 调试脚本、调试输出和练习二答案。回答复位地址在哪里、最初指令完成什么、如何证明控制权进入内核。

[RELY]
实验环境：
- Ubuntu 24.04（WSL2）
- QEMU 8.2.2
- `riscv64-unknown-elf-gcc` 13.2.0
- `gdb-multiarch` 15.1

已有构建目标：
```make
qemu:
debug:
gdb:
```

构建产物：
- `bin/kernel`：带符号和调试信息的 ELF 文件；
- `bin/ucore.img`：QEMU 加载的内核镜像。

关键 GDB 命令：
```gdb
set architecture riscv:rv64
target remote localhost:1234
info registers pc
x/8i $pc
si
break *0x80200000
continue
info registers pc sp
```

关键符号：
```text
0x80200000  kern_entry
0x8020000a  kern_init
0x80201000  bootstack
0x80203000  bootstacktop
```

[GUARANTEE]
本任务不新增内核 C 函数。必须保证以下入口可用：
```make
qemu
debug
gdb
```

必须生成或更新：
```text
bin/kernel
bin/ucore.img
gdb_lab1.cmd
gdb-trace.txt
```

必须验证：
- GDB 连接后 `pc=0x1000`；
- 执行复位代码后 `pc=0x80000000`；
- `0x80200000` 断点命中 `kern_entry`；
- 执行 `la sp, bootstacktop` 后 `sp=0x80203000`；
- 执行 `tail kern_init` 后 `pc=0x8020000a`；
- QEMU 输出 `(THU.CST) os is loading ...`。

[SPECIFICATION]
## 构建与入口检查
**Pre-Condition**:
- 已进入 Lab1 工程目录。
- 交叉编译器和 QEMU 可用。

**Post-Condition**:
- `make clean && make` 成功。
- `bin/kernel` 和 `bin/ucore.img` 已生成。
- `readelf` 显示入口地址为 `0x80200000`。

## 复位代码跟踪
**Pre-Condition**:
- QEMU 使用 `-s -S` 启动。
- GDB 已连接端口 1234。

**Post-Condition**:
- 记录 `pc=0x1000` 和复位指令。
- 单步后记录 `pc=0x80000000` 以及 `a0`、`a1`、`a2`、`t0`。

  **Case 1**:
  - 当前 PC 为 `0x1000` 时，执行六次 `si`，验证进入 OpenSBI。

  **Case 2**:
  - 当前 PC 不是 `0x1000` 时，检查 `-S` 和调试端口，不得伪造结果。

## 内核入口验证
**Pre-Condition**:
- CPU 已进入 OpenSBI。
- ELF 符号已载入 GDB。

**Post-Condition**:
- `break *0x80200000` 命中 `kern_entry`。
- 记录入口反汇编、初始 `sp`、栈切换后的 `sp` 和进入 `kern_init()` 后的 PC。

## 环境兼容处理
**Pre-Condition**:
- 原始 Makefile 在当前环境中无法正常启动或连接。

**Post-Condition**:
- 只进行最小兼容修改并记录原因。

  **Case 1**:
  - `-device loader` 导致 OpenSBI 的 `Next Address` 为 `0x0` 时，使用 `-kernel bin/ucore.img`。

  **Case 2**:
  - `riscv64-unknown-elf-gdb` 不存在时，使用 `gdb-multiarch` 并设置 `riscv:rv64`。

**Requirements**:
- 结论必须来自实际运行。
- `watch *0x80200000` 无法观察启动前预加载时，应如实说明，并用 ELF 入口、OpenSBI `Next Address` 和入口断点形成证据链。



## 完整性检查
**Pre-Condition**:
- Markdown 更新完成。

**Post-Condition**:
- 报告图片引用数和 `pic` 目录图片数均为15。
- Windows 和 WSL 副本一致。

**Requirements**:
- 不使用绝对图片路径。
- 实验截图只放入练习二，不插入阅读回答。
````

````
