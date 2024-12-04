#include "co.h"
#include <stdlib.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#define CO_SIZE 200

static inline void stack_switch_call(void *sp, void *entry, uintptr_t arg)
{
	asm volatile(
#if __x86_64__
		"movq %%rsp,-0x10(%0); leaq -0x20(%0), %%rsp; movq %2, %%rdi ; call *%1; movq -0x10(%0) ,%%rsp;"
		:
		: "b"((uintptr_t)sp), "d"(entry), "a"(arg)
		: "memory"
#else
		"movl %%esp, -0x8(%0); leal -0xC(%0), %%esp; movl %2, -0xC(%0); call *%1;movl -0x8(%0), %%esp"
		:
		: "b"((uintptr_t)sp), "d"(entry), "a"(arg)
		: "memory"
#endif
	);
}
enum co_status
{
	CO_NEW = 1,		// 新创建，还未执行过
	CO_RUNNING = 2, // 已经执行过
	CO_WAITING = 3, // 在 co_wait 上等待
	CO_DEAD = 4,	// 已经结束，但还未释放资源
};

#define STACK_SIZE 4 * 1024 * 8 // uintptr_t ---->  8 in x86_64
#define CANARY_SZ 2
#define MAGIC 0x55
struct co
{
	struct co *next;
	void (*func)(void *);
	void *arg;
	char name[50];
	enum co_status status;		   // 协程的状态
	struct co *waiter;			   // 是否有其他协程在等待当前协程
	jmp_buf context;			   // 寄存器现场 (setjmp.h)
	uint8_t stack[STACK_SIZE + 1]; // 协程的堆栈						   // uint8_t == unsigned char
};
// void canary_init(uint8_t ptr[])
// {
// 	for (int i = 0; i < CANARY_SZ; i++)
// 		ptr[i] = ptr[STACK_SIZE - i] = MAGIC;
// }
// void canary_check(uint8_t ptr[], struct co *co)
// {
// 	for (int i = 0; i < CANARY_SZ; i++)
// 	{
// 		if (ptr[i] != MAGIC)
// 			printf("\n %s overflow", co->name);
// 		if (ptr[STACK_SIZE - i] != MAGIC)
// 			printf("\n %s overflow", co->name);
// 	}
// }
struct co *current;
struct co *co_start(const char *name, void (*func)(void *), void *arg)
{
	struct co *start = (struct co *)malloc(sizeof(struct co));
	start->arg = arg;
	start->func = func;
	start->status = CO_NEW;
	strcpy(start->name, name);
	if (current == NULL) // init main
	{
		current = (struct co *)malloc(sizeof(struct co));
		current->status = CO_RUNNING; // BUG !! 写成了 current->status==CO_RUNNING;
		current->waiter = NULL;
		strcpy(current->name, "main");
		current->next = current;
	}
	//环形链表
	struct co *h = current;
	while (h)
	{
		if (h->next == current)
			break;

		h = h->next;
	}
	assert(h);
	h->next = start;
	start->next = current;
	return start;
}
int times = 1;
void co_wait(struct co *co)
{
	current->status = CO_WAITING;
	co->waiter = current;
	while (co->status != CO_DEAD)
	{
		co_yield ();
	}
	current->status = CO_RUNNING;
	struct co *h = current;

	while (h)
	{
		if (h->next == co)
			break;
		h = h->next;
	}
	//从环形链表中删除co
	h->next = h->next->next;
	free(co);
}
void co_yield ()
{
	if (current == NULL) // init main
	{
		current = (struct co *)malloc(sizeof(struct co));
		current->status = CO_RUNNING;
		strcpy(current->name, "main");
		current->next = current;
	}
	assert(current);
	int val = setjmp(current->context);
	if (val == 0) // co_yield() 被调用时，setjmp 保存寄存器现场后会立即返回 0，此时我们需要选择下一个待运行的协程 (相当于修改 current)，并切换到这个协程运行。
	{
		// choose co_next
		struct co *co_next = current;
		do
		{
			co_next = co_next->next;
		} while (co_next->status == CO_DEAD || co_next->status == CO_WAITING);
		current = co_next;
		if (co_next->status == CO_NEW)
		{
			assert(co_next->status == CO_NEW);
			((struct co volatile *)current)->status = CO_RUNNING; //  fogot!!!
			stack_switch_call(&current->stack[STACK_SIZE], current->func, (uintptr_t)current->arg);
			((struct co volatile *)current)->status = CO_DEAD;
			if (current->waiter)
				current = current->waiter;
		}
		else
		{
			longjmp(current->context, 1);
		}
	}
	else // longjmp returned(1) ,don't need to do anything
	{
		return;
	}
}

// #include "co.h"
// #include <stdlib.h>
// #include <stdint.h>
// #include <string.h>
// #include <stdio.h>
// #include <setjmp.h>
// #include <assert.h>
// 
// #define CO_AMOUNT  256
// #define STACK_SIZE 512
// 
// struct context {
// 
// };
// 
// enum co_status {
//     CO_NEW = 1, // 新创建，还未执行过
//     CO_RUNNING, // 已经执行过
//     CO_WAITING, // 在 co_wait 上等待
//     CO_DEAD,    // 已经结束，但还未释放资源
// };
// 
// struct co {
//     char *name;
//     void (*func)(void *); // co_start 指定的入口地址和参数
//     void *arg;
// 
//     enum co_status status;  // 协程的状态
//     struct co *    waiter;  // 是否有其他协程在等待当前协程
//     //struct context context; // 寄存器现场
//     jmp_buf context; // 寄存器现场
//     uint8_t        stack[STACK_SIZE]; // 协程的堆栈
// };
// 
// static struct co *current = NULL;
// static struct co *co_list[CO_AMOUNT];;
// static int co_num = 0;
// 
// __attribute__((constructor))
// void co_init(void) {
//    strcpy(current->name, "main");
//    current->func = NULL;
//    current->arg = NULL;
//    current->status = CO_RUNNING;
//    current->waiter = NULL;
//    memset(current->stack, 0, STACK_SIZE);
//    co_list[co_num++] = current;
// }
// 
// static inline void stack_switch_call(void *sp, void *entry, uintptr_t arg) {
//     asm volatile (
// #if __x86_64__
//                   "movq %0, %%rsp;"
//                   "movq %2, %%rdi;"
//                   "call *%1"
//                   :
//                   : "b"((uintptr_t)sp), "d"(entry), "a"(arg)
//                   : "memory"
// #else
//                   "movl %0, %%esp;"
//                   "movl %2, 4(%0);"
//                   "call *%1"
//                   :
//                   : "b"((uintptr_t)sp - 8), "d"(entry), "a"(arg)
//                   : "memory"
// #endif
//     );
//           asm volatile(
//       #if __x86_64__
//                 "movq (%0), %%rdi"
//                 :
//                 : "b"((uintptr_t)sp)
//                 : "memory"
//       #else
//                 "movl 0x8(%0), %%esp; movl 0x4(%0), %%ecx"
//                 :
//                 : "b"((uintptr_t)sp)
//                 : "memory"
//       #endif
//       );
// }
// 
// static inline void *wrapper_(void *arg) {
//     struct co *t = (struct co *)arg;
//     t->func(t->name);
//     return NULL;
// }
// 
// struct co *co_start(const char *name, void (*func)(void *), void *arg) {
//     assert(co_num < CO_AMOUNT);
// 
//     struct co *new_co = co_list[co_num];
//     strcpy(new_co->name, name);
//     new_co->func = func;
//     new_co->arg = arg;
//     new_co->status = CO_NEW;
//     new_co->waiter = NULL;
//     memset(new_co->stack, 0, STACK_SIZE);
// 
//     co_num++;
//     return new_co;
// }
// 
// void co_wait(struct co *co) {
//   assert(co != NULL);
//   co->waiter = current;
//   current->status = CO_WAITING;
//   while (co->status != CO_DEAD) {
//     co_yield();
//   }
//   free(co);
//   int id = 0;
//   for (id = 0; id < co_num; ++id) {
//     if (co_list[id] == co) {
//       break;
//     }
//   }
//   while (id < co_num - 1) {
//     co_list[id] = co_list[id+1];
//     ++id;
//   }
//   --co_num;
//   co_list[co_num] = NULL;
// }
// 
// 
// // static struct co * switch_to_co() {
// //     assert(co_num != 0 && current != NULL);
// //     // random switch
// //     int random_index = rand() % co_num;
// //     return co_list[random_index];
// // }
// 
// struct co *switch_to_co() {
//   int count = 0;
//   for (int i = 0; i < co_num; ++i) {
//     assert(co_list[i]);
//     if (co_list[i]->status == CO_NEW || co_list[i]->status == CO_RUNNING) {
//       ++count;
//     }
//   }
// 
//   int id = rand() % count, i = 0;
//   for (i = 0; i < co_num; ++i) {
//     if (co_list[i]->status == CO_NEW || co_list[i]->status == CO_RUNNING) {
//       if (id == 0) {
//         break;
//       }
//       --id;
//     }
//   }
//   return co_list[i];
// }
// 
// void co_yield(void) {
//     assert(current != NULL);
// 
//     int val = setjmp(current->context);
//     if (val == 0) {
//         struct co *next_co = switch_to_co();
//         current = next_co;
// 
//         switch (current->status) {
//         case CO_NEW:
//             current->status = CO_RUNNING;
//             stack_switch_call(current->stack + STACK_SIZE, current->func, (uintptr_t)(current->arg));
//             // If co is here, what should it be in state? need thinking...
//             // In stack_switch_call, the excute flow will switch to current->func until finish task.
//             // it return here, which mean the end of task? 
//             // So it should be CO_DEAD? TODO: test
//             current->status = CO_DEAD;
//             if (current->waiter) {
//                 current = current->waiter;
//                 longjmp(current->context, 1);
//             }
//             break;
//         case CO_RUNNING:
//             longjmp(current->context, 1);
//             break;
//         default:
//             printf("now status is %d\n", current->status);
//             break;
//         }
//     } else { // longjmp return 1...
//         return;
//     }
// }
