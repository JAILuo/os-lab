#include <os/common.h>
#include <os/spinlock.h>
#include <test/test.h>

static void os_init() {
    pmm->init();
    // simple-test
    // test_simple();

    // // basic-alloc-test
    // test_buddy_alloc();

    // // edge-large-test
    // test_edge_cases();

    // // extreme-error-test
    // // test_extreme();

    // // stress-test
    // test_pressure();
    // // stress-test-long-runing
    test_long_running_stability();
}

static void os_run() {
    for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
        putch(*s == '*' ? '0' + cpu_current() : *s);
    }
    // simple-test ✅
    // test_simple();

    // basic-alloc-test ✅
    // test_buddy_alloc();

    // edge-large-test
    // test_edge_cases();

    // extreme-error-test
    // test_extreme();

    // stress-test
    // test_pressure();
    // // stress-test-long-runing
    // test_long_running_stability();
    
    // int i = 1;
    // T_sum(i);
    // safe_printf("sum  = %d\n", sum);
    // safe_printf("%d*n = %d\n", T * 10, T * 10L * N);

    while (1) ;
}

MODULE_DEF(os) = {
    .init = os_init,
    .run  = os_run,
};
