#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_ORDER (10 + 1)
#define MEM_SIZE (1 << 20)
#define MIN_SIZE (1 << 12)

struct list_head {
    struct list_head *prev;
    struct list_head *next;
};

struct chunk {
    unsigned int order; // 阶数
    bool is_free; 
}chunk;

// chunks数组元素结构（利用位字段）
typedef struct {
    uintptr_t type : 1;     // 类型：0伙伴，1Slab
    uintptr_t used : 1;     // 使用状态：1一个使用一个空闲，0两个都是空闲或者使用状态
    uintptr_t order : 5;    // 伙伴块阶数（最多支持32）
    uintptr_t index : 25;   // 在数组中的索引（可选，取决于需求）
} ChunkMeta;

struct free_area {
    
};

struct free_area free_list[MAX_ORDER];



#define PAGESIZE (4 * 1024)
#define CHUNKS_SIZE (((uintptr_t)heap.end) - ((uintptr_t)heap.start) + PAGESIZE - 1) / PAGESIZE;

#define MAX_SIZE (16 * 1024 * 1024)

static size_t calculate_alignment(size_t size){

    return SIZE_MAX;
}

void *buddy_alloc(size_t size) {

    return NULL;
}

void *slab_alloc(size_t size) {

    return NULL;
}

void *kmalloc(size_t size) {
    if (size > MAX_SIZE) return NULL; // Allocations over 16MiB are not supported
    
    size_t align_size = calculate_alignment(size);

    //panic_on(align_size > MAX_SIZE, "Allocations over 16MiB are not supported");

    void *res = align_size > PAGESIZE ? 
        buddy_alloc(align_size) : slab_alloc(align_size);

    return res;
}

void buddy_init() {
    for (int i = 0; i <= MAX_ORDER; i++) {
    }

}

int main () {

    uintptr_t *chunks = (uintptr_t*)heap.start;
    uintptr_t chunks_size = (((uintptr_t)heap.end) - ((uintptr_t)heap.start) + PAGESIZE - 1) / PAGESIZE;
    printf("chunks: [%X, %X), chunks_size: %D\n", (uint64_t)(uintptr_t)chunks, (uint64_t)(uintptr_t)(chunks + chunks_size), (uint64_t)chunks_size);
}


