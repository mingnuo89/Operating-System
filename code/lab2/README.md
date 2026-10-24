# Lab2：物理内存和页表

此目录基于课程 `lab2.zip` 框架，默认使用 Best-Fit；包含物理页初始化、Buddy、简化 SLUB 和 QEMU 地址翻译调试脚本。

## 在课程容器运行

从仓库根目录进入环境目录：

```bash
cd code
docker compose up -d --build
docker compose exec os-dev bash
cd /workspace/lab2
make
make qemu
```

依次按 Ctrl+A、X 退出 QEMU。自动检查和官方评分：

```bash
python3 tools/run_check.py
make grade
```

`run_check.py` 创建 evidence 日志，发现最终自检通过后停止自己启动的 QEMU。官方评分只覆盖初始化与 Best-Fit，其他模块由内核独立自检验证。

## 切换页分配器

更换宏定义前清理构建产物：

```bash
make clean
make EXTRA_CFLAGS=-DPMM_BUDDY
python3 tools/run_check.py --log evidence/run-buddy.log
make clean
make EXTRA_CFLAGS=-DPMM_FIRST_FIT
python3 tools/run_check.py --log evidence/run-first-fit.log
make clean
make
```

## QEMU 4.1.1 调试

容器 Dockerfile 已包含构建依赖。在 student 用户下执行：

```bash
bash tools/build_debug_qemu.sh
python3 tools/trace_mmu.py
```

调试版构建在 `/tmp/lab2-qemu/qemu-4.1.1`，不替换系统 QEMU。脚本启动宿主与客体两个 GDB，使用回环地址 1235 端口并自动记录 TLB 查询、页表翻译和 Bare 模式。

## 代码和验证范围

- Best-Fit 的两个原始测试函数和官方评分文件保持原样，可执行 `python3 tools/check_integrity.py` 核对。
- Best-Fit、First-Fit、Buddy 三种配置和 SLUB 自检已在本地实际运行；官方评分结果为 20/20。
- SLUB 是单核简化实现，八种小对象规格加连续页大对象分配；测试覆盖尺寸、对齐、数据隔离、跨页、复用与耗尽恢复。
- 框架 `entry.S` 增加过渡阶段低地址恒等映射，保证开启分页后能够跳到高地址内核。
- 个人报告：[report-part-b.md](report-part-b.md)，范围为组员 B 的 Best-Fit、SLUB、GDB 3/4。
- 提示词：[prompt.md](prompt.md)。四份正文也已放进报告，按指导书四段式格式补充整理，注明并非最初开发对话的逐字记录。
- 截图：[13 张真实终端截图索引](images/README.md)；图注区分现场运行与日志回看。原始运行、评分及调试日志留在本地；执行脚本可以重新生成。
- 调试构建缓存与构建产物不提交；脚本可以重新生成日志。
