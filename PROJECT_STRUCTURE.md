# 项目结构

| 路径 | 职责 |
| --- | --- |
| code/kern/trap/trapentry.S | 组员 A 的 Trap 入口、寄存器恢复与 sret |
| code/kern/trap/trap.c | 组员 B 的中断分发和时钟处理、组员 C 的异常处理 |
| code/kern/driver/clock.c | 首次时钟初始化、rdtime、SBI 定时事件 |
| code/kern/mm/pmm.c | Lab 2 物理内存初始化，保留 Lab 3 页表与中断保护流程 |
| code/kern/mm/best_fit_pmm.c | 从同仓库 Lab 2 补入的 Best-Fit 实现及 Lab 3 框架检查 |
| code/tools/ | 课程测试注入与评分脚本 |
| report/report.md | 三人合并实验报告 |
| report/prompt_a.md、report/prompt_b.md | A、B 的规格与最终提示词，C 的规格在合并报告正文 |
| report/images/ | 原有测试截图 |
| report/grade-lab3.log、report/qemu-lab3.log | 本次真实评分与普通启动串口记录 |

在装有 RISC-V 交叉编译工具和 QEMU 的课程环境进入 code，执行 make grade 或 make qemu。

启动入口位于 code/kern/init/entry.S。QEMU 加载由 code/Makefile 与 code/tools/grade.sh 统一使用 -kernel。
