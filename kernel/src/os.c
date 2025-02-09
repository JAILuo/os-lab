#include <common.h>
static void os_init()
{
	pmm->init();
}

static void os_run()
{
	for (const char *s = "Hello World from CPU #*\n"; *s; s++)
	{
#ifndef TEST
		putch(*s == '*' ? '0' + cpu_current() : *s);
#else
		static size_t cpucnt = 0;
		putchar(*s == '*' ? '0' + cpucnt++ : *s);
#endif
	}
	void *add1 = pmm->alloc(1020);
	if (add1 == NULL)
	{
		printf("add is NULL");
		halt(1);
	}
	else
		printf("add1: %x\n", add1);
	
    int *add1_int=(int *)add1;
	*add1_int=9876;
	*(add1_int+(1020-4)/4)=114514;

	void *add2 = pmm->alloc(20);
	if (add2 == NULL)
	{
		printf("add2 is NULL");
		halt(1);
	}
	else
		printf("add2: %x\n", add2);

	void *add3 = pmm->alloc(512);
	if (add3 == NULL)
	{
		printf("add3 is NULL");
		halt(1);
	}
	else
		printf("add3: %x\n", add3);

	int *add3_int=(int *)add3;
	*add3_int=543210;
	*(add3_int+(512-4)/4)=114514;

	pmm->free(add2);

	assert(*add1_int==9876);
	assert(*(add1_int+(1020-4)/4)==114514);
	assert(*add3_int==543210);
	assert(*(add3_int+(512-4)/4)==114514);

	pmm->free(add1);

	assert(*add3_int==543210);
	assert(*(add3_int+(512-4)/4)==114514);

	pmm->free(add3);

	while(true);
}

MODULE_DEF(os) = {
	.init = os_init,
	.run = os_run,
};

// #include <common.h>
// 
// typedef int lock_t;
// 
// extern lock_t big_lock;
// void lockinit(int *lock);
// void spin_lock(int *lock);
// void spin_unlock(int *lock);
// 
// #define MAGIC (0x55)
// #define TEST_NUM (128)
// 
// // for test buddy
// static int choose_memory_block(void) {
//     //int small_size = 256, medium_size = 4 * 1024, large_size = 1024 * 1024;
//     int medium_size = 4 * 1024, large_size = 1024 * 1024;
//     int memory_size = 0;
//     int probabilities = rand() % 100;
//     switch (probabilities) {
//     // case 0 ... 4: // alloc small memory block: 5%
//     //     memory_size = rand() % small_size;
//     //     break;
//     // case 5 ... 89: // alloc medium memory block: 85%
//     //     memory_size = small_size + rand() % (medium_size - small_size);
//     //     break;
//     case 1 ... 99: // alloc large memory block: 10% 
//         memory_size = medium_size + (rand() % (large_size - medium_size));
//         break;
//     default: 
//         panic("memory_size error");
//         break;
//     }
//     return memory_size;
// }
// 
// // static int choose_memory_block(void) {
// //     int small_size = 256, medium_size = 4 * 1024, large_size = 1024 * 1024;
// //     int memory_size = 0;
// //     int probabilities = rand() % 100;
// //     switch (probabilities) {
// //     case 0 ... 89: // alloc small memory block: 90%
// //         memory_size = rand() % small_size;
// //         break;
// //     case 90 ... 97: // alloc medium memory block: 8%
// //         memory_size = small_size + rand() % (medium_size - small_size);
// //         break;
// //     case 98 ... 99: // alloc large memory block: 2% 
// //         memory_size = medium_size + (rand() % (large_size - medium_size));
// //         break;
// //     default: 
// //         panic("memory_size error");
// //         break;
// //     }
// //     return memory_size;
// // }
// 
// void test_kalloc(char *array[], int array_size[]) {
//     int allocated = 0;
//     for (int i = 0; i < TEST_NUM; i++) {
//         if (array[i] == NULL) {
//             array_size[i] = choose_memory_block();
//             array[i] = (char *)pmm->alloc(array_size[i]);
// 
//             panic_on(array[i] == NULL, "Allocation failed: out of memory");
//             printf("CPU #%d: Allocated 0x%x bytes at %p (slot %d)\n",
//                    cpu_current(), array_size[i], array[i], i);
// 
//             // spin_lock(&big_lock);
//             memset(array[i], MAGIC + i, array_size[i]);
//             // spin_unlock(&big_lock);
// 
//             allocated = 1;
//             break;
//         }
//     }
//     if (!allocated)
//         printf("All slots full. Skipping allocation.\n");
// }
// 
// void test_kfree(char *array[], int array_size[]) {
//     int indices[TEST_NUM];
//     int count = 0;
// 
//     // 收集所有已分配块的索引
//     for (int i = 0; i < TEST_NUM; i++) {
//         if (array[i] != NULL) {
//             indices[count++] = i;
//         }
//     }
// 
//     if (count == 0) {
//         printf("No blocks to free.\n");
//         return;
//     }
// 
//     // 随机选择一个块释放
//     int selected = indices[rand() % count];
//     char *block = array[selected];
//     int size = array_size[selected];
// 
//     // 检查内存是否损坏
//     spin_lock(&big_lock); // 确保检查期间内存不被修改
//     for (int i = 0; i < size; i++) {
//         if (block[i] != (char)(MAGIC + selected)) { // 检查每个字节
//             spin_unlock(&big_lock);
//             printf("Memory corruption in block %d at offset %d\n", selected, i);
//             panic("Memory corruption in block  at offset");
//         }
//     }
//     spin_unlock(&big_lock);
// 
//     // 释放内存块
//     pmm->free(block);
//     array[selected] = NULL;
//     array_size[selected] = 0;
// 
//     printf("CPU #%d: Freed block at %p (slot %d)\n",
//            cpu_current(), block, selected);
// }
// 
// void test_kalloc_stress() {
//     for (int i = 0; i < 10; i++) {
//         void *ptr = pmm->alloc(128); // 分配 128 字节
//        if (ptr == NULL) {
//             printf("Allocation failed at iteration %d\n", i);
//             break;
//         }
//         pmm->free(ptr); // 释放内存
//     }
// }
// 
// void test_pmm() {
//     char *array[TEST_NUM] = {NULL};
//     int array_size[TEST_NUM] = {0};
// 
//     while (1) {
//         int alloc_or_free = rand() % 2;
//         if (alloc_or_free == 1) { // Allocate test
//             test_kalloc(array, array_size);
//         } else { // Free test
//             test_kfree(array, array_size);
//         }
//     }
// }
// 
// // long volatile sum = 0;
// // #define T        3
// // #define N  100000
// // 
// // 
// // void T_sum(int tid) {
// //     for (int i = 0; i < N; i++) {
// //         spin_lock(&big_lock);
// // 
// //         // This critical section is even longer; but
// //         // it should be safe--the world is stopped.
// //         // We also marked sum as volatile to make
// //         // sure it is loaded and stored in each
// //         // loop iteration.
// //         for (int _ = 0; _ < 10; _++) {
// //             sum++;
// //         }
// // 
// //         spin_unlock(&big_lock);
// //     }
// // 
// //     printf("Thread %d: sum = %d\n", tid, sum);
// // }
// 
// 
// static void os_init() {
//     pmm->init();
// }
// 
// static void os_run() {
//     for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
//         putch(*s == '*' ? '0' + cpu_current() : *s);
//     }
//     
//     // int i = 1;
//     // T_sum(i);
//     // printf("sum  = %d\n", sum);
//     // printf("%d*n = %d\n", T * 10, T * 10L * N);
// 
// 
//     // test_kalloc_stress();
//     test_pmm();
// 
//     while (1) ;
// }
// 
// MODULE_DEF(os) = {
//     .init = os_init,
//     .run  = os_run,
// };
