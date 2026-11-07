# 组员 B：中断处理与时钟中断

## [PROMPT]

根据项目已有 TrapFrame、时钟驱动和评分脚本，实现 code/kern/trap/trap.c 中的 interrupt_handler。保留 lab3 注释并用学号 2410625 替换 YOUR CODE。保留 A 的 Trap 入口和恢复流程以及 C 的异常处理。优先复用已有 clock_set_next_event、print_ticks、sbi_shutdown 和 print_trapframe。先核对输出字符串，再运行课程评分和普通启动，分别验证持续计数和关机。

## [RELY]

- Trap 分发器用 cause 最高位识别中断，TrapFrame 布局与汇编保存布局一致。
- clock_init 将 ticks 清零并安排首次事件，clock_set_next_event 根据 rdtime 计算下一截止时间。
- riscv.h 提供 IRQ 编号，sbi.h 提供关机接口。评测还直接构造了软件中断和编号 15 的未知原因。
- 当前实验在单 hart 环境验证，处理器进入 Trap 时屏蔽同级中断。

## [GUARANTEE]

- 仅屏蔽原因最高位，不修改 TrapFrame 中的原始原因或返回地址。
- 每个 S 态 timer IRQ 恰好重设一次时钟并递增 ticks 一次。
- 每 100 tick 打印一次，普通运行第 10 次打印后请求关机。
- 非时钟分支不会增加时钟计数。软件中断输出 Supervisor software interrupt，未知编号 15 输出 Unknown interrupt: cause 15，并打印 TrapFrame。
- 现有异常处理和汇编不因本模块改动而改变。

## [SPECIFICATION]

1. 输入为有效的 struct trapframe 指针。用 uintptr_t 计算最高位掩码，保留编号所有低位。
2. 按 IRQ 常量分类。IRQ_S_TIMER 首先设置下一次事件，再自增 ticks。以 ticks 对 TICK_NUM 取模，余数为零时输出并累加报告次数。
3. 当报告次数达到 SHUTDOWN_AFTER 时调用 sbi_shutdown。按现有框架保留 DEBUG_GRADE 的特殊输出行为。
4. 已知非时钟来源输出类别，未知来源输出编号和原始 TrapFrame。
5. 验收覆盖单次打印、多次进入后 ticks=200、普通启动十次打印后退出，并记录工具链或前置依赖导致的限制。