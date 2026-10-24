/* lab2 GDB demonstration - 2410849: same data, hit then forced refill. */
#include <pmm.h>
#include <stdio.h>
volatile uint64_t lab2_probe_value __attribute__((aligned(4096))) = 0x1122334455667788UL;
void mmu_probe(void) {
    uint64_t first, second, third;
    uintptr_t address = (uintptr_t)&lab2_probe_value;
    asm volatile(
        "sfence.vma\n"
        ".global lab2_probe_first\nlab2_probe_first:\nld %0, 0(%3)\n"
        ".global lab2_probe_second\nlab2_probe_second:\nld %1, 0(%3)\n"
        "sfence.vma\n"
        ".global lab2_probe_third\nlab2_probe_third:\nld %2, 0(%3)\n"
        : "=&r"(first), "=&r"(second), "=&r"(third) : "r"(address) : "memory");
    assert(first == 0x1122334455667788UL && first == second && second == third);
    cprintf("MMU probe: three loads agree, value=0x%016lx\n", first);
}
