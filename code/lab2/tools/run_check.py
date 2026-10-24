#!/usr/bin/env python3
"""Run QEMU until the kernel's final self-check, save the real console output."""
import argparse, os, signal, subprocess, sys, time, selectors
parser = argparse.ArgumentParser()
parser.add_argument('--log', default='evidence/run-best-fit.log')
parser.add_argument('--qemu', default='qemu-system-riscv64')
parser.add_argument('--memory', default='128M')
a = parser.parse_args()
cmd = [a.qemu, '-machine', 'virt', '-nographic', '-bios', 'default',
       '-m', a.memory, '-kernel', 'bin/ucore.img']
p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                     start_new_session=True)
selector = selectors.DefaultSelector()
selector.register(p.stdout, selectors.EVENT_READ)
output = b''
try:
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline and b'slub_check() succeeded!' not in output:
        events = selector.select(0.2)
        for key, event in events:
            chunk = os.read(key.fileobj.fileno(), 65536)
            if not chunk: break
            output += chunk
        if p.poll() is not None: break
finally:
    if p.poll() is None: os.killpg(p.pid, signal.SIGTERM)
    tail = p.communicate(timeout=5)[0]
    output += tail
text = output.decode(errors='replace')
os.makedirs(os.path.dirname(a.log) or '.', exist_ok=True)
open(a.log, 'w').write('$ ' + ' '.join(cmd) + '\n' + text)
print(text)
if 'slub_check() succeeded!' not in text or 'panic' in text.lower(): sys.exit(1)
