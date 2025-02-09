#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

// 元数据头，位于每个分配块的前端
typedef struct header {
    size_t size;        // 用户请求的大小（对齐后）
    bool is_page;       // 是否为页分配
} header_t;

#define MAX_ORDER (10 + 1)
#define PAGESIZE (4 * 1024)
#define MAX_PAGESIZE ((2 << (MAX_ORDER - 1)) * PAGESIZE)

struct list_head {
    struct list_head *next, *prev;
};


struct page {
    struct list_head buddy_list;
    unsigned int order;         // 块阶数（0~10）
    bool used;                  // 是否被使用
    bool is_slab;
    struct page *compound_head;
};

int main() {
    printf("MAX_PAGESIZE: 0x%x\n", MAX_PAGESIZE);


    void *ptr = malloc(100);
    header_t *hptr1 = (header_t *) ptr - 1;
    header_t *hptr2 = (header_t *)((uintptr_t)ptr - sizeof(header_t));

    printf("==ptr: %p==\n", ptr);
    printf("hptr1: %p   hptr2: %p\n", hptr1, hptr2);

    printf("sizeof(struct page): %zu\n", sizeof(struct page));

    return 0;
}
