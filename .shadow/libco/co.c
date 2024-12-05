#include "co.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <assert.h>

#define CO_AMOUNT  256
#define STACK_SIZE 1024

enum co_status {
    CO_NEW = 1, // 新创建，还未执行过
    CO_RUNNING, // 已经执行过
    CO_WAITING, // 在 co_wait 上等待
    CO_DEAD,    // 已经结束，但还未释放资源
};

struct co {
    __attribute__((aligned(16))) char name[30];
    void (*func)(void *); // co_start 指定的入口地址和参数
    void *arg;

    enum co_status status;  // 协程的状态
    struct co *    waiter;  // 是否有其他协程在等待当前协程
    jmp_buf context;        // 寄存器现场
    __attribute__((aligned(16)))uint8_t        stack[STACK_SIZE]; // 协程的堆栈
};

struct co *current = NULL;
struct co *co_list[CO_AMOUNT];;
static int co_num = 0;

// __attribute__((constructor)) void co_init() {
//     struct co* main = (struct co*)malloc(sizeof(struct co));
//     strcpy(main->name, "main");
//     main->status = CO_RUNNING;
//     main->waiter = NULL;
//     main->func = (void (*)(void *))main;
//     memset(main->stack, 0, STACK_SIZE);
// 
//     current = main;
//     memset(co_list, 0, sizeof(co_list));
//     co_list[co_num++] = main;
// }
// 
// __attribute__((destructor)) void co_exit() {
//     if (current && strcmp(current->name, "main") == 0) {
//         free(current);
//         current = NULL;
//     }
// }

static inline void stack_switch_call(void *sp, void *entry, uintptr_t arg)
{
	asm volatile(
#if __x86_64__
		"movq %%rsp, -0x10(%0); leaq -0x20(%0), %%rsp; movq %2, %%rdi ; call *%1; movq -0x10(%0) ,%%rsp;"
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
// }

//static inline void *wrapper_(void *arg) {
//    struct co *t = (struct co *)arg;
//    t->func(t->name);
//    return NULL;
//}

struct co *co_start(const char *name, void (*func)(void *), void *arg) {
    assert(co_num < CO_AMOUNT);

    struct co* new_co= (struct co*)malloc(sizeof(struct co));
    new_co->status = CO_NEW;
    strcpy(new_co->name, name);
    new_co->func = func;
    new_co->arg = arg;
    new_co->waiter = NULL;
    memset(new_co->stack, 0, STACK_SIZE);
    if (current == NULL) {
        current = (struct co *)malloc(sizeof(struct co));
		current->status = CO_RUNNING; // BUG !! 写成了 current->status==CO_RUNNING;
		current->waiter = NULL;
		strcpy(current->name, "main");
        co_list[co_num++] = current;
    }

    co_list[co_num++] = new_co;
    return new_co;
}

void co_wait(struct co *co) {
    assert(co != NULL);
    co->waiter = current;
    current->status = CO_WAITING;
    while (co->status != CO_DEAD) {
        co_yield();
    }
    free(co);
    int id = 0;
    for (id = 0; id < co_num; ++id) {
        if (co_list[id] == co) {
          break;
        }
    }
    while (id < co_num - 1) {
        co_list[id] = co_list[id+1];
        ++id;
    }
    --co_num;
    co_list[co_num] = NULL;
}


struct co *switch_to_co() {
    int count = 0;
    for (int i = 0; i < co_num; ++i) {
        assert(co_list[i]);
        if (co_list[i]->status == CO_NEW || co_list[i]->status == CO_RUNNING) {
            ++count;
        }
    }

    int idx = rand() % count, i = 0;
    for (i = 0; i < co_num; ++i) {
        if (co_list[i]->status == CO_NEW || co_list[i]->status == CO_RUNNING) {
            if (idx == 0) {
                break;
            }
        --idx;
        }
    }
    return co_list[i];
}

void co_yield(void) {
    assert(current != NULL);

    printf("co_list[0]->name: %s\n", co_list[0]->name);
    printf("co_list[1]->name: %s\n", co_list[1]->name);
    printf("co_list[2]->name: %s\n", co_list[2]->name);
    int val = setjmp(current->context);
    if (val == 0) {
        struct co *next_co = co_list[1];
        current = next_co;

        switch (next_co->status) {
        case CO_NEW:
            ((struct co volatile *)next_co)->status = CO_RUNNING;
            next_co->stack[1] = 99;
	asm volatile(
		"movq %%rsp, -0x10(%0); leaq -0x20(%0), %%rsp; movq %2, %%rdi ; call *%1; movq -0x10(%0) ,%%rsp;"
		:
		: "b"((uintptr_t)next_co->stack[STACK_SIZE - 1]), "d"(next_co->func), "a"(next_co->arg)
		: "memory"
	);
            //stack_switch_call(&(current->stack[STACK_SIZE - 1]), current->func, (uintptr_t)(current->arg));
            // If co is here, what should it be in state? need thinking...
            // In stack_switch_call, the excute flow will switch to current->func until finish task.
            // it return here, which mean the end of task? 
            // So it should be CO_DEAD? TODO: test
            ((struct co volatile *)next_co)->status = CO_DEAD;

            if (current->waiter) {
                current = current->waiter;
                longjmp(current->context, 1);
            }
            break;
        case CO_RUNNING:
            longjmp(current->context, 1);
            break;
        default:
            printf("now status is %d\n", current->status);
            break;
        }
    } else { // longjmp return 1...
        return;
    }
}
