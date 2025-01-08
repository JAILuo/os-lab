#include "co.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <assert.h>

#define IF_VERISON
#define CO_AMOUNT  256
#define STACK_SIZE 1024 * 8

// The following bugs are on 64-bit machines, not considering 32-bit.
//
// #define STACK_SIZE 1024 * 4 Segmentation fault, 
// but is it okay to adjust it to something greater than 4KB(and I choose 16KB)?
// the size of stack maybe too small?
// However, after changing the size of the large stack, 
// it is only improved to be able to output X/Y190 before segmentation fault...

// further, add printf after switch_to_co in co_yield func, 
// then it was able to pass all the tests! It's weird!
//
// // Besides, the size of stack something wrong, and the above 8kb is expired, 9KB is ok(choose 16KB)...
// // stack size again?



enum co_status {
    CO_NEW = 1, // 新创建，还未执行过
    CO_RUNNING, // 已经执行过
    CO_WAITING, // 在 co_wait 上等待
    CO_DEAD,    // 已经结束，但还未释放资源
};

struct __attribute__((aligned(16))) co {
    char name[30]__attribute__((aligned(16)));
    void (*func)(void *); // co_start 指定的入口地址和参数
    void *arg;

    enum co_status status;  // 协程的状态
    struct co *    waiter;  // 是否有其他协程在等待当前协程
    jmp_buf context;        // 寄存器现场
    //uint8_t        stack[STACK_SIZE]; // 协程的堆栈
    uint8_t        stack[STACK_SIZE]__attribute__((aligned(16))); // 协程的堆栈
};

struct co *current = NULL;
struct co *co_list[CO_AMOUNT];;
static int co_num = 0;

__attribute__((constructor)) void co_init() {
    struct co* main = (struct co*)malloc(sizeof(struct co));
    strcpy(main->name, "main");
    main->status = CO_RUNNING;
    main->waiter = NULL;
    main->func = (void (*)(void *))main;
    memset(main->stack, 0, STACK_SIZE);

    current = main;
    memset(co_list, 0, sizeof(co_list));
    co_list[co_num++] = main;
}

__attribute__((destructor)) void co_exit() {
    if (current && strcmp(current->name, "main") == 0) {
        free(current);
        current = NULL;
    }
}

static inline void stack_switch_call(void *sp, void *entry, uintptr_t arg) {
	asm volatile(
#if __x86_64__
        "movq %%rdi, (%0)\n"
		"movq %%rsp, -0x10(%0)\n"   // save current rsp
        "leaq -0x20(%0), %%rsp\n"   // switch to new stack
        "andq $-16, %%rsp\n"        // Ensure stack is 16-byte aligned"
        "movq %2, %%rdi\n"          // set parameter
        "call *%1\n"                // call function(entry)
        "movq -0x10(%0) ,%%rsp\n"   // restore the original rsp
        "movq (%0), %%rdi\n"        // restore the original parameter
		:
		: "b"((uintptr_t)sp), "d"(entry), "a"(arg)
		: "cc", "memory"
#else
        // The IA32 parameters are passed in the stack,
        // so I just need to save the esp?
		"movl %%esp, -0x8(%0);\n"
        "leal -0xC(%0), %%esp\n"
        "andl $-4, %%esp\n"  // Ensure stack is 4-byte aligned
        "movl %2, -0xC(%0)\n"
        "call *%1\n"
        "movl -0x8(%0), %%esp"
		:
		: "b"((uintptr_t)sp), "d"(entry), "a"(arg)
		: "memory"
#endif
	);
}

// some bugs with wrapper_
// while using this func, Segmentation fault...
static inline void wrapper_(void *arg) {
    struct co *t = (struct co *)arg;
    t->func(t->name);
}
// Where is the entry of the thread function without wrapper, 
// and where is it after returning
// co_wait->co_yield->stack_switch_call

struct co *co_start(const char *name, void (*func)(void *), void *arg) {
    assert(co_num < CO_AMOUNT);

    struct co* new_co = (struct co*)malloc(sizeof(struct co));
    memset(new_co, 0, sizeof(struct co));

    new_co->status = CO_NEW;
    strcpy(new_co->name, name);
    new_co->func = func;
    new_co->arg = arg;
    new_co->waiter = NULL;
    memset(new_co->stack, 0, STACK_SIZE);

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


// learn by GPT, random select
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
                // printf("i in switch_to_co: %d\n", i);
                break;
            }
        --idx;
        }
    }
    return co_list[i];
}

void co_yield(void) {
    assert(current != NULL);

    int val = setjmp(current->context);
    if (val == 0) {
        struct co *next_co = switch_to_co();
        current = next_co;

#ifdef IF_VERISON
        if (next_co->status == CO_NEW) {
            next_co->status = CO_RUNNING;

            stack_switch_call(next_co->stack + sizeof(next_co->stack), next_co->func, (uintptr_t)(next_co->arg));

            ((struct co volatile *)next_co)->status = CO_DEAD;
            if (current->waiter) {
                current = current->waiter;
                longjmp(current->context, 1);
            }
            co_yield();
        } else if(next_co->status == CO_RUNNING) {
            longjmp(next_co->context, 1);
        } else {
            assert(0);
        }

#else // SWITCH_VERSION
        switch (next_co->status) {
        case CO_NEW:
            ((struct co volatile *)next_co)->status = CO_RUNNING;

            stack_switch_call(&(next_co->stack[STACK_SIZE - 1]), next_co->func, (uintptr_t)(next_co->arg));

            // If co is here, what should it be in state? need thinking...
            // In stack_switch_call, the excute flow will switch to current->func until finish task.
            // it return here, which mean the end of task? 
            // So it should be CO_DEAD? yes.
            ((struct co volatile *)next_co)->status = CO_DEAD;

            if (current->waiter) {
                current = current->waiter;
                longjmp(current->context, 1);
            }
            co_yield();
            break;
        case CO_RUNNING:
            longjmp(current->context, 1);
            break;
        default:
            printf("now status is %d\n", current->status);
            assert(0);
            break;
        }
#endif
    } else { // longjmp return 1...
    }
}
