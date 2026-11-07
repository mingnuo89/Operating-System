# 练习一：Trap 上下文恢复与 Trap 入口功能

## [PROMPT]

**任务：**

请修改 `kern/trap/trapentry.S` 文件，完成 Trap 上下文恢复与 Trap 入口功能，主要包括 `RESTORE_ALL` 和 `__alltraps` 两部分。

当 CPU 发生中断或异常时，已有的 `SAVE_ALL` 会将寄存器状态保存到栈上的 TrapFrame 中。需要在此基础上实现上下文恢复，并建立汇编入口与 C 语言 `trap()` 函数之间的调用关系，使系统能够正确完成 Trap 的进入、处理和返回过程。

**操作要求：**

请直接修改项目中的实际文件，在标有 `lab3 function1 YOUR CODE` 的位置补充代码。保留已有的 `SAVE_ALL`、`__trapret` 等代码，不要修改 TrapFrame 的结构或现有函数接口，也不要增加与本次练习无关的功能。

**输出要求：**

根据下面提供的已有接口和功能要求完成代码，尽量保持原有代码风格。完成后简要说明新增代码的作用，重点解释寄存器恢复、栈指针处理和 TrapFrame 参数传递的过程。

## [RELY]

本功能可以依赖以下已有代码、宏定义和数据结构：

**1. SAVE_ALL 宏**

`SAVE_ALL` 已经实现了上下文保存功能，会在当前栈上分配 36 个寄存器大小的空间，并按照固定顺序保存 32 个通用寄存器和 4 个 CSR 的值。

其中，原来的 `sp` 会先通过 `sscratch` 临时保存，再写入 TrapFrame 的第 2 个位置。`sstatus`、`sepc`、`sb adaddr`（实际代码名称为 `sbadaddr`）和 `scause` 分别保存在第 32～35 个位置。

**2. TrapFrame 数据结构**

`kern/trap/trap.h` 中已经定义了 `struct trapframe`，其中包含保存通用寄存器的 `struct pushregs gpr`，以及 `status`、`epc`、`badvaddr`、`cause` 四个字段。

这些字段的顺序与 `SAVE_ALL` 的保存布局一致，恢复上下文时需要遵循相同的布局。

**3. 已有宏和接口**

- `REGBYTES`：表示当前架构下一个寄存器占用的字节数。
- `LOAD`、`STORE`：用于通用寄存器与内存之间的数据读取和保存。
- `csrr`、`csrw`：用于读取和写入 CSR。
- `trap(struct trapframe *tf)`：已有的 C 语言 Trap 处理接口。
- `__trapret`：已有的 Trap 返回入口，负责执行上下文恢复和 `sret`。

另外，可以依赖 RISC-V 的函数调用约定，其中第一个函数参数通过 `a0` 传递。

## [GUARANTEE]

必须在 `kern/trap/trapentry.S` 中完成以下两个功能：

**1. RESTORE_ALL**

实现与现有 `SAVE_ALL` 对应的汇编宏，负责从 TrapFrame 中恢复 CPU 的寄存器状态，使处理器能够恢复到 Trap 发生前的执行环境。

**2. __alltraps**

补全已有的 `__alltraps` 入口，使其能够在保存上下文后，正确调用 C 语言的 `trap(struct trapframe *tf)` 函数，并在处理完成后进入统一的 Trap 返回流程。

两个功能需要相互配合，形成完整的 Trap 进入、上下文保存、C 语言处理、上下文恢复和返回流程。

## [SPECIFICATION]

### 1. RESTORE_ALL

**Pre-Condition（前置条件）：**

- CPU 已经通过 `SAVE_ALL` 将上下文保存到当前栈上的 TrapFrame 中。
- 当前 `sp` 指向 TrapFrame 的起始地址。
- TrapFrame 的布局与 `struct trapframe` 定义保持一致。
- C 语言的 Trap 处理程序已经执行完成，准备恢复原来的执行状态。

**Post-Condition（后置条件）：**

- 原来保存的通用寄存器能够被正确恢复。
- `sstatus` 和 `sepc` 等返回所需的处理器状态得到正确恢复。
- 栈指针恢复到 Trap 发生前的位置。
- 恢复过程不会破坏仍然需要读取的 TrapFrame 数据。
- 完成恢复后，处理器能够继续执行 `sret`，返回正确的执行位置。

**Case 1：普通寄存器恢复**

对于已经保存在 TrapFrame 中的通用寄存器，需要根据 `SAVE_ALL` 的保存布局恢复原来的值。恢复完成后，寄存器内容应与 Trap 发生前保持一致，其中 `x0` 为恒零寄存器，不需要实际恢复。

**Case 2：栈指针恢复**

`sp` 在 Trap 处理过程中既用于定位 TrapFrame，又是需要恢复的寄存器，因此必须单独考虑。恢复时不能提前破坏当前栈指针，应该保证其他寄存器的数据读取完成后，栈指针能够正确回到 Trap 发生前的位置。

**Case 3：CSR 状态恢复**

需要恢复 Trap 返回所依赖的处理器状态，特别是 `sstatus` 和 `sepc`，保证中断使能状态、原特权级信息以及返回地址正确，使后续 `sret` 能够正常执行。

**Requirements（其他要求）：**

- 恢复布局必须与已有的 `SAVE_ALL` 完全对应。
- 不允许修改 `SAVE_ALL` 和 `struct trapframe` 的定义。
- 注意寄存器恢复的先后顺序，避免覆盖仍在使用的临时数据。
- 不要在恢复完成后留下额外的栈空间占用。

### 2. __alltraps

**Pre-Condition（前置条件）：**

- CPU 已经发生 Trap，并通过 `stvec` 跳转到 `__alltraps`。
- 已有的 `SAVE_ALL` 可以将当前上下文保存为 TrapFrame。
- C 语言中的 `trap(struct trapframe *tf)` 函数已经定义，可以直接调用。

**Post-Condition（后置条件）：**

- Trap 发生时的上下文能够完整保存到 TrapFrame。
- C 语言的 `trap()` 函数能够接收到正确的 TrapFrame 指针。
- `trap()` 可以读取和处理当前的中断或异常信息。
- `trap()` 返回后，程序能够继续进入 `__trapret`，执行上下文恢复并通过 `sret` 返回。
- 整个过程不会丢失原来的寄存器状态。

**Case 1：Trap 入口处理**

当 CPU 进入 `__alltraps` 时，需要利用已有的 `SAVE_ALL` 保存当前上下文，并保证保存后的 TrapFrame 能够被后续处理函数访问。

**Case 2：调用 C 语言处理函数**

上下文保存完成后，需要按照 RISC-V 函数调用约定，将当前 TrapFrame 的地址作为参数传递给 `trap()`。调用过程中即使使用了参数寄存器或返回地址寄存器，也不能影响原来保存的执行状态。

**Case 3：Trap 处理完成后返回**

当 `trap()` 处理完成并返回时，程序需要进入已有的 `__trapret`，通过 `RESTORE_ALL` 恢复上下文，最后执行 `sret` 返回原来的执行流。

**Requirements（其他要求）：**

- 保持 `__alltraps` 和 `__trapret` 原有的入口结构。
- 遵守 RISC-V 函数调用约定，正确传递 TrapFrame 指针。
- 调用 `trap()` 前后应保证栈指针与 TrapFrame 的关系正确。
- 不修改 `trap()` 的函数签名，也不额外增加不必要的处理流程。