/* Black-box tests of the public SLUB allocation/free contract. */
#include <slub.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>
static void *objects[600];
static const size_t requests[] = {1,15,16,17,31,32,33,63,64,65,127,128,129,
    255,256,257,511,512,513,1023,1024,1025,2047,2048,2049,4096,8193};
void slub_check(void) {
    size_t before = nr_free_pages();
    assert(kmalloc(0) == NULL);
    assert(kmalloc(~(size_t)0) == NULL);
    kfree(NULL);
    for (size_t i = 0; i < sizeof(requests)/sizeof(requests[0]); i++) {
        objects[i] = kmalloc(requests[i]);
        assert(objects[i] && ((uintptr_t)objects[i] & 15) == 0);
        memset(objects[i], i + 1, requests[i]);
        for (size_t j = 0; j < i; j++) {
            uintptr_t a = (uintptr_t)objects[i], b = (uintptr_t)objects[j];
            assert(a + requests[i] <= b || b + requests[j] <= a);
        }
    }
    for (size_t i = 0; i < sizeof(requests)/sizeof(requests[0]); i++) {
        for (size_t j = 0; j < requests[i]; j++)
            assert(((unsigned char *)objects[i])[j] == (unsigned char)(i + 1));
        kfree(objects[i]);
    }
    assert(slub_pages_in_use() == 0 && nr_free_pages() == before);
    cprintf("SLUB sizes/alignment/data/large allocations: PASS\n");
    for (size_t i = 0; i < 600; i++) {
        objects[i] = kmalloc(16);
        assert(objects[i]);
        memset(objects[i], i % 251, 16);
    }
    assert(slub_pages_in_use() >= 3);
    void *released = objects[0];
    kfree(objects[0]);
    objects[0] = kmalloc(16);
    assert(objects[0] == released);
    memset(objects[0], 0, 16);
    for (size_t i = 0; i < 600; i++) {
        for (size_t j = 0; j < 16; j++)
            assert(((unsigned char *)objects[i])[j] == i % 251);
    }
    for (size_t i = 0; i < 600; i += 2) kfree(objects[i]);
    for (size_t i = 1; i < 600; i += 2) kfree(objects[i]);
    assert(slub_pages_in_use() == 0 && nr_free_pages() == before);
    assert(kmalloc((before + 1) * PGSIZE) == NULL);
    assert(nr_free_pages() == before);
    cprintf("SLUB multi-page/reuse/reclaim/failure: PASS\n");
    void *anchor = kmalloc(16);
    assert(anchor);
    memset(anchor, 0x5a, 16);
    list_entry_t exhausted;
    list_init(&exhausted);
    struct Page *p;
    while ((p = alloc_page()) != NULL) list_add(&exhausted, &p->page_link);
    assert(nr_free_pages() == 0);
    void *cached = kmalloc(16);
    assert(cached && cached != anchor);
    assert(kmalloc(32) == NULL && kmalloc(8193) == NULL);
    for (size_t i = 0; i < 16; i++) assert(((unsigned char *)anchor)[i] == 0x5a);
    kfree(cached);
    kfree(anchor);
    while (!list_empty(&exhausted)) {
        p = le2page(list_next(&exhausted), page_link);
        list_del(&p->page_link);
        free_page(p);
    }
    assert(slub_pages_in_use() == 0 && nr_free_pages() == before);
    cprintf("SLUB true exhaustion/cached object/recovery: PASS\n");
    cprintf("slub_check() succeeded!\n");
}
