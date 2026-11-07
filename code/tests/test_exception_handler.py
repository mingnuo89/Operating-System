#!/usr/bin/env python3
"""Exercise the actual handler on the host without the unfinished trap entry."""
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'kern/trap/trap.c').read_text()
start = source.index('void exception_handler(struct trapframe *tf) {')
end = source.index('\nstatic inline void trap_dispatch', start)
handler = source[start:end]
harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define CAUSE_ILLEGAL_INSTRUCTION 2
#define CAUSE_BREAKPOINT 3
struct trapframe { uintptr_t cause, epc; };
static char output[128];
static int fallback;
static void cprintf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vsnprintf(output, sizeof(output), format, ap);
    va_end(ap);
}
static void print_trapframe(struct trapframe *tf) { (void)tf; fallback++; }
''' + handler + r'''
int main(void) {
    uint16_t insn[] = {0x0000, 0x007b, 0x9002, 0x0073};
    struct trapframe tf;
    for (int cause = 2; cause <= 3; cause++) {
        for (int i = 0; i < 4; i++) {
            tf.cause = cause;
            tf.epc = (uintptr_t)&insn[i];
            exception_handler(&tf);
            assert(tf.epc == (uintptr_t)&insn[i] + (i % 2 ? 4 : 2));
            assert(strstr(output, cause == 2 ? "Illegal instruction at" : "Breakpoint at"));
        }
    }
    tf.cause = 20;
    tf.epc = 0;
    exception_handler(&tf);
    assert(tf.epc == 0 && fallback == 1);
    puts("exception_handler: 16-bit, 32-bit, and fallback cases passed");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    test = Path(tmp) / 'test.c'
    test.write_text(harness)
    executable = Path(tmp) / 'test'
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', str(test), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
