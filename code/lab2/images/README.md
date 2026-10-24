# 实验截图索引

截图日期：2026-10-22。全部是通过原生 Otty 窗口截图接口保存的 JPEG，没有使用绘图程序重建终端或改写画面。日志回看与现场运行分开标注。

| 图片 | 取证方式 | 对应命令/原始材料 | 说明 |
| --- | --- | --- | --- |
| [01-build.jpg](01-build.jpg) | 现场编译 | `make clean && make -j4` | 全部模块编译、链接与镜像生成 |
| [02-best-fit-slub.jpg](02-best-fit-slub.jpg) | 现场运行 | `evidence/run-best-fit.log` | Best-Fit、三次访存、SLUB 全部自检 |
| [03-official-grade.jpg](03-official-grade.jpg) | 现场评分 | `evidence/grade.log` | 官方初始化与 Best-Fit，20/20 |
| [04-buddy.jpg](04-buddy.jpg) | 日志回看 | `evidence/run-buddy.log` | Buddy 自检及其上的 SLUB |
| [05-first-fit.jpg](05-first-fit.jpg) | 日志回看 | `evidence/run-first-fit.log` | First-Fit 与 SLUB |
| [06-memory-sizes.jpg](06-memory-sizes.jpg) | 日志回看 | `evidence/run-64M.log、run-256M.log` | RAM 探测、管理上限、自检 |
| [07-qemu-4.1.1.jpg](07-qemu-4.1.1.jpg) | 现场运行 | `evidence/run-qemu-4.1.1.log` | 指导书版本运行结果 |
| [08-gdb-guest.jpg](08-gdb-guest.jpg) | 日志回看 | `evidence/gdb-guest-capture.log` | 客体 GDB 停点、三条 ld、数据 |
| [09-gdb-page-walk.jpg](09-gdb-page-walk.jpg) | 日志回看 | `evidence/gdb-host-tlb-capture.log` | C 查询、PTE、实际 PA、两次填充 |
| [10-gdb-bare-addend.jpg](10-gdb-bare-addend.jpg) | 日志回看 | `evidence/gdb-host-tlb-capture.log` | Bare 填充与宿主 addend |
| [11-gdb-live.jpg](11-gdb-live.jpg) | 现场双 GDB | `evidence/trace-summary.log` | 重新执行双 GDB 与自动核对结果 |
| [12-slub-cases.jpg](12-slub-cases.jpg) | 源码与日志回看 | `kern/mm/slub_check.c、evidence/run-best-fit.log` | 27 种尺寸与耗尽恢复 |
| [13-test-integrity.jpg](13-test-integrity.jpg) | 现场完整性检查 | `evidence/integrity-check.log` | 官方测试与评分文件未修改 |

GDB 现场重跑后会覆盖默认日志，因此 08—10 对应的那一轮另存为 `*-capture.log`。宿主内存地址因进程布局变化而不同，不能用不同轮次的宿主地址直接比较。
