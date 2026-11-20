# Lab 4 项目结构

- `kern/process/` 管理进程控制块、内核线程入口和寄存器现场。模块三在 `proc.c` 和 `switch.S` 实现空闲循环及上下文切换。
- `kern/schedule/sched.c` 实现唤醒和调度选择。`kern/sync/sync.h` 提供本 hart 的中断状态保护。
- `kern/mm/`、`kern/trap/` 和 `libs/` 提供内存、Trap 与底层运行支持。完整内核构建依赖小组补齐课程框架中的实验空位。
- `Makefile` 和 `tools/` 提供 RISC-V 构建及课程评分。完成小组整合后，在本目录运行 `make grade`。
- `report.md` 记录模块三、Challenge 1、验证和 AI 协作过程。
