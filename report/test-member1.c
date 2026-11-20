#include <assert.h>
#include <error.h>
#include <pmm.h>
#include <stdio.h>

static size_t budget;
static const struct pmm_manager *allocator;

static struct Page *alloc_with_budget(size_t count)
{
    if (count > budget)
    {
        return NULL;
    }
    budget -= count;
    return allocator->alloc_pages(count);
}

void test_member1_edges(void)
{
    const uintptr_t address = PGSIZE;
    struct Page *payload = alloc_page();
    assert(payload != NULL);
    assert(boot_pgdir_va[0] == 0);
    size_t free_before = nr_free_pages();

    page_remove(boot_pgdir_va, address);
    assert(boot_pgdir_va[0] == 0 && nr_free_pages() == free_before);
    cprintf("[Member 1] remove without a page table: PASS\n");

    allocator = pmm_manager;
    struct pmm_manager limited = *allocator;
    limited.alloc_pages = alloc_with_budget;
    for (size_t allowance = 0; allowance <= 1; ++allowance)
    {
        budget = allowance;
        pmm_manager = &limited;
        int result = page_insert(boot_pgdir_va, payload, address, PTE_R);
        pmm_manager = allocator;
        assert(result == -E_NO_MEM && boot_pgdir_va[0] == 0);
        assert(nr_free_pages() == free_before && page_ref(payload) == 0);
    }
    cprintf("[Member 1] first/second table allocation failure: PASS\n");

    struct Page *replacement = alloc_page();
    assert(replacement != NULL);
    *(uint32_t *)page2kva(payload) = 0x1234;
    *(uint32_t *)page2kva(replacement) = 0x5678;
    volatile uint32_t *mapped = (volatile uint32_t *)address;
    assert(page_insert(boot_pgdir_va, payload, address, PTE_R) == 0);
    assert(*mapped == 0x1234);
    assert(page_insert(boot_pgdir_va, replacement, address, PTE_R) == 0);
    assert(*mapped == 0x5678 && page_ref(payload) == 0);
    page_remove(boot_pgdir_va, address);
    assert(page_ref(replacement) == 0);

    struct Page *middle_table = pde2page(boot_pgdir_va[0]);
    pde_t *directory = page2kva(middle_table);
    struct Page *leaf_table = pde2page(directory[0]);
    boot_pgdir_va[0] = 0;
    flush_tlb();
    free_page(leaf_table);
    free_page(middle_table);
    assert(nr_free_pages() == free_before + 1);
    cprintf("[Member 1] access after replacing a cached mapping: PASS\n");
}
