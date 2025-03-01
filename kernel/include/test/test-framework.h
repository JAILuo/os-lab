#ifndef __TEST_FRAMEWORK_H
#define __TEST_FRAMEWORK_H

#include <utils.h>

#define TEST(test_name) static void test_name()

#define CHECK(cond, ...) do { \
    if (!(cond)) { \
        printf(ANSI_FG_RED "Test FAIL!" ANSI_NONE \
               "@ %s:%d | %s | ", __FILE__, __LINE__, #cond); \
        printf(__VA_ARGS__); \
        printf("\n"); \
        panic(0); \
    } } while (0)

//#define CHECK(cond) do {
//    if (!(cond)) {
//        printf(ANSI_FG_RED "Test FAIL!" ANSI_NONE
//               "@ %s:%d | %s | ", __FILE__, __LINE__, #cond);
//        panic(0);
//    return; } } while (0)

// #define RUN_TEST(test_fn) do { 
//     printf("[Running " #test_fn "]\n"); 
//     test_fn(); 
//     printf("[" #test_fn "] passed!\n"); 
// } while(0)

// 增强版测试运行宏（带颜色和分类标识）
#define RUN_TEST(test_fn) do { \
    printf(ANSI_FG_CYAN "[ RUN      ]" ANSI_NONE " %s\n", #test_fn); \
    test_fn(); \
    printf(ANSI_FG_GREEN "[   PASSED ]" ANSI_NONE " %s\n", #test_fn); \
} while(0)

// 测试套件分类宏
#define TEST_SUITE(name, color, ...) do { \
    printf("\n%s[==========]" ANSI_NONE " %s\n", color, name); \
    void (*tests[])(void) = { __VA_ARGS__ }; \
    for (unsigned i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) { \
        RUN_TEST(tests[i]); \
    } \
    printf("%s[==========]" ANSI_NONE " %s Completed\n", color, name); \
} while(0)

// 具体分类的快捷宏
#define UNIT_TEST_SUITE(...)    TEST_SUITE("Unit Tests",   ANSI_FG_CYAN, ##__VA_ARGS__)
#define INTEG_TEST_SUITE(...)   TEST_SUITE("Integration",  ANSI_FG_BLUE, ##__VA_ARGS__)
#define STRESS_TEST_SUITE(...)  TEST_SUITE("Stress Tests", ANSI_FG_MAGENTA, ##__VA_ARGS__)
#define PERF_TEST_SUITE(...)    TEST_SUITE("Performance",  ANSI_FG_YELLOW, ##__VA_ARGS__)
#define RAND_TEST_SUITE(...)    TEST_SUITE("Random Ops",   ANSI_FG_GREEN, ##__VA_ARGS__)

/**
 * maybe need to use C-method to support portability
 */

// 精确的 TSC 计时宏（x86_64）
#define TIME() ({ \
    unsigned int _lo, _hi; \
    asm volatile ( \
        "mfence\n\t"          /* 内存屏障保证指令顺序 */ \
        "rdtsc\n\t" \
        : "=a"(_lo), "=d"(_hi) \
    ); \
    ((unsigned long)_hi << 32) | _lo; \
})

#endif
