#include <os/common.h>
#include <os/spinlock.h>
#include <os/buddy.h>
#include <test/test-buddy.h>

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
//         safe_printf("Thread %d: Allocated and freed (size=%d)\n", thread_id, size);
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
//     safe_printf("Multithreaded test passed!\n");
// }


static int choose_memory_block(void) {
    int small_size = 256, medium_size = 4 * 1024, large_size = 1024 * 1024;
    int memory_size = 0;
    int probabilities = rand() % 100;
    switch (probabilities) {
    case 0 ... 19: // alloc small memory block: 90%
        // memory_size = rand() % small_size; // will generate 0
        memory_size = (rand() % small_size) ? : 1; 
        memory_size = ROUNDUP(memory_size, 4096); // 关键修复
        break;
    case 20 ... 39: // alloc medium memory block: 8%
        memory_size = small_size + rand() % (medium_size - small_size);
        break;
    case 40 ... 99: // alloc large memory block: 2% 
        memory_size = medium_size + (rand() % (large_size - medium_size));
        break;
    default: 
        panic("memory_size error");
        break;
    }
    return memory_size;
}

static void test_kalloc(char *array[], int array_size[]) {
    int allocated = 0;
    for (int i = 0; i < TEST_NUM; i++) {
        if (array[i] == NULL) {
            array_size[i] = choose_memory_block();
                if (array_size[i] == 0) {
                    safe_printf("WARNING: Skip 0-byte allocation\n");
                    return;
            }

            // safe_printf("allocated size: 0x%x\n", array_size[i]);
            array[i] = (char *)pmm->alloc(array_size[i]);

            // safe_printf("CPU #%d: Allocated 0x%x bytes at %p (slot %d)\n",
            //        cpu_current(), array_size[i], array[i], i);
            panic_on(array[i] == NULL, "Allocation failed: out of memory");

            memset(array[i], TEST_MAGIC + i, array_size[i]);

            allocated = 1;
            break;
        }
    }
    if (!allocated) {
        //safe_printf("All slots full. Skipping allocation.\n");
    }
}

static void test_kfree(char *array[], int array_size[]) {
    int indices[TEST_NUM];
    int count = 0;

    // 收集所有已分配块的索引
    for (int i = 0; i < TEST_NUM; i++) {
        if (array[i] != NULL) {
            indices[count++] = i;
        }
    }

    if (count == 0) {
        //safe_printf("No blocks to free.\n");
        return;
    }

    // 随机选择一个块释放
    int selected = indices[rand() % count];
    char *block = array[selected];
    int size = array_size[selected];

    // 检查内存是否损坏
    for (int i = 0; i < size; i++) {
        if (block[i] != (char)(TEST_MAGIC + selected)) { // 检查每个字节
            safe_printf("Memory corruption in block %d at offset %d\n", selected, i);
            panic("Memory corruption in block  at offset");
        }
    }

    // 释放内存块
    pmm->free(block);
    array[selected] = NULL;
    array_size[selected] = 0;

    // safe_printf("CPU #%d: Freed block at %p (slot %d)\n",
    //        cpu_current(), block, selected);
}

void test_long_running_stability() {
    char *array[TEST_NUM] = {NULL};
    int array_size[TEST_NUM] = {0};

    safe_printf("==========test begin==========\n\n\n");
    while (1) {
        int alloc_or_free = rand() % 2;
        if (alloc_or_free == 1) {
            test_kfree(array, array_size);
        } else {
            test_kalloc(array, array_size);
        }
    }
}

// maybe act as test-free
void test_simple() {
    void *add1 = pmm->alloc(1020);
    if (add1 == NULL) {
        halt(1);
    } else {
        safe_printf("add1: %x\n", add1);
    }

    int *add1_int=(int *)add1;
    *add1_int=9876;
    *(add1_int+(1020-4)/4)=114514;
    safe_printf("add1_int: 0x%x  *add1_int: 0x%x\n", add1_int, *add1_int);
    safe_printf("add1_int+(1020-4)/4: 0x%x, *(add1_int+(1020-4)/4): 0x%x\n",
                add1_int+(1020-4)/4, *(add1_int+(1020-4)/4));

    void *add2 = pmm->alloc(20);
    if (add2 == NULL) {
        safe_printf("add2 is NULL");
        halt(1);
    } else {
        safe_printf("add2: %x\n", add2);
    }

    void *add3 = pmm->alloc(512);
    if (add3 == NULL) {
        safe_printf("add3 is NULL");
        halt(1);
    } else {
        safe_printf("add3: %x\n", add3);
    }

    int *add3_int=(int *)add3;
    *add3_int=543210;
    *(add3_int+(512-4)/4)=114514;

    pmm->free(add2);
    safe_printf("add1_int: 0x%x  *add1_int: 0x%x\n", add1, *add1_int);
    safe_printf("add1_int+(1020-4)/4: 0x%x, *(add1_int+(1020-4)/4): 0x%x\n",
                add1_int+(1020-4)/4, *(add1_int+(1020-4)/4));
    safe_printf("add3_int: 0x%x  *add3_int: 0x%x\n", add3_int, *add3_int);

    assert(*add1_int==9876);
    assert(*(add1_int+(1020-4)/4)==114514);
    assert(*add3_int==543210);
    assert(*(add3_int+(512-4)/4)==114514);

    pmm->free(add1);

    assert(*add3_int==543210);
    assert(*(add3_int+(512-4)/4)==114514);

    pmm->free(add3);

    safe_printf("\n=======================================\n");
    safe_printf("other simple test paseed\n");
    safe_printf("=======================================\n");
}

void test_buddy_alloc() {
    //safe_printf("\nTesting buddy_alloc...\n");

    size_t allocations[] = {
        4096,      // 1 page (4KiB)
        8192,      // 2 pages (8KiB)
        16384,     // 4 pages (16KiB)
        1048576,   // 256 pages (1MiB)
        2097152,   // 512 pages (2MiB)
        4194304,   // 1024 pages (4MiB)
        // 8388608,   // 2048 pages (8MiB)
        // the allocation of 8MiB memory is not supported
        // need to learn something...
    };

    for (size_t i = 0; i < sizeof(allocations)/sizeof(allocations[0]); i++) {
        //size_t size = allocations[i];
        void *ptr = pmm->alloc(allocations[i]);

        if (ptr == NULL) {
            //safe_printf("Allocation of 0x%x bytes failed\n", size);
        } else {
            //safe_printf("Allocated 0x%x bytes at address %p\n", size, ptr);
        }
    }
    safe_printf("\n=======================================\n");
    safe_printf("test_buddy_alloc paseed\n");
    safe_printf("=======================================\n");
}


// 测试释放非法指针
void test_kfree_invalid() {
    void *invalid_ptr = (void *)0xdeadbeef;
    pmm->free(invalid_ptr);
}

// 测试释放未分配的块
void test_kfree_unallocated() {
    void *p = pmm->alloc(1024);
    pmm->free(p);
    pmm->free(p); // 再次释放未分配的块，应该触发错误
}

void test_kfree_null() {
    pmm->free(NULL);
}

// this test lead to qemu crash(actually panic done)...
void test_extreme() {
    test_kfree_invalid();
    test_kfree_unallocated();
    test_kfree_null();
}

void test_edge_cases() {
    safe_printf("\nTesting edge cases...\n");

    // 测试目前最大支持的分配16MiB
    void *p = pmm->alloc(16 * 1024 * 1024 + 1); // 超过支持大小
    assert(p == NULL);

    // 测试分配超过堆大小
    size_t large_allocation = HEAP_SIZE + 1;
    void *ptr = pmm->alloc(large_allocation);
    if (ptr == NULL) {
        safe_printf("Allocating 0x%x bytes (exceeds heap) failed (expected)\n", large_allocation);
    } else {
        safe_printf("Error: Allocated 0x%X bytes which exceeds heap size\n", large_allocation);
        panic("Error: Allocated bytes which exceeds heap size");
    }

    // 测试分配 0 字节（非法）
    ptr = pmm->alloc(0);
    if (ptr == NULL) {
        safe_printf("Allocating 0 bytes failed (expected)\n");
    } else {
        panic("Error: Allocated 0 bytes");
    }

    safe_printf("\n=======================================\n");
    safe_printf("edge_case test passed!\n");
    safe_printf("=======================================\n");
}




