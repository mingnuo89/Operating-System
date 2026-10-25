# Lab2：物理内存和页表

南开大学 2026 操作系统实验，小组 25。成员：2410878 吕明诺、2410849 张祖浩、2410625 吴宇轩。

本分支只保留一套整合后的实验代码和报告：

```text
code/                     # 内核、测试工具和 Docker 环境
report/
  report_lab2.md          # 小组完整报告
  prompt_b.md             # B：Best-Fit、SLUB、GDB 3/4 完整提示词
  prompt_c.md             # C：Buddy 实现与测试提示词
  images/member-b/        # B 的 13 张截图及来源索引
  images/member-c/        # C 的截图
  evidence/member_c/      # C 的验证记录
```

- [代码与运行说明](code/README.md)
- [小组实验报告](report/report_lab2.md)
- [成员 B 完整提示词](report/prompt_b.md)、[成员 C 提示词](report/prompt_c.md)
- [成员 B 截图索引](report/images/member-b/README.md)
- [目录说明](PROJECT_STRUCTURE.md)

已配置 RISC-V 工具链、QEMU 和 OpenSBI 的 Linux/WSL 环境，可以直接运行：

```bash
cd code
make
make qemu
```

Apple Silicon macOS 可使用课程 Docker 环境：

```bash
cd code
docker compose up -d --build
docker compose exec os-dev bash
# 以下命令在容器内执行
cd /workspace
make
python3 tools/run_check.py
make grade
python3 tools/check_integrity.py
```

默认使用 Best-Fit，可切换 First-Fit 和 Buddy；三种配置共用 SLUB 自检。官方评分仅覆盖初始化和 Best-Fit。双 GDB 调试使用单独构建的 QEMU 4.1.1，具体步骤见代码说明。

Lab1 保留在 [lab1 分支](https://github.com/mingnuo89/Operating-System/tree/lab1)。
