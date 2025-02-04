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


int main() {
    void *ptr = malloc(100);
    header_t *hptr1 = (header_t *) ptr - 1;
    header_t *hptr2 = (header_t *)((uintptr_t)ptr - sizeof(header_t));

    printf("==ptr: %p==\n", ptr);
    printf("hptr1: %p   hptr2: %p\n", hptr1, hptr2);
    return 0;
}
