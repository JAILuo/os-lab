#include <stdbool.h>
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

#define HEAP_SIZE (((uintptr_t)heap.end) - ((uintptr_t)heap.start))

struct page {
    struct list_head buddy_list;
    unsigned int order;         // 块阶数（0~10）
    bool used;                  // 是否被使用
    bool is_slab;
    struct page *compound_head;
};

struct free_area {
    struct list_head *head;
    unsigned long nr_free;
};

struct free_area *ok_free_lists = NULL;


static inline struct page *pfn_to_page(unsigned long pfn) {
    // return (struct page *)heap.start + pfn;
    return (struct page *)((uintptr_t)heap.start + pfn * sizeof(struct page));
}

static inline unsigned long page_to_pfn(struct page *page) {
    return ((unsigned long)((uintptr_t)page - (uintptr_t)heap.start) / sizeof(struct page));
}

static inline int get_order(size_t size) {
    int order = 0;
    while ((1UL << order) * PAGESIZE < size) order++;
    return order;
}

static void remove_from_free_list(struct free_area *area) {
    struct list_head *block_node = area->head->next;
    list_del(block_node);
    area->nr_free--;
}

// static unsigned long set_block_meta_data() {
// }

void *allocate_block(int order) {
    struct free_area *area = &ok_free_lists[order];

    if (area->nr_free == 0) return NULL;
    panic_on(area->nr_free == 0, "nr_free should > 0");
    
    remove_from_free_list(area);

    struct page *block_page = (struct page *)area->head->next;
    block_page->used = true;
    block_page->order = order;

    return block_page;
}

// 辅助函数：将伙伴块添加到对应空闲链表
static void add_buddy_to_freelist(struct page *buddy_page, int order) {
    struct free_area *area = &ok_free_lists[order];
    list_add(&buddy_page->buddy_list, area->head);
    area->nr_free++;
}

void split_block(struct page *block_page,
                 int *current_order,
                 int target_order) {
    unsigned long block_pfn = page_to_pfn(block_page);
    while (*current_order > target_order) {
        (*current_order)--;
        
        // 计算伙伴块的PFN（异或操作是伙伴系统的核心）
        unsigned long buddy_pfn = block_pfn ^ (1UL << *current_order);
        struct page *buddy_page = pfn_to_page(buddy_pfn);

        // 初始化伙伴块元数据
        buddy_page->order = *current_order;
        buddy_page->used = false;

        // 将伙伴块加入空闲链表
        add_buddy_to_freelist(buddy_page, *current_order);

        // 更新当前块元数据（order会在循环最后更新）
        block_page->order = *current_order;
    }
}

void *buddy_alloc(size_t size) {
    panic_on(size < PAGESIZE, "size must be at least PAGESIZE");

    int order = get_order(size);
    debug_pf("Requested size: 0x%x, required order: %d\n", size, order);

    for (int current_order = order; current_order < MAX_ORDER; current_order++) {
        struct page *block_page = allocate_block(current_order);
        if (!block_page) continue;

        int original_order = current_order;
        split_block(block_page, &current_order, order);

        debug_pf("Allocated block at pfn %u (order %d->%d)\n",
               page_to_pfn(block_page), original_order, current_order);

        debug_pf("block_page: 0x%x to pfn: %d\n", block_page, page_to_pfn(block_page));

        return (void *)(heap.start + page_to_pfn(block_page) * PAGESIZE);
    }

    debug_pf("No available blocks for order %d\n", order);
    return NULL;
}

void *slab_alloc(size_t size) {
    return NULL;
}

static void *kalloc(size_t size) {
    if (size > 16 * 1024 * 1024) return NULL; // Allocations over 16MiB are not supported

    debug_pf("==========start alloc=========\n");
   
    size_t align_size = ROUNDUP(size, PAGESIZE);
    debug_pf("size: 0x%x  align_size: 0x%x\n", size, align_size);

    panic_on(align_size > 16 * 1024 * 1024, 
             "Allocations over 16MiB are not supported");

    // now for test, only >= 4KB
    // condititon should be > 4KB
    void *res = align_size >= PAGESIZE ?
        buddy_alloc(align_size) : slab_alloc(align_size);

    debug_pf("==========finish alloc=========\n");
    return res;
}

static void kfree(void *ptr) {

}

void debug_free_list(struct free_area *free_lists, int order) {
    struct page *page;
    struct list_head *head = free_lists[order].head; // 链表头指针
    struct list_head *current;                         // 当前节点指针

    debug_pf("free_lists[%d].nr_free = %d\n", order, free_lists[order].nr_free);

    // 获取链表头的下一个节点
    current = head->next;

    // 开始遍历链表
    while (1) {
        // 计算当前节点对应的 struct page 的地址
        page = (struct page *)((char *)current);

        // 打印当前页面的信息
        debug_pf("Page in order %d: 0x%x (compound_head: 0x%x)\n",
                 order, page, (uintptr_t)page->compound_head);

        // 获取下一个链表节点
        current = current->next;

        // 如果遍历到链表头节点，退出循环
        if (current == head) {
            break;
        }
    }
}

void init_free_block(struct free_area *free_lists,
                     unsigned long start_pfn, unsigned long end_pfn) {
    if (start_pfn > end_pfn) panic("Invalid PFN range");
    unsigned long remaining_pages = end_pfn - start_pfn + 1;

    for (int order = MAX_ORDER - 1; order >= 0; order--) {
        unsigned long block_size = 1UL << order;

        // 分配块的头页PFN（从后向前）
        while (remaining_pages >= block_size) {
            unsigned long head_pfn = start_pfn + remaining_pages - block_size;
            struct page *head_page = pfn_to_page(head_pfn);

            debug_pf("head_pfn: 0x%x  head_page: 0x%x\n",
                     head_pfn, head_page);

            head_page->used = false;
            head_page->order = order;
            head_page->compound_head = head_page;  // 头页指向自身
            INIT_LIST_HEAD(&head_page->buddy_list);
            
            debug_pf("head_page->compound_head: 0x%x\n", head_page->compound_head);

            // 问题应该在这里，free_lists->head_page->buddy_list

            // 初始化尾页元数据
            for (unsigned long pfn = head_pfn + 1; pfn < head_pfn + block_size; pfn++) {
                struct page *tail_page = pfn_to_page(pfn);
                tail_page->used = false;
                tail_page->order = -1;             // 标记为尾页
                tail_page->compound_head = head_page; // 尾页指向头页
                INIT_LIST_HEAD(&tail_page->buddy_list);
                list_add_tail(&tail_page->buddy_list, &head_page->buddy_list);
            }

            // 将头页的buddy_list加入free_lists[order].head
            
            // 这里搞晕了，一直不明白为什么head->next一直都不变
            // 其实用的list_add_tail，插到尾部，肯定头不变啊
            // 不过再想想，因为我是从后向前分配页的，
            // 加上自己想低阶/各阶首个空闲块地址是从小到大
            // 应该用list_add

            list_add(&head_page->buddy_list, free_lists[order].head);
            free_lists[order].nr_free++;

            remaining_pages -= block_size;

            // TODO 学习，linux遍历链表的API
            // struct page *current_page;
            // list_for_each_entry(current_page, free_lists[order].head, buddy_list) {
            //     debug_pf("Node in free_lists[%d]: 0x%x free_lists[%d].nr_free: %d\n", 
            //              order, current_page, order, free_lists[order].nr_free);
            // }

            debug_pf("free_list[%d].head: 0x%x  "
                     "free_lists[%d].head: next=0x%x prev=0x%x\n", 
                     order, free_lists[order].head,
                     order, free_lists[order].head->next, 
                            free_lists[order].head->prev);

            debug_pf("now order: %d  start_pfn: 0x%x  end_pfn: 0x%x(%d)  "
                     "remaining_pages: 0x%x  block_size: 0x%x(%d)\n\n",
                     order, start_pfn, end_pfn, end_pfn,
                     remaining_pages, block_size, block_size);
        }
    }
    for (int i = MAX_ORDER - 1; i >= 0; i--) {
        debug_pf("free_lists[%d].nr_free: 0x%x(%d)\n",
                 i, free_lists[i].nr_free, free_lists[i].nr_free);
    }
}

size_t init_page_meta_data(unsigned long start_pfn, unsigned long end_pfn) {
    uintptr_t total_pages = HEAP_SIZE / PAGESIZE;
    debug_pf("total_pages: 0x%x\n", total_pages);
    debug_pf("start_pfn: 0x%x  end_pfn: 0x%x(%d)\n", start_pfn, end_pfn, end_pfn);

    // 计算元数据区大小及所占页数
    size_t pagedata_size = total_pages * sizeof(struct page);
    unsigned long pagedata_pages = (pagedata_size + PAGESIZE - 1) / PAGESIZE;

    struct page *page_meta = (struct page *)heap.start;
    for (size_t pfn = 0; pfn < total_pages; pfn++) {
        page_meta[pfn].used = (pfn < pagedata_pages);
        page_meta[pfn].is_slab = false;
        page_meta[pfn].order = 0;
        INIT_LIST_HEAD(&page_meta[pfn].buddy_list);
    }

    debug_pf("metadata_size: 0x%x  metadata_pages: 0x%x(%d)\n", pagedata_size, pagedata_pages, pagedata_pages);
    start_pfn += pagedata_pages;
    debug_pf("start_pfn: 0x%x  end_pfn: 0x%x(%d)\n", start_pfn, end_pfn, end_pfn);
    debug_pf("==============\n");

    return pagedata_size;
}

// 初始化页元数据
void init_pages() {
    unsigned long start_pfn = 0, end_pfn = (HEAP_SIZE / PAGESIZE) - 1;

    // uintptr_t total_pages = HEAP_SIZE / PAGESIZE;
    // debug_pf("total_pages: 0x%x\n", total_pages);
    // debug_pf("start_pfn: 0x%x  end_pfn: 0x%x(%d)\n", start_pfn, end_pfn, end_pfn);

    // // 计算元数据区大小及所占页数
    // size_t pagedata_size = total_pages * sizeof(struct page);
    // size_t pagedata_pages = (pagedata_size + PAGESIZE - 1) / PAGESIZE;

    // struct page *page_meta = (struct page *)heap.start;
    // for (size_t pfn = 0; pfn < total_pages; pfn++) {
    //     page_meta[pfn].used = (pfn < pagedata_pages);
    //     page_meta[pfn].is_slab = false;
    //     page_meta[pfn].order = 0;
    //     INIT_LIST_HEAD(&page_meta[pfn].buddy_list);
    // }

    // debug_pf("metadata_size: 0x%x  metadata_pages: 0x%x(%d)\n", pagedata_size, pagedata_pages, pagedata_pages);
    // start_pfn += pagedata_pages;
    // debug_pf("start_pfn: 0x%x  end_pfn: 0x%x(%d)\n", start_pfn, end_pfn, end_pfn);
    // debug_pf("==============\n");
    
    // 记录这个时候page_meta 用了多大的内存，然后对齐4KB
    // 接着存储空闲链表

    size_t pagedata_size = init_page_meta_data(start_pfn, end_pfn);
    debug_pf("tes: 0x%x\n", pagedata_size);

    uintptr_t free_list_addr = (uintptr_t)heap.start + pagedata_size;
    debug_pf("free_list_addr: 0x%x\n", free_list_addr);

    ok_free_lists = (struct free_area *)free_list_addr;
    struct free_area *free_lists = (struct free_area *)free_list_addr;

    for (int i = MAX_ORDER - 1; i >= 0; i--) {
        debug_pf("free_lists[%d].nr_free: 0x%x(%d)  ",
                 i, free_lists[i].nr_free, free_lists[i].nr_free);
        debug_pf("now free_lists addr: 0x%x\n", &free_lists[i]);
    }
    // 静态：LIST_HEAD_INIT
    // 动态：INIT_LIST_HEAD
    for (int i = 0; i < MAX_ORDER; i++) {
        INIT_LIST_HEAD(free_lists[i].head);
        free_lists[i].nr_free = 0;

        debug_pf("====sizoeof(free_lists): 0x%x\n", sizeof(struct free_area) * MAX_ORDER);
        debug_pf("====&free_lists[%d]: 0x%x  free_lists[%d].head: 0x%x  free_lists[%d].nr_free: 0x%x\n",
                 i, &free_lists[i], i, &free_lists[i].head, i, &free_lists[i].nr_free);

        // struct free_area **area = (struct free_area **)free_list_addr;
        // area[i] = &free_lists[i];
        // 问题在这一句
        // 将free_lists的地址再次赋给area，但是我复现不出来了！！
        //debug_pf("area[%d]: 0x%x   free_lists[%d]'s addr: 0x%x\n", i, area[i], i, &free_lists[i]);
        // debug_pf("free_list[%d].head: 0x%x\n", i, free_lists[i].head);
    }
    
    for (int i = MAX_ORDER - 1; i >= 0; i--) {
        panic_on(free_lists[i].nr_free != 0, "nr_free error");
        debug_pf("free_lists[%d].nr_free: 0x%x(%d)  ",
                 i, free_lists[i].nr_free, free_lists[i].nr_free);
        debug_pf("now free_lists addr: 0x%x\n", &free_lists[i]);
    }

    unsigned long free_lists_size = MAX_ORDER * sizeof(struct free_area);
    unsigned long free_lists_pages = (free_lists_size + PAGESIZE - 1) / PAGESIZE;
    debug_pf("free_lists_size: 0x%x  free_lists_pages: 0x%x(%d)\n", 
             free_lists_size, free_lists_pages, free_lists_pages);
    start_pfn += free_lists_pages;
    debug_pf("start_pfn: 0x%x  end_pfn: 0x%x(%d)\n", start_pfn, end_pfn, end_pfn);

    uintptr_t start_used = ROUNDUP(free_list_addr + MAX_ORDER * sizeof(struct free_area), PAGESIZE);
    debug_pf("start_used: 0x%x  start_pfn: 0x%x end_pfn: 0x%x\n", 
             start_used, start_pfn, end_pfn);

    debug_pf("==============\n");
    // 开始计算放入到空闲链表的各阶的空闲块数量、大小
    init_free_block(free_lists, start_pfn, end_pfn);
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

