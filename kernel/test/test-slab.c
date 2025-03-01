#include <os/common.h>
#include <os/buddy.h>
#include <os/slab.h>
#include <test/test-framework.h>

// 测试用例实现
TEST(test_cache_create) {
    struct kmem_cache *cache = kmem_cache_create("test", 64, 0);
    CHECK(cache != NULL, "cache should not be NULL");
    CHECK(cache->obj_size == ALIGN_UP(64, DEFAULT_ALIGN),
        "cache->obj_size error");
    CHECK(!list_empty(&cache->slabs_free), 
          "slabs_free should not be empty");
}

TEST(test_alloc_free) {
    struct kmem_cache *cache = kmem_cache_create("alloc_test", 128, 0);
    void *obj = kmem_cache_alloc(cache);
    CHECK(obj != NULL, "obj should not ne NULL");
    kmem_cache_free(cache, obj);

    CHECK(!list_empty(&cache->slabs_free),
          "slabs_free should not be empty");
}

TEST(test_boundary_sizes) {
    // 最小尺寸测试
    struct kmem_cache *min_cache = kmem_cache_create("min", MIN_OBJECT_SIZE, 0);
    CHECK(min_cache->obj_size == ALIGN_UP(MIN_OBJECT_SIZE, DEFAULT_ALIGN),
          "min_cache->obj_size error");

    // 最大尺寸测试
    __attribute__((unused))struct kmem_cache *max_cache = kmem_cache_create("max", MAX_OBJECT_SIZE, 0);
    int objs_per_slab = (PAGESIZE - sizeof(struct slab)) / MAX_OBJECT_SIZE;
    CHECK(objs_per_slab >= 1, "objs_per_slab(max) error");
}

TEST(test_mass_allocation) {
    // 集成测试：批量分配释放
    struct kmem_cache *cache = kmem_cache_create("mass", 256, 0);
    //const int NUM = 14;
    const int NUM = 100;
    void *objects[NUM];

    // 分配阶段
    for (int i = 0; i < NUM; ++i) {
        //printf("now i %d\n", i);
        objects[i] = kmem_cache_alloc(cache);
        CHECK(objects[i] != NULL, "obj should not be NULL");
    }

    // 释放阶段
    for (int i = 0; i < NUM; ++i) {
        kmem_cache_free(cache, objects[i]);
    }

    // 验证slab状态恢复
    CHECK(list_empty(&cache->slabs_full), "slabs_full should be empty");
}

TEST(test_perf) {
    // 性能测试：分配/释放吞吐量
    struct kmem_cache *cache = kmem_cache_create("perf", 64, 0);
    const int LOOPS = 10000;
    unsigned long start = TIME();

    for (int i = 0; i < LOOPS; ++i) {
        void *obj = kmem_cache_alloc(cache);
        kmem_cache_free(cache, obj);
    }

    unsigned long duration = (TIME() - start );
    printf(" %d ops in %u cycles\n", LOOPS, duration);
}

TEST(test_stress) {
    struct kmem_cache *cache = kmem_cache_create("stress", 512, 0);
    for (int i = 0; i < 1000000; ++i) {
        void *obj = kmem_cache_alloc(cache);
        CHECK(obj != NULL, "obj should not be NULL");
        kmem_cache_free(cache, obj);
    }
}

//TEST(test_stress_mixed) {
//    struct kmem_cache *caches[4];
//    const char *names[] = {"stress-64", "stress-128", "stress-256", "stress-512"};
//    const size_t sizes[] = {64, 128, 256, 512};
//
//    // 初始化多个缓存
//    for (int i = 0; i < 4; i++) {
//        caches[i] = kmem_cache_create(names[i], sizes[i], 0);
//        CHECK(caches[i] != NULL, "Failed to create %s cache", names[i]);
//    }
//
//    // 百万次混合操作
//    for (int i = 0; i < 1000000; ++i) {
//        int idx = rand() % 4; // 随机选择缓存
//        void *obj = kmem_cache_alloc(caches[idx]);
//        CHECK(obj != NULL, "Alloc failed in %s", names[idx]);
//        kmem_cache_free(caches[idx], obj);
//    }
//
//    // 销毁所有缓存
//    for (int i = 0; i < 4; i++) {
//        kmem_cache_destroy(caches[i]);
//    }
//}

TEST(test_random_ops) {
    struct kmem_cache *cache = kmem_cache_create("random", 96, 0);
    void *pool[100];
    int count = 0;

    for (int i = 0; i < 10000; ++i) {
        if (count < 100 && (rand() % 2)) { // 随机分配
            pool[count++] = kmem_cache_alloc(cache);
            CHECK(pool[count - 1] != NULL, "pool should not be NULL");
        } else if (count > 0) { // 随机释放
            kmem_cache_free(cache, pool[--count]);
        }
    }
}


void slab_test_all() {
    // 单元测试
    UNIT_TEST_SUITE(
        test_cache_create,
        test_alloc_free,
        test_boundary_sizes
    );

    // 集成测试
    INTEG_TEST_SUITE(
        test_mass_allocation
    );

    // 压力测试
    STRESS_TEST_SUITE(
        test_stress,
        //test_stress_mixed
    );

    // 性能测试
    PERF_TEST_SUITE(
        test_perf,
        //test_perf_advanced
    );

    // 随机操作测试
    RAND_TEST_SUITE(
        test_random_ops,
        //test_random_advanced
    );

    // 最终汇总
    printf("\n" ANSI_FG_GREEN "[======================]"
           ANSI_FG_GREEN "\n[  ALL TESTS PASSED  ]\n"
           "[======================]" ANSI_NONE "\n");
}

// 主测试入口
// void slab_test_all() {
// 
//     printf("\n" ANSI_FG_CYAN "[Unit Tests]" ANSI_NONE "\n");
//     RUN_TEST(test_cache_create);
//     RUN_TEST(test_alloc_free);
//     RUN_TEST(test_boundary_sizes);
//     printf("[Unit Tests] passed!\n");
// 
//     printf("\n" ANSI_FG_CYAN "[Integration Tests]" ANSI_FG_CYAN "\n");
//     RUN_TEST(test_mass_allocation);
//     printf("[Integration Tests] passed!\n");
// 
//     printf("\n====[Performance Tests]====\n");
//     RUN_TEST(test_perf);
//     printf("[Performance Tests] end!\n");
// 
//     printf("\n====[Stress Tests]====\n");
//     RUN_TEST(test_stress);
//     printf("[Stress Tests] passed!\n");
// 
//     printf("\n====[Random_ops Tests]====\n");
//     RUN_TEST(test_random_ops);
//     printf("[Random_ops Tests] passed!\n");
//     
//     //check_leaks();
//     printf(ANSI_FG_GREEN "All tests passed!" ANSI_NONE "\n");
// }


