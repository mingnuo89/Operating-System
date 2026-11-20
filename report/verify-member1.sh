#!/usr/bin/env bash
set -euo pipefail

lab=$(cd "$(dirname "$0")/.." && pwd)
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
cp -R "$lab/Makefile" "$lab/kern" "$lab/libs" "$lab/tools" "$scratch/"
cd "$scratch"

sed '/^void test_lab4_process_create(void)/,$d' tools/test_lab4.c > kern/libs/test_member1.c
cp "$lab/report/test-member1.c" kern/libs/test_member1_edges.c
sed -i '/#include <dtb.h>/a #include <sbi.h>\nvoid test_lab4_pmm_basic(void);\nvoid test_member1_edges(void);' kern/init/init.c
sed -i '/    pmm_init();/a\    test_member1_edges();\n    test_lab4_pmm_basic();\n    sbi_shutdown();\n    for (;;) { }' kern/init/init.c
sed -i '/    pic_init();/,/    cpu_idle();/d' kern/init/init.c

printf '\n=== Lab 4: member 1 only (temporary boot entry) ===\n'
riscv64-unknown-elf-gcc --version | head -n 1
qemu-system-riscv64 --version | head -n 1
timeout 30s make qemu > run.log 2>&1 || { tail -n 65 run.log; exit 1; }
sed -n '/(THU.CST) os is loading/,$p' run.log
grep -Fq '[Test PMM] PASS' run.log
