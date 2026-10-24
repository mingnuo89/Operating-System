#!/usr/bin/env python3
"""Two real GDB sessions: host QEMU breakpoints, guest ucore execution."""
import os, pathlib, signal, socket, subprocess, time, argparse
p = argparse.ArgumentParser()
p.add_argument('--qemu-root', default='/tmp/lab2-qemu/qemu-4.1.1')
a = p.parse_args()
root = pathlib.Path(a.qemu_root)
qemu = root/'riscv64-softmmu/qemu-system-riscv64'
pathlib.Path('evidence').mkdir(exist_ok=True)
symbols = {}
for line in subprocess.check_output(['riscv64-unknown-elf-nm','bin/kernel'],text=True).splitlines():
    fields=line.split()
    if len(fields)==3: symbols[fields[2]]=int(fields[0],16)
va=symbols['lab2_probe_value']; bare=symbols['boot_hartid']-0xffffffff40000000
source=root/'target/riscv/cpu_helper.c'
lines=source.read_text().splitlines()
pte_line=next(i for i,s in enumerate(lines,1) if 'if (!(pte & PTE_V))' in s)
pa_line=next(i for i,s in enumerate(lines,1) if 'if ((pte & PTE_R) || ((pte & PTE_X) && mxr))' in s)
fill_line=next(i for i,s in enumerate(lines,1) if 'ret = get_physical_address(env, &pa' in s)
cputlb=root/'accel/tcg/cputlb.c'
check_line=next(i for i,s in enumerate(cputlb.read_text().splitlines(),1) if 'if (!tlb_hit(tlb_addr, addr))' in s and i>1200)
ready_line=next(i for i,s in enumerate(cputlb.read_text().splitlines(),1) if 'tlb_n_used_entries_inc(env, mmu_idx);' in s and i>800)
script=f'''set pagination off
set confirm off
set print pretty off
handle SIGPIPE nostop noprint pass
set $fills = 0
break {source}:{fill_line} if address == {va:#x} || (address == {bare:#x} && mmu_idx == 1)
commands
silent
printf "FILL address=0x%lx access=%d mmu_idx=%d satp=0x%lx priv=%d\\n", address, access_type, mmu_idx, env->satp, env->priv
if address == {va:#x}
set $fills = $fills + 1
printf "PROBE_FILL_COUNT=%d\\n", $fills
bt 6
end
continue
end
break {cputlb}:{check_line} if addr == {va:#x} && !code_read
commands
silent
printf "C_TLB_CHECK addr=0x%lx cached=0x%lx index=%lu mmu_idx=%lu\\n", addr, tlb_addr, index, mmu_idx
continue
end
break {source}:{pte_line} if addr == {va:#x}
commands
silent
printf "PTE level_iteration=%d index=%lu base=0x%lx pte_addr=0x%lx pte=0x%lx ppn=0x%lx ptshift=%d\\n", i, idx, base, pte_addr, pte, ppn, ptshift
continue
end
break {source}:{pa_line} if addr == {va:#x}
commands
silent
printf "TRANSLATED va=0x%lx pa_page=0x%lx vpn=0x%lx\\n", addr, *physical, vpn
continue
end
break tlb_set_page if vaddr == {va:#x} || (vaddr == {bare & ~4095:#x} && mmu_idx == 1)
commands
silent
printf "TLB_INSTALL va_page=0x%lx pa_page=0x%lx prot=%d mmu_idx=%d size=%lu\\n", vaddr, paddr, prot, mmu_idx, size
continue
end
break {cputlb}:{ready_line} if vaddr == {va:#x} || (vaddr == {bare & ~4095:#x} && mmu_idx == 1)
commands
silent
printf "TLB_READY tag=0x%lx addend=0x%lx host_page=0x%lx\\n", te->addr_read, te->addend, vaddr_page + te->addend
continue
end
run
'''
pathlib.Path('evidence/host-trace.gdb').write_text(script)
guest=f'''set pagination off
set confirm off
set architecture riscv:rv64
file bin/kernel
target remote 127.0.0.1:1235
break slub_init
continue
printf "GUEST: reached slub_init after all three probe loads\\n"
x/3i {symbols['lab2_probe_first']:#x}
x/i {symbols['lab2_probe_third']:#x}
x/gx {va:#x}
monitor info registers
detach
quit
'''
pathlib.Path('evidence/guest-trace.gdb').write_text(guest)
hostlog=open('evidence/gdb-host-tlb.log','w')
cmd=['gdb','-q','-nx','-batch','-x','evidence/host-trace.gdb','--args',str(qemu),
     '-machine','virt','-nographic','-bios','default','-kernel','bin/ucore.img',
     '-monitor','none','-serial','file:evidence/gdb-qemu-console.log',
     '-gdb','tcp:127.0.0.1:1235','-S']
outer=subprocess.Popen(cmd,stdout=hostlog,stderr=subprocess.STDOUT,start_new_session=True)
try:
    deadline=time.monotonic()+15
    while time.monotonic()<deadline:
        if outer.poll() is not None: raise RuntimeError('Host GDB exited; see log')
        try:
            with socket.create_connection(('127.0.0.1',1235),timeout=.2): pass
            break
        except OSError: time.sleep(.1)
    else: raise RuntimeError('QEMU GDB stub did not start')
    guest_run=subprocess.run(['gdb-multiarch','-q','-nx','-batch','-x','evidence/guest-trace.gdb'],
                             stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=40,text=True)
    pathlib.Path('evidence/gdb-guest.log').write_text(guest_run.stdout)
    if guest_run.returncode: raise RuntimeError(guest_run.stdout)
    time.sleep(.5)
finally:
    if outer.poll() is None: os.killpg(outer.pid,signal.SIGTERM)
    try:outer.wait(timeout=5)
    except subprocess.TimeoutExpired:os.killpg(outer.pid,signal.SIGKILL);outer.wait()
    hostlog.close()
text=pathlib.Path('evidence/gdb-host-tlb.log').read_text()
print(text)
print(pathlib.Path('evidence/gdb-guest.log').read_text())
assert 'PROBE_FILL_COUNT=2' in text and 'PROBE_FILL_COUNT=3' not in text
assert 'TRANSLATED' in text and 'C_TLB_CHECK' in text
assert 'satp=0x0' in text and 'TLB_INSTALL' in text
print('TRACE CHECK: two probe refills for three loads; Bare fill/install also observed.')
