#include <stdint.h>
#include <stddef.h>

#include <common.h>
#include <spinlock.h>
#include <list.h>

#define PMMLOCKED 1
#define PMMUNLOCKED 0

typedef int lock_t;
lock_t big_lock;

void lockinit(int *lock) {
    atomic_xchg(lock, PMMUNLOCKED);
}

void spin_lock(int *lock) {
    while(atomic_xchg(lock, PMMLOCKED) == PMMLOCKED) {;}
}

void spin_unlock(int *lock) {
    panic_on(atomic_xchg(lock, PMMUNLOCKED) != PMMLOCKED, "lock is not acquired");
}
/*----------------------------------------*/

#define DEBUG
#ifdef DEBUG
#define debug_pf(fmt, args...) printf(fmt, ##args)
#else
#define debug_pf(fmt, args...) 
#endif


#define MAX_ORDER (10 + 1)
#define PAGESIZE (4 * 1024)
#define MAX_PAGESIZE ((1 << (MAX_ORDER - 1)) * PAGESIZE)

#define PFN_ALIGN(x)	(((unsigned long)(x) + (PAGE_SIZE - 1)) & PAGE_MASK)
#define PFN_UP(x)	(((x) + PAGE_SIZE-1) >> PAGE_SHIFT)
#define PFN_DOWN(x)	((x) >> PAGE_SHIFT)
// #define PFN_PHYS(x)	((phys_addr_t)(x) << PAGE_SHIFT)
// #define PHYS_PFN(x)	((unsigned long)((x) >> PAGE_SHIFT))

#define ROUNDUP(a, sz)   ((((uintptr_t)a) + (sz) - 1) & ~((sz) - 1))
#define ROUNDDOWN(a, sz) ((((uintptr_t)a)) & ~((sz) - 1))

#define HEAP_SIZE (((uintptr_t)heap.end) - ((uintptr_t)heap.start))

struct page {
    unsigned int order : 4;     // 块阶数（0~10）
    bool used;                  // 是否被使用
};

struct free_area {
    struct list_head *head;
    unsigned long nr_free;
};

struct free_area free_lists[MAX_ORDER];

static size_t calculate_alignment(size_t size){
    return SIZE_MAX;
}

void *buddy_alloc(size_t size) {
    return NULL;
}

void *slab_alloc(size_t size) {
    return NULL;
}

static void *kalloc(size_t size) {
    if (size > 16 * 1024 * 1024) return NULL; // Allocations over 16MiB are not supported

    size_t align_size = calculate_alignment(size);

    //panic_on(align_size > MAX_SIZE, "Allocations over 16MiB are not supported");

    void *res = align_size > PAGESIZE ?
        buddy_alloc(align_size) : slab_alloc(align_size);

    return res;

}

static void kfree(void *ptr) {

}


// uintptr_t jump_pages_meta(uintptr_t max_page_num) {
//     uintptr_t pmsize = (
//         (uintptr_t)heap.end - (uintptr_t)heap.start
//     );
// 
//     // round up
//     max_page_num = pmsize / MAX_PAGESIZE;
// 
//     printf("heap_size: 0x%x  MAX_PAGESIZE: 0x%x\n", pmsize, MAX_PAGESIZE);
//     printf("pages_meta: [0x%x, 0x%x), max_page_num: 0x%x(%d)\n", 
//            (uint64_t)(uintptr_t)heap.start, 
//            (uint64_t)(uintptr_t)(heap.start + max_page_num),
//            (uintptr_t)max_page_num, (uint64_t)max_page_num);
// 
//     uintptr_t pages_meta_size = max_page_num * sizeof(struct pagemeta);
// 
//     uintptr_t buddy_start = align_up((uintptr_t)heap.start + pages_meta_size, PAGESIZE);
// 
//     max_page_num = ((uintptr_t)heap.end - buddy_start) / MAX_PAGESIZE;
//     panic_on(buddy_start < (uintptr_t)heap.start, "maybe overwrite page metadata");
//     printf("now, for use, heap_size: 0x%x heap.start: 0x%x, max_page_num: %d\n", 
//            (uintptr_t)heap.end - buddy_start, buddy_start, max_page_num);
// 
//     return buddy_start;
// }

void init_free_block(unsigned long start_pfn, unsigned long end_pfn) {
    if (start_pfn > end_pfn) panic("start_pfn and end_pfn error");

    unsigned long total_pages = end_pfn - start_pfn + 1;
    unsigned long remaining_pages = total_pages;

    for (int order = MAX_ORDER - 1; order >= 0; order--) {
        unsigned long block_size = 1UL << order; // 2^order
        while (remaining_pages >= block_size) {
            // allocte from back to front
            struct list_head *free_block = (struct list_head *)(start_pfn + remaining_pages - block_size);
            INIT_LIST_HEAD(free_block);
            // debug_pf("now order: %d  start_pfn: 0x%x  end_pfn: 0x%x(%d) remaining_pages: 0x%x block_size: 0x%x\n",
            //          order, start_pfn, end_pfn, end_pfn, remaining_pages, block_size);

            list_add(free_block, free_lists[order].head);
            free_lists[order].nr_free++;

            // 更新剩余页数
            remaining_pages -= block_size;
            debug_pf("now order: %d  start_pfn: 0x%x  end_pfn: 0x%x(%d)  "
                     "remaining_pages: 0x%x  block_size: 0x%x(%d)\n",
                     order, start_pfn, end_pfn, end_pfn,
                     remaining_pages, block_size, block_size);
        }
    }
}

// 初始化页元数据
void init_pages() {
    uintptr_t total_pages = HEAP_SIZE / PAGESIZE;
    unsigned long start_pfn = 0, end_pfn = HEAP_SIZE / PAGESIZE - 1;
    debug_pf("start_pfn: 0x%x  end_pfn: 0x%x(%d)\n", start_pfn, end_pfn, end_pfn);

    // 计算元数据区大小及所占页数
    size_t pagedata_size = total_pages * sizeof(struct page);
    size_t pagedata_pages = (pagedata_size + PAGESIZE - 1) / PAGESIZE;
    debug_pf("metadata_size: 0x%x  metadata_pages: 0x%x\n", pagedata_size, pagedata_pages);
    start_pfn += pagedata_pages;
    debug_pf("start_pfn: 0x%x  end_pfn: 0x%x(%d)\n", start_pfn, end_pfn, end_pfn);
    

    // 假设元数据区位于物理内存起始处（page_meta 指向物理地址 0）
    struct page *page_meta = (struct page *)heap.start;
    
    // 初始化元数据区：标记为已使用
    for (size_t pfn = 0; pfn < total_pages; pfn++) {
        page_meta[pfn].used = (pfn < pagedata_pages);
        page_meta[pfn].order = false;
    }
    
    // 记录这个时候page_meta 用了多大的内存，然后对齐4KB
    // 接着存储空闲链表

    uintptr_t free_list_addr = (uintptr_t)heap.start + pagedata_size;
    debug_pf("free_list_addr: 0x%x\n", free_list_addr);
    unsigned long free_lists_size = MAX_ORDER * sizeof(struct free_area);
    unsigned long free_lists_pages = (free_lists_size + PAGESIZE - 1) / PAGESIZE;
    start_pfn += free_lists_pages;
    debug_pf("start_pfn: 0x%x  end_pfn: 0x%x(%d)\n", start_pfn, end_pfn, end_pfn);

    struct free_area *area = (struct free_area *)free_list_addr;
    for (size_t i = 0; i < MAX_ORDER; i++) {
        INIT_LIST_HEAD(free_lists[i].head);
        free_lists[i].nr_free = 0;
        area[i] = free_lists[i];
        debug_pf("area: 0x%x\n", &area[i]);
        // 应该初始化空闲链表了，但是应该用哪个？动态初始化还是静态初始化？
        // 静态：LIST_HEAD_INIT
        // 动态：INIT_LIST_HEAD
    }

    uintptr_t start_used =  free_list_addr + sizeof(free_lists);
    debug_pf("start_used: 0x%x  start_pfn: 0x%x end_pfn: 0x%x\n", 
             start_used, start_pfn, end_pfn);

    // 开始计算放入到空闲链表的各阶的空闲块数量、大小
    init_free_block(start_pfn, end_pfn);
}

void init_freelist() {
   // 将整个池加入空闲链表
    for (int i = 0; i < MAX_ORDER; i++) {
        INIT_LIST_HEAD(free_lists[i].head);
    }
    list_add(free_lists[MAX_ORDER - 1].head, (struct list_head *)heap.start);
}

void init_buddy() {
    init_pages();
}


static void pmm_init() {
    lockinit(&big_lock);

    uintptr_t pmsize = (
        (uintptr_t)heap.end - (uintptr_t)heap.start
    );

    printf(
        "Got %d MiB heap: [%p, %p)\n",
        pmsize >> 20, heap.start, heap.end
    );

    init_buddy();
}

MODULE_DEF(pmm) = {
    .init  = pmm_init,
    .alloc = kalloc,
    .free  = kfree,
};

