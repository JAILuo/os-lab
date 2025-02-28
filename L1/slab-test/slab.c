#include <linux/module.h>   // Core header for all modules
#include <linux/init.h>     // Macros like module_init()
#include <linux/slab.h>     // For memory allocation (kmalloc())
#include <linux/mm.h>       // For memory management declarations
#include <linux/kernel.h>   // For printk() and other kernel macros

static struct kmem_cache *slab_test;

struct student {
    int age;
    int score;
};

static void mystruct_constructor(void *addr) {
    memset(addr, 0, sizeof(struct student));
}
struct student *peter;

int slab_test_create_kmem(void) {
    int ret = -1;
    slab_test = kmem_cache_create("slab_test",
                                  sizeof(struct student),
                                  0, 0, mystruct_constructor);
    if (slab_test != NULL) {
        printk("slab_test create success!\n");
        ret = 0;
    }
    peter = kmem_cache_alloc(slab_test, GFP_KERNEL);
    if (peter != NULL) {
        printk("alloc object success!\n");
        ret = 0;
    }
    return ret;
}

// int slab_test_create_kmem(void) {
//     slab_test = kmem_cache_create("slab_test", sizeof(struct student),
//                                   0, 0, mystruct_constructor);
//     if (!slab_test) {
//         pr_err("kmem_cache_create failed\n");
//         return -ENOMEM;
//     }
//     
//     peter = kmem_cache_alloc(slab_test, GFP_KERNEL);
//     if (!peter) {
//         pr_err("kmem_cache_alloc failed\n");
//         kmem_cache_destroy(slab_test);
//         return -ENOMEM;
//     }
//     return 0;
// }

static int __init slab_test_init(void) {
    int ret;
    printk("slab_test kernel module init\n");
    ret = slab_test_create_kmem();
    return 0;
}

static void __exit slab_test_exit(void) {
    if (peter) {
        kmem_cache_free(slab_test, peter);
    }
    if (slab_test) {
        kmem_cache_destroy(slab_test);
    }
    printk(KERN_INFO "slab_test kernel module exit\n");
}

// static void __exit slab_test_exit(void) {
//     printk("slab_test kernel module exit\n");
//     kmem_cache_destroy(slab_test);
// }

module_init(slab_test_init);
module_exit(slab_test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jai");
MODULE_DESCRIPTION("Slab Allocator Test Module");

