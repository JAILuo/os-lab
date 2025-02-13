#include <buddy.h>
#include <common.h>

// 测试函数声明
void test_buddy_alloc();
void test_buddy_free();
void test_edge_cases();


    // 运行测试
    // test_buddy_alloc();
    // test_buddy_free();
    // test_edge_cases();

void test_buddy_alloc() {
    printf("\nTesting buddy_alloc...\n");

    // 测试分配不同大小的内存块
    size_t allocations[] = {
        4096,      // 1 page (4KiB)
        8192,      // 2 pages (8KiB)
        16384,     // 4 pages (16KiB)
        1048576,   // 256 pages (1MiB)
        2097152,   // 512 pages (2MiB)
        4194304,   // 1024 pages (4MiB)
        // 8388608,   // 2048 pages (8MiB)
        // the one-time allocation of 8MiB memory is not supported
        // need to learn something...
    };

    for (size_t i = 0; i < sizeof(allocations)/sizeof(allocations[0]); i++) {
        size_t size = allocations[i];
        void *ptr = pmm->alloc(size);

        if (ptr == NULL) {
            printf("Allocation of 0x%x bytes failed\n", size);
        } else {
            printf("Allocated 0x%x bytes at address %p\n", size, ptr);
        }
    }
}

void test_edge_cases() {
    printf("\nTesting edge cases...\n");

    // 测试分配超过堆大小
    size_t max_heap = HEAP_SIZE;
    size_t large_allocation = max_heap + 1;
    void *ptr = pmm->alloc(large_allocation);
    if (ptr == NULL) {
        printf("Allocating 0x%x bytes (exceeds heap) failed (expected)\n", large_allocation);
    } else {
        printf("Error: Allocated 0x%X bytes which exceeds heap size\n", large_allocation);
    }

    // 测试分配 0 字节（非法）
    ptr = pmm->alloc(0);
    if (ptr == NULL) {
        printf("Allocating 0 bytes failed (expected)\n");
    } else {
        printf("Error: Allocated 0 bytes\n");
    }

    // 测试释放空指针
    pmm->free(NULL); // 应该抛出断言错误
}
