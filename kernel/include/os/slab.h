#ifndef __SLAB_H
#define __SLAB_H

#include <stddef.h>

#include <os/list.h>
#include <os/spinlock.h>

struct kmem_cache {
    const char *name;               // 缓存名
    size_t obj_size;                // 对象大小
    unsigned int align;             // 对齐要求

    struct list_head slabs_full;    // 满的 Slab
    struct list_head slabs_partial; // 部分使用的 Slab
    struct list_head slabs_free;    // 空闲的 Slab
    struct list_head list;          // 全局缓存链表节点
};

struct slab {
    struct kmem_cache *cache;       // 所属缓存
    struct list_head list;          // 同一缓存下的 Slab 列表
    void **free_list;               // 空闲object链表
    unsigned int nr_used;           // 已分配对象数量
};


struct kmem_cache *kmem_cache_create(const char *name, 
                                     size_t size, 
                                     unsigned int align);
void kmem_cache_destroy(struct kmem_cache *cache);

/**
 * allocate and free objects
 */
void *kmem_cache_alloc(struct kmem_cache *cache);
void kmem_cache_free(struct kmem_cache *cache, void *obj);


#endif

