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
#define INTEG_TEST_SUITE(...)   TEST_SUITE("Integration",  ANSI_FG_CYAN, ##__VA_ARGS__)
#define STRESS_TEST_SUITE(...)  TEST_SUITE("Stress Tests", ANSI_FG_CYAN, ##__VA_ARGS__)
#define PERF_TEST_SUITE(...)    TEST_SUITE("Performance",  ANSI_FG_CYAN, ##__VA_ARGS__)
#define RAND_TEST_SUITE(...)    TEST_SUITE("Random Ops",   ANSI_FG_CYAN, ##__VA_ARGS__)

#endif
