#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

// 简单的分配和释放测试
static void test_kalloc_simple() {
    unsigned char *p1 = (unsigned char *)pmm->alloc(1024);
    assert(p1 != NULL);
    *p1 = '\0'; // 测试分配的内存是否可写

    void *p2 = pmm->alloc(4096 * 2); // 分配大于一页的内存
    assert(p2 != NULL);

    pmm->free(p2); // 测试释放操作
    // 再次分配，验证是否正确回收
    void *p3 = pmm->alloc(4096 * 2);
    assert(p3 != NULL);

    // 测试释放指针后
    pmm->free(p1);
    pmm->free(p3);
}

// 多次分配和释放，测试Buddy系统的稳定性
static void test_kalloc_pressure() {
    int alloc_sizes[] = {1024, 4096, 8192, 32768, 16384};
    int size_count = sizeof(alloc_sizes)/sizeof(int);
    int rounds = 100;

    struct MemBlock {
        void *ptr;
        size_t size;
    } blocks[rounds * size_count];

    for (int i=0; i<rounds; i++) {
        for (int j=0; j<size_count; j++) {
            // 随机分配
            int rand_size = alloc_sizes[rand() % size_count];
            blocks[i*size_count + j].ptr = pmm->alloc(rand_size);
            blocks[i*size_count + j].size = rand_size;
            assert(blocks[i*size_count + j].ptr != NULL);
        }
    }

    // 随机释放
    for (int i=0; i<rounds*size_count; i++) {
        pmm->free(blocks[i].ptr);
    }

    printf("Pressure test passed!\n");
}


// 多线程测试
static void *thread_alloc(void *arg) {
    int thread_id = *((int *)arg);
    for (int i=0; i<100; i++) {
        size_t size = 4096 * (rand() % 4 + 1);
        void *p = pmm->alloc(size);
        assert(p != NULL);
        // 测试分配的内存是否可用
        memset(p, 0, size);
        // 休眠一段时间，模拟释放延迟
        usleep(rand() % 10000);
        pmm->free(p);
        vprintf("Thread %d: Allocated and freed (size=%d)\n", thread_id, size);
    }
    pthread_exit(NULL);
}

void test_kalloc_multithread() {
    pthread_t threads[10];
    int tids[10];

    for (int i=0; i<10; i++) {
        tids[i] = i;
        int ret = pthread_create(&threads[i], NULL, thread_alloc, (void*)&tids[i]);
        assert(ret == 0);
    }

    // 等待线程完成
    for (int i=0; i<10; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Multithreaded test passed!\n");
}

// 测试分配大块失败
void test_kalloc_large() {
    void *p = pmm->alloc(16 * 1024 * 1024 + 1); // 超过支持大小
    assert(p == NULL);
}

// 测试释放非法指针
void test_kfree_invalid() {
    void *invalid_ptr = (void *)0xdeadbeef;
    // 假设非法指针会导致panic
    pmm->free(invalid_ptr); // 应该触发某个错误
}

// 测试释放未分配的块
void test_kfree_unallocated() {
    void *p = pmm->alloc(1024);
    pmm->free(p);
    pmm->free(p); // 再次释放未分配的块，应该触发错误
}

// 该函数用于初始化测试环境
static void init_test() {
    // 初始化随机数种子
    srand(time(NULL));
}

int main(int argc, char *argv[]) {
    init_test();

    if (argc < 2) {
        printf("Usage: %s [simple|pressure|multithread|all]\n", argv[0]);
        return 1;
    }

    const char *test_type = argv[1];
    if (strcmp(test_type, "simple") == 0) {
        test_kalloc_simple();
    } else if (strcmp(test_type, "pressure") == 0) {
        test_kalloc_pressure();
    } else if (strcmp(test_type, "multithread") == 0) {
        test_kalloc_multithread();
    } else if (strcmp(test_type, "all") == 0) {
        test_kalloc_simple();
        test_kalloc_pressure();
        test_kalloc_multithread();
        test_kalloc_large();
        test_kfree_invalid();
        test_kfree_unallocated();
    } else {
        printf("Unknown test type: %s\n", test_type);
        return 1;
    }

    printf("All tests passed!\n");
    return 0;
}
