# 项目目录

## 实验代码

`code/` 是唯一的 Lab2 编译入口，整合了 A 的物理内存初始化与 First-Fit、B 的 Best-Fit/SLUB/GDB 3/4 探针，以及 C 的 Buddy。

- `kern/`、`libs/`：内核和基础库。
- `tools/`：编译、评分、自动运行、完整性核对和双 GDB 工具。
- `Dockerfile`、`compose.yaml`、`docker/`：容器环境，当前 code 目录映射到 `/workspace`。
- `evidence/official-test-hashes.json`：从官方框架提取的原始测试哈希。运行日志和 GDB 脚本在执行时生成，不随目录整理新增上传。

默认使用 Best-Fit。切换前先清理，再通过 `EXTRA_CFLAGS=-DPMM_FIRST_FIT` 或 `EXTRA_CFLAGS=-DPMM_BUDDY` 编译。

## 实验材料

- [report/report_lab2.md](report/report_lab2.md)：A、B、C 的完整小组报告。
- [report/prompt_b.md](report/prompt_b.md)：B 的四份完整提示词，总报告中保留相同正文。
- [report/prompt_c.md](report/prompt_c.md)：C 的 Buddy 实现与测试提示词。
- [report/images/member-b/README.md](report/images/member-b/README.md)：B 的 13 张原始截图及采集方式。
- `report/images/member-c/`、`report/images/pic*.png`：C 和 A 的截图，保留既有文件名。
- `report/evidence/member_c/`：C 已提交的验证记录。

旧的 B 独立副本已从当前目录移除，原版本可从 Git 历史查看。历史截图仍按各自采集时的版本与环境解释。

启动流程为 `kern_entry → kern_init → pmm_init → mmu_probe → slub_init → slub_check`。
