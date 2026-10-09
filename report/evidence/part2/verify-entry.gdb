set pagination off
set confirm off
set architecture riscv:rv64
file bin/kernel
target remote localhost:1234
break *kern_entry
continue
printf "\n=== Before stack initialization ===\n"
info registers pc sp ra
p/x &bootstack
p/x &bootstacktop
p/d (char *)&bootstacktop - (char *)&bootstack
x/3i $pc
si
si
printf "\n=== After la sp, bootstacktop ===\n"
info registers pc sp ra
si
printf "\n=== Enter kern_init: tail does not change ra ===\n"
info registers pc sp ra
p/x &edata
p/x &end
break *0x8020001c
continue
printf "\n=== C function stack frame (16 bytes) ===\n"
info registers pc sp
x/4i $pc
detach
quit
