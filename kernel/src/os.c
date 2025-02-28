#include <os/common.h>
#include <os/spinlock.h>
#include <test/test.h>

void test_all() {
    // // //simple-test
    //test_simple();

    // // basic-alloc-test
    //test_buddy_alloc();

    // // edge-large-test
    while (1) test_edge_cases();

    // // extreme-error-test
    // test_extreme();

    // stress-test-long-runing
    //test_long_running_stability();

}

/**
 * Simple concurrency test
 */
int test_var = 0;
void test_mp() {
    for (int i = 0; i < 10000000; i++) {
        spin_lock(&big_lock);
        test_var++;
        spin_unlock(&big_lock);
    }
    printf("in CPU#%d, var: %d\n", cpu_current(), test_var);
}

static void os_init() {
    pmm->init();
    //test_all();
}

static void os_run() {
    for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
        putch(*s == '*' ? '0' + cpu_current() : *s);
    }

    //test_all();
    //test_mp();

    while (1) ;
}

MODULE_DEF(os) = {
    .init = os_init,
    .run  = os_run,
};
