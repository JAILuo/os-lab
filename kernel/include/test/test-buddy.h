#ifndef __TEST_BUDDY_H
#define __TEST_BUDDY_H

#define TEST_NUM 128
#define TEST_MAGIC 0x55

// simple-test
void test_simple();

// basic-alloc-test
void test_buddy_alloc();

// edge-large-test
void test_edge_cases();

// extreme-error-test
void test_extreme();

// stress-test
void test_pressure();
// stress-test-long-runing
void test_long_running_stability();


// 多线程测试
// static void *thread_alloc(void *arg);
// void test_kalloc_multithread();

#endif
