# 变更记录

## 2026-11-07

- 在 trap.c 实现组员 B 中断分发与时钟处理，保留 lab3 标记并填写 2410625。
- 从 origin/lab2（8fcd03f）补入 pmm.c 的 page_init 和 best_fit_pmm.c 的分配器主体，补齐 Lab 3 编译依赖。物理上界使用 Lab 3 现有内存布局常量计算。
- 补全 report/report.md 的组员 B 章节及整合验证，新增 report/prompt_b.md 和真实运行日志。

- 修复 QEMU 固件入口和 satp 切换取指所需的恒等映射。WSL 整合评分 100/100，普通启动十次输出后正常退出。

- Trap 汇编保存与恢复统一使用 stval，修复新版 RISC-V 汇编器拒绝旧 CSR 名称的问题。
