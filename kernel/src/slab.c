#include <stdint.h>

#include <os/common.h>
#include <os/buddy.h>
#include <os/list.h>
#include <os/slab.h>

//#define SLAB_TEST
#ifdef SLAB_TEST
#define kdebug(fmt, args...) \
    printf(fmt, ##args)

#else
#define kdebug(fmt, args...)
#endif

static struct kmem_cache boot_kmem_cache = {
    .name = "kmem_cache",
    .obj_size = sizeof(struct kmem_cache),
    .align = 0,
    //.align = __alignof__(struct kmem_cache),
    .slabs_full = LIST_HEAD_INIT(boot_kmem_cache.slabs_full),
    .slabs_partial = LIST_HEAD_INIT(boot_kmem_cache.slabs_partial),
    .slabs_free = LIST_HEAD_INIT(boot_kmem_cache.slabs_free),
};

/* The slab cache that manages slab cache information */
struct kmem_cache *kmem_cache;
/* The list of all slab caches on the system */
struct list_head slab_caches;

static enum {
    DOWN,      // 未初始化
    PARTIAL,   // 启动缓存可用
    UP         // 完全运行
} slab_state = DOWN;

/******************** 缓存创建核心函数 ********************/
static char boot_store[sizeof(struct kmem_cache)]__attribute__((aligned(8)));
static struct kmem_cache *__kmem_cache_create(const char *name,
                                              size_t size,
                                              size_t align,
                                              unsigned long flags) {
    struct kmem_cache *cache;
    if (align == 0) {
        align = DEFAULT_ALIGN; // 例如8字节
    }
    
    if (slab_state == DOWN) {
        cache = (struct kmem_cache *)boot_store;
        kdebug("sizeof(struct kmem_cache): %d\n", sizeof(struct kmem_cache));
    } else {
        // 正常阶段通过已有缓存分配
        cache = kmem_cache_alloc(kmem_cache);
    }

    cache->name = name;
    cache->align = align;
    cache->obj_size = ALIGN_UP(size, align);
    kdebug("name: %s, obj_size: %d\n", cache->name, cache->obj_size);
    
    INIT_LIST_HEAD(&cache->slabs_full);
    INIT_LIST_HEAD(&cache->slabs_partial);
    INIT_LIST_HEAD(&cache->slabs_free);
    INIT_LIST_HEAD(&cache->list);
    
    return cache;
}

/******************** 辅助初始化函数 ********************/
static void slab_init(struct slab *slab, struct kmem_cache *cache) {
    slab->cache = cache;
    slab->nr_used = 0;
    INIT_LIST_HEAD(&slab->list);
    
    size_t obj_size = cache->obj_size;
    int num_objs = (PAGESIZE - sizeof(struct slab)) / obj_size;
    kdebug("size struct slab: %d  num_objs: %d\n", sizeof(struct slab), num_objs);

    panic_on(num_objs == 0, "num_objs should not be 0");
    slab->nr_total = num_objs;

    char *start = (char *)ALIGN_UP((uintptr_t)(slab + 1), cache->align);
    char *p = start;
    void *prev = NULL;
    for (int i = 0; i < num_objs; ++i) {
        *(void **)p = prev;
        prev = p;
        p += obj_size;
    }
    slab->free_list = prev;
}

static int init_cache_slab(struct kmem_cache *cache) {
    void *page = buddy_alloc(PAGESIZE);
    if (page == NULL) return -1;

    struct slab *slab = (struct slab *)page;
    slab_init(slab, cache);

    // 将slab加入缓存空闲列表
    list_add(&slab->list, &cache->slabs_free);

    // 注册到全局缓存链表
    list_add(&cache->list, &slab_caches);
    return 0;
}

/******************* 启动缓存创建函数 *******************/
static void create_boot_cache(struct kmem_cache **cachep,
                              const char *name,
                              size_t size,
                              size_t align) {
    *cachep = __kmem_cache_create(name, size, align, 0);
    panic_on(*cachep == NULL, "Failed to create boot cache");
    panic_on(init_cache_slab(*cachep) != 0, "init_cache_slab error");
}

/******************** 用户态缓存创建接口 ********************/
struct kmem_cache *kmem_cache_create(const char *name,
                                     size_t size,
                                     unsigned int align) {
    struct kmem_cache *cache = __kmem_cache_create(name, size, align, 0);
    panic_on(cache == NULL, "Failed to create boot cache");

    if (init_cache_slab(cache) != 0) {
        if (slab_state != DOWN)
            //kmem_cache_free(kmem_cache, cache);
        return NULL;
    }
    return cache;
}

/******************** 用户态缓存销毁接口 ********************/
//void kmem_cache_destroy(struct kmem_cache *cache) {
//    struct slab *slab, *tmp;
//    // 释放所有slab
//    list_for_each_entry_safe(slab, tmp, &cache->slabs_full, list) {
//        buddy_free(slab, PAGESIZE);
//    }
//    list_for_each_entry_safe(slab, tmp, &cache->slabs_partial, list) {
//        buddy_free(slab, PAGESIZE);
//    }
//    list_for_each_entry_safe(slab, tmp, &cache->slabs_free, list) {
//        buddy_free(slab, PAGESIZE);
//    }
//    // 从全局链表移除
//    list_del(&cache->list);
//    // 释放缓存自身
//    if (cache != &boot_kmem_cache) {
//        kmem_cache_free(kmem_cache, cache);
//    }
//}

/******************** 初始化入口函数 ********************/
struct kmem_cache *size_caches[MAX_ORDER];  // 足够存放8B到4KB的缓存

static int calc_index(size_t size) {
    size_t normalized = size / MIN_OBJECT_SIZE;
    int index = 0;
    while (normalized > 1) {
        normalized >>= 1;
        index++;
    }
    return index;
}

void kmem_cache_init(void) {
    INIT_LIST_HEAD(&slab_caches);

    // 创建管理 kmem_cache 的专用缓存
    create_boot_cache(&kmem_cache, "kmem_cache",
                      sizeof(struct kmem_cache),
                      __alignof__(struct kmem_cache));
    slab_state = PARTIAL;

    // 分配常用的缓存
    kdebug("\n=======pre-allocate======\n");
    for (unsigned int size = MIN_OBJECT_SIZE; size < MAX_OBJECT_SIZE; size <<= 1) {
        char name[32];
        kdebug("size: %d\n", size);
        snprintf(name, sizeof(name), "size-%d", size);
        size_caches[calc_index(size)] = kmem_cache_create(name, size, 0);
    }
}

/******************** 对象分配函数 ********************/
void *kmem_cache_alloc(struct kmem_cache *cache) {
    struct slab *target_slab = NULL;
    
    // 优先从部分满 slab 分配
    if (!list_empty(&cache->slabs_partial)) {
        target_slab = list_first_entry(&cache->slabs_partial, struct slab, list);
    }  else if (!list_empty(&cache->slabs_free)) {
    // 其次尝试空闲 slab
        target_slab = list_first_entry(&cache->slabs_free, struct slab, list);
        list_move(&target_slab->list, &cache->slabs_partial);
    } else {
        void *page = buddy_alloc(PAGESIZE);
        if (!page) return NULL;
        target_slab = (struct slab *)page;
        slab_init(target_slab, cache);
        list_add(&target_slab->list, &cache->slabs_partial);
    }
    
    // 执行分配
    void *obj = target_slab->free_list;
    target_slab->free_list = *(void **)obj;
    target_slab->nr_used++;
    
    if (target_slab->nr_used == target_slab->nr_total) {
        list_move(&target_slab->list, &cache->slabs_full);
    }
    
    return obj;
}

/******************** 对象释放函数 ********************/
void kmem_cache_free(struct kmem_cache *cache, void *obj)
{
    struct slab *slab = (struct slab *)((unsigned long)obj & PAGE_MASK);

    // 回收对象
    *(void **)obj = slab->free_list;
    slab->free_list = obj;
    slab->nr_used--;

    // 状态迁移
    if (slab->nr_used == 0) {
        list_move(&slab->list, &cache->slabs_free);
    } else {
        // 如果之前是full状态（通过总对象数判断）
        if (slab->nr_used == (slab->nr_total - 1)) {
            list_move(&slab->list, &cache->slabs_partial);
        }
    }
}


