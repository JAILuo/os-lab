#include <os/common.h>
#include <os/buddy.h>
#include <os/slab.h>

#define TEST(test_name) static void test_name()
#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("Test failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        panic(0); \
    return; } } while (0)


// /**
//  * 测试 kmem_cache_create 和 kmem_cache_destroy
//  */
// TEST(test_cache_create_destroy) {
//     struct kmem_cache *cache = kmem_cache_create("test_cache", 128, 0, NULL, NULL);
//     CHECK(cache != NULL);
//     kmem_cache_destroy(cache);
// }
// 
// /**
//  * 测试对象分配与释放
//  */
// TEST(test_alloc_free) {
//     struct kmem_cache *cache = kmem_cache_create("test_alloc", 64, 0, NULL, NULL);
//     CHECK(cache != NULL);
// 
//     // 分配一个对象
//     void *obj1 = kmem_cache_alloc(cache);
//     CHECK(obj1 != NULL);
// 
//     // 再分配第二个对象
//     void *obj2 = kmem_cache_alloc(cache);
//     CHECK(obj2 != NULL);
// 
//     // 释放其中一个对象
//     kmem_cache_free(cache, obj1);
// 
//     // 销毁缓存
//     kmem_cache_destroy(cache);
// }
// 
// /**
//  * 测试 Slab 状态管理
//  */
// TEST(test_slab_state_management) {
//     struct kmem_cache *cache = kmem_cache_create("state_mgmt", 128, 0, NULL, NULL);
//     CHECK(cache != NULL);
// 
//     // 分配多个对象，使Slab进入partial状态
//     void *objs[3];
//     for (int i = 0; i < 3; i++) {
//         objs[i] = kmem_cache_alloc(cache);
//         CHECK(objs[i] != NULL);
//     }
// 
//     // 释放部分对象，检查Slab状态变化
//     kmem_cache_free(cache, objs[1]);
// 
//     // 销毁缓存
//     kmem_cache_destroy(cache);
// }
// 
// /**
//  * 测试伙伴系统的页分配和释放
//  */
// TEST(test_buddy_integration) {
//     struct kmem_cache *cache = kmem_cache_create("buddy_test", PAGESIZE, 0, NULL, NULL);
//     CHECK(cache != NULL);
// 
//     // 分配一页大小的对象
//     void *obj = kmem_cache_alloc(cache);
//     CHECK(obj != NULL);
// 
//     // 释放对象时，页是否能被释放
//     kmem_cache_free(cache, obj);
// 
//     kmem_cache_destroy(cache);
// }
// 
// /**
//  * 测试缓存销毁
//  */
// TEST(test_cache_destroy) {
//     struct kmem_cache *cache = kmem_cache_create("destroy_test", 32, 0, NULL, NULL);
//     CHECK(cache != NULL);
// 
//     // 分配并释放多个对象
//     for (int i = 0; i < 10; i++) {
//         void *obj = kmem_cache_alloc(cache);
//         CHECK(obj != NULL);
//         kmem_cache_free(cache, obj);
//     }
// 
//     // 销毁缓存
//     kmem_cache_destroy(cache);
// }
// 
// /**
//  * 测试对象对齐
//  */
// TEST(test_object_alignment) {
//     struct kmem_cache *cache = kmem_cache_create("align_test", 8, 8, NULL, NULL);
//     CHECK(cache != NULL);
// 
//     void *obj = kmem_cache_alloc(cache);
//     CHECK(obj != NULL);
// 
//     // 检查对齐
//     CHECK(((uintptr_t)obj % 8) == 0);
// 
//     kmem_cache_free(cache, obj);
//     kmem_cache_destroy(cache);
// }
// 
// 
// /**
//  * 测试构造和析构函数
//  */
// int ctor_dtor_counter = 0;
// 
// void test_ctor(void *obj, size_t size) {
//     ctor_dtor_counter++;
// }
// 
// void test_dtor(void *obj, size_t size) {
//     ctor_dtor_counter++;
// }
// 
// TEST(test_constructor_destructor) {
//     struct kmem_cache *cache = kmem_cache_create("ctor_dtor_test", 8, 0, test_ctor, test_dtor);
//     CHECK(cache != NULL);
// 
//     void *obj = kmem_cache_alloc(cache);
//     CHECK(obj != NULL);
//     CHECK(ctor_dtor_counter == 1);
// 
//     kmem_cache_free(cache, obj);
//     CHECK(ctor_dtor_counter == 2);
// 
//     kmem_cache_destroy(cache);
// }
// 
// /**
//  * 测试 Slab 的回收机制
//  */
// TEST(test_slab_reclaim) {
//     struct kmem_cache *cache = kmem_cache_create("reclaim_test", 64, 0, NULL, NULL);
//     CHECK(cache != NULL);
// 
//     // 分配多个对象
//     void *objs[5];
//     for (int i = 0; i < 5; i++) {
//         objs[i] = kmem_cache_alloc(cache);
//         CHECK(objs[i] != NULL);
//     }
// 
//     // 释放中间的一些对象
//     kmem_cache_free(cache, objs[2]);
//     kmem_cache_free(cache, objs[3]);
// 
//     // 再次分配对象，应该能复用空闲对象
//     void *new_obj = kmem_cache_alloc(cache);
//     CHECK(new_obj != NULL);
// 
//     kmem_cache_destroy(cache);
// }
// 
// void slab_test_all() {
//     test_cache_create_destroy();
//     test_alloc_free();
//     test_slab_state_management();
//     test_buddy_integration();
//     test_cache_destroy();
//     test_object_alignment();
//     test_constructor_destructor();
//     test_slab_reclaim();
// 
//     printf("All tests passed!\n");
// }






