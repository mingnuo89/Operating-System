# Lab2 代码与运行

本目录是 Lab2 的工程根目录，包含 `Makefile`、`kern/`、`libs/`、`tools/` 及 Docker 环境。报告和提示词位于同级的 [report 目录](../report/report.md)，截图见 [索引](../report/images/README.md)。

## 编译与验证

从仓库根目录执行：

```bash
cd lab/code
docker compose up -d --build
docker compose exec os-dev bash
```

容器把当前 `code/` 目录挂载为 `/workspace`，进入容器后直接运行：

```bash
cd /workspace
make
make qemu
```

依次按 `Ctrl+A`、`X` 退出 QEMU。自动检查与官方评分：

```bash
python3 tools/run_check.py
make grade
python3 tools/check_integrity.py
```

官方评分只覆盖初始化和 Best-Fit，原实测结果为 20/20；SLUB、Buddy 和 GDB 另行验证。完整性脚本读取 `evidence/official-test-hashes.json`，该文件是测试元数据。运行时产生的日志也保存在 `evidence/`，历史原始日志未公开上传。

## 切换底层分配器

在工程根目录操作，切换前清理目标文件：

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

三种页分配器接入同一套 SLUB 自检。SLUB 为单核简化实现：八种小对象规格，加连续页大对象分配，不提供并发安全保证。

## QEMU 4.1.1 调试

容器中执行：

```bash
bash tools/build_debug_qemu.sh
python3 tools/trace_mmu.py
```

调试版单独构建在 `/tmp/lab2-qemu/qemu-4.1.1`，不替换系统 QEMU。脚本启动宿主和客体两个 GDB，stub 只监听回环地址 1235 端口，生成 TLB 查询、PTE、填充及 Bare 模式的日志。

本次目录整理保留内核、原始测试和调试脚本的文件内容，仅更改存放路径与文档链接。构建产物、调试缓存及历史运行日志不纳入提交。
