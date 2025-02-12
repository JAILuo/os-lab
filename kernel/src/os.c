#include <common.h>

typedef int lock_t;

extern lock_t big_lock;
void lockinit(int *lock);
void spin_lock(int *lock);
void spin_unlock(int *lock);

#define MAGIC (0x55)
#define TEST_NUM (128)

// long volatile sum = 0;
// #define T        3
// #define N  100000
// 
// 
// void T_sum(int tid) {
//     for (int i = 0; i < N; i++) {
//         spin_lock(&big_lock);
// 
//         // This critical section is even longer; but
//         // it should be safe--the world is stopped.
//         // We also marked sum as volatile to make
//         // sure it is loaded and stored in each
//         // loop iteration.
//         for (int _ = 0; _ < 10; _++) {
//             sum++;
//         }
// 
//         spin_unlock(&big_lock);
//     }
// 
//     printf("Thread %d: sum = %d\n", tid, sum);
// }

// for test buddy
static int choose_memory_block(void) {
    //int small_size = 256, medium_size = 4 * 1024, large_size = 1024 * 1024;
    int medium_size = 4 * 1024, large_size = 1024 * 1024;
    int memory_size = 0;
    int probabilities = rand() % 100;
    switch (probabilities) {
    // case 0 ... 4: // alloc small memory block: 5%
    //     memory_size = rand() % small_size;
    //     break;
    // case 5 ... 89: // alloc medium memory block: 85%
    //     memory_size = small_size + rand() % (medium_size - small_size);
    //     break;
    case 1 ... 99: // alloc large memory block: 10% 
        memory_size = medium_size + (rand() % (large_size - medium_size));
        break;
    default: 
        panic("memory_size error");
        break;
    }
    return memory_size;
}

// static int choose_memory_block(void) {
//     int small_size = 256, medium_size = 4 * 1024, large_size = 1024 * 1024;
//     int memory_size = 0;
//     int probabilities = rand() % 100;
//     switch (probabilities) {
//     case 0 ... 89: // alloc small memory block: 90%
//         memory_size = rand() % small_size;
//         break;
//     case 90 ... 97: // alloc medium memory block: 8%
//         memory_size = small_size + rand() % (medium_size - small_size);
//         break;
//     case 98 ... 99: // alloc large memory block: 2% 
//         memory_size = medium_size + (rand() % (large_size - medium_size));
//         break;
//     default: 
//         panic("memory_size error");
//         break;
//     }
//     return memory_size;
// }

void test_kalloc(char *array[], int array_size[]) {
    int allocated = 0;
    for (int i = 0; i < TEST_NUM; i++) {
        if (array[i] == NULL) {
            array_size[i] = choose_memory_block();
            array[i] = (char *)pmm->alloc(array_size[i]);

            panic_on(array[i] == NULL, "Allocation failed: out of memory");
            printf("CPU #%d: Allocated 0x%x bytes at %p (slot %d)\n",
                   cpu_current(), array_size[i], array[i], i);

            // spin_lock(&big_lock);
            memset(array[i], MAGIC + i, array_size[i]);
            // spin_unlock(&big_lock);

            allocated = 1;
            break;
        }
    }
    if (!allocated)
        printf("All slots full. Skipping allocation.\n");
}

void test_kfree(char *array[], int array_size[]) {
    int indices[TEST_NUM];
    int count = 0;

    // 收集所有已分配块的索引
    for (int i = 0; i < TEST_NUM; i++) {
        if (array[i] != NULL) {
            indices[count++] = i;
        }
    }

    if (count == 0) {
        printf("No blocks to free.\n");
        return;
    }

    // 随机选择一个块释放
    int selected = indices[rand() % count];
    char *block = array[selected];
    int size = array_size[selected];

    // 检查内存是否损坏
    spin_lock(&big_lock); // 确保检查期间内存不被修改
    for (int i = 0; i < size; i++) {
        if (block[i] != (char)(MAGIC + selected)) { // 检查每个字节
            spin_unlock(&big_lock);
            printf("Memory corruption in block %d at offset %d\n", selected, i);
            panic("Memory corruption in block  at offset");
        }
    }
    spin_unlock(&big_lock);

    // 释放内存块
    pmm->free(block);
    array[selected] = NULL;
    array_size[selected] = 0;

    printf("CPU #%d: Freed block at %p (slot %d)\n",
           cpu_current(), block, selected);
}

void test_kalloc_stress() {
    for (int i = 0; i < 10; i++) {
        void *ptr = pmm->alloc((i + 1) * 512); // 分配 128 字节
        if (ptr == NULL) {
            printf("Allocation failed at iteration %d\n", i);
            break;
        }
        pmm->free(ptr); // 释放内存
    }
    printf("\n=======================================\n");
    printf("test_kalloc_stress paseed\n");
    printf("=======================================\n\n");

}

void test_pmm() {
    char *array[TEST_NUM] = {NULL};
    int array_size[TEST_NUM] = {0};

    while (1) {
        int alloc_or_free = rand() % 2;
        if (alloc_or_free == 1) { // Allocate test
            test_kalloc(array, array_size);
        } else { // Free test
            test_kfree(array, array_size);
        }
    }
}

void test_kalloc_other() {
    void *add1 = pmm->alloc(1020);
    if (add1 == NULL) {
        printf("add is NULL");
        halt(1);
    } else
        printf("add1: %x\n", add1);

      int *add1_int=(int *)add1;
      *add1_int=9876;
      *(add1_int+(1020-4)/4)=114514;
      printf("add1_int: 0x%x  *add1_int: 0x%x\n"
             "add1_int+(1020-4)/4: 0x%x, *(add1_int+(1020-4)/4): 0x%x\n",
             add1_int, *add1_int, add1_int+(1020-4)/4, *(add1_int+(1020-4)/4));

    void *add2 = pmm->alloc(20);
    if (add2 == NULL) {
        printf("add2 is NULL");
        halt(1);
    } else
        printf("add2: %x\n", add2);

    void *add3 = pmm->alloc(512);
    if (add3 == NULL) {
        printf("add3 is NULL");
        halt(1);
    } else
        printf("add3: %x\n", add3);

    int *add3_int=(int *)add3;
    *add3_int=543210;
    *(add3_int+(512-4)/4)=114514;

    pmm->free(add2);
    printf("add1_int: 0x%x  *add1_int: 0x%x\n"
           "add1_int+(1020-4)/4: 0x%x, *(add1_int+(1020-4)/4): 0x%x\n",
           add1_int, *add1_int, add1_int+(1020-4)/4, *(add1_int+(1020-4)/4));
    printf("add3_int: 0x%x  *add3_int: 0x%x\n", add3_int, *add3_int);

    assert(*add1_int==9876);
    assert(*(add1_int+(1020-4)/4)==114514);
    assert(*add3_int==543210);
    assert(*(add3_int+(512-4)/4)==114514);

    pmm->free(add1);

    assert(*add3_int==543210);
    assert(*(add3_int+(512-4)/4)==114514);

    pmm->free(add3);

    printf("\n=======================================\n");
    printf("other test paseed\n");
    printf("=======================================\n\n");
}

// 简单的分配和释放测试
void test_kalloc_simple() {
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

    printf("\n=======================================\n");
    printf("test_kalloc_simple paseed\n");
    printf("=======================================\n\n");
}

// 多次分配和释放，测试Buddy系统的稳定性
void test_kalloc_pressure() {
    int alloc_sizes[] = {1024, 4096, 8192, 32768};
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

    // // 随机释放
    // for (int i=0; i<rounds*size_count; i++) {
    //     pmm->free(blocks[i].ptr);
    // }

    printf("\n=======================================\n");
    printf("Pressure test passed!\n");
    printf("=======================================\n\n");
}


// // 多线程测试
// static void *thread_alloc(void *arg) {
//     int thread_id = *((int *)arg);
//     for (int i=0; i<100; i++) {
//         size_t size = 4096 * (rand() % 4 + 1);
//         void *p = pmm->alloc(size);
//         assert(p != NULL);
//         // 测试分配的内存是否可用
//         memset(p, 0, size);
//         // 休眠一段时间，模拟释放延迟
//         usleep(rand() % 10000);
//         pmm->free(p);
//         vprintf("Thread %d: Allocated and freed (size=%d)\n", thread_id, size);
//     }
//     pthread_exit(NULL);
// }
// 
// void test_kalloc_multithread() {
//     pthread_t threads[10];
//     int tids[10];
// 
//     for (int i=0; i<10; i++) {
//         tids[i] = i;
//         int ret = pthread_create(&threads[i], NULL, thread_alloc, (void*)&tids[i]);
//         assert(ret == 0);
//     }
// 
//     // 等待线程完成
//     for (int i=0; i<10; i++) {
//         pthread_join(threads[i], NULL);
//     }
// 
//     printf("Multithreaded test passed!\n");
// }

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


static void os_init() {
    pmm->init();
    // test_kalloc_other();
    // test_kalloc_simple();
    // test_kalloc_stress();
    // test_kalloc_pressure();
    // test_pmm();
}

static void os_run() {
    for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
        putch(*s == '*' ? '0' + cpu_current() : *s);
    }
    
    // int i = 1;
    // T_sum(i);
    // printf("sum  = %d\n", sum);
    // printf("%d*n = %d\n", T * 10, T * 10L * N);

    //test_kalloc_other();
    //test_kalloc_simple();
    test_kalloc_stress();
    //test_kalloc_pressure();
    // test_pmm();

    while (1) ;
}

MODULE_DEF(os) = {
    .init = os_init,
    .run  = os_run,
};
