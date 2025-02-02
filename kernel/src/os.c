#include <common.h>

static void os_init() {
    pmm->init();
}

void test_kalloc() {
    void *ptr1 = pmm->alloc(128);
    void *ptr2 = pmm->alloc(256);
    void *ptr3 = pmm->alloc(512);

    printf("Allocated: %p, %p, %p\n", ptr1, ptr2, ptr3);

    pmm->free(ptr1);
    pmm->free(ptr2);
    pmm->free(ptr3);

    void *ptr4 = pmm->alloc(128);
    void *ptr5 = pmm->alloc(256);
    void *ptr6 = pmm->alloc(512);

    printf("Re-Allocated: %p, %p, %p\n", ptr4, ptr5, ptr6);
}

static void os_run() {
    for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
        putch(*s == '*' ? '0' + cpu_current() : *s);
    }

    printf("===test_kalloc===\n");
    test_kalloc();
    while (1) ;
}

MODULE_DEF(os) = {
    .init = os_init,
    .run  = os_run,
};
