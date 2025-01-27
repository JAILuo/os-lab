#include "co.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <assert.h>

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
//
// Update: 
// so strange, use canary to check serval times and pass?!!
// even if and switch version all paseed!
//
// Update2:
//   Use the released flag to manage resource release.

#define CO_AMOUNT  256
#define STACK_SIZE 1024 * 16

enum co_status {
    CO_NEW = 1, // 新创建，还未执行过
    CO_RUNNING, // 已经执行过
    CO_WAITING, // 在 co_wait 上等待
    CO_DEAD,    // 已经结束，但还未释放资源
};

/*
 * Notice!!!
 * The location of the struct members, 
 * which needs to fllow the alignment requirements of the ISA
 * 
 * @note: released
 *  It's okay under 64 bits, 
 *  but it can only pass under 32 bits if you put it here, 
 *  otherwise segmentation fault?
 *  Update: 
 *    The reason is that both the co_wait and the co_wrapper 
 *    have carried out resource recycling, and even accessed
 *    the relevant heap area after free.
 *    correct: 
 *      co_warpper only set the released flag, 
 *      and the release of specific resources was deferred to the co_wait.
 */
struct __attribute__((aligned(16))) co {
    char name[30]__attribute__((aligned(16)));

    int released ;
    void (*func)(void *); // 真正的函数入口
    void *arg;
    void (*entry)(void *); // co_start 指定的入口地址和参数
    void *entry_arg;

    enum co_status status;  // 协程的状态
    struct co *    waiter;  // 是否有其他协程在等待当前协程

    jmp_buf context;        // 寄存器现场
    uint8_t        stack[STACK_SIZE]__attribute__((aligned(16))); // 协程的堆栈
};
struct co *current = NULL;
struct co *co_list[CO_AMOUNT];;
static int co_num = 0;

// learn from jyy's OS course: https://jyywiki.cn/OS/2024/lect13.md
//#define CANARY_CHECK
#define CANARY_SZ 4
#define MAGIC 0x55555555
#define BOTTOM (STACK_SIZE / sizeof(uint32_t) - 1)

#define STRINGIFY(s)        #s
#define TOSTRING(s)         STRINGIFY(s)
#define panic_on(cond, s) \
  ({ if (cond) { \
      printf("Panic: "); printf(s); \
      printf(" @ " __FILE__ ":" TOSTRING(__LINE__) "  \n"); \
      assert(0); \
    } })

#ifdef CANARY_CHECK
void canary_init(uint8_t stack[]) {
    uint32_t *ptr = (uint32_t *)stack;
    for (int i = 0; i < CANARY_SZ; i++)
        ptr[BOTTOM - i] = ptr[i] = MAGIC;
}

void canary_check(struct co *co) {
    uint32_t *ptr = (uint32_t *)(co->stack);
    for (int i = 0; i < CANARY_SZ; i++) {
        // printf("in canary_check, co->name: %s\n", co->name);
        panic_on(ptr[BOTTOM - i] != MAGIC, "underflow");
        panic_on(ptr[i] != MAGIC, "overflow");
    }
}
#else
void canary_init(uint8_t stack[]) {
}

void canary_check(struct co *co) {
}
#endif

__attribute__((constructor)) void co_init() {
    struct co* main = (struct co*)malloc(sizeof(struct co));
    strcpy(main->name, "main");
    main->status = CO_RUNNING;
    main->waiter = NULL;
    main->func = (void (*)(void *))main;
    memset(main->stack, 0, STACK_SIZE);
    canary_init(main->stack);

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
        "movq %2, %%rdi\n"          // set function parameter
        "call *%1\n"                // call function(entry)
        "movq -0x10(%0) ,%%rsp\n"   // restore the original rsp
        "movq (%0), %%rdi\n"        // restore the original parameter
		:
		: "b"((uintptr_t)sp), "d"(entry), "a"(arg)
		: "cc", "memory"
#else
        // The IA32 parameters are passed in the stack,
        // so just need to save the esp.
		"movl %%esp, (%0);\n"
        "leal -0x4(%0), %%esp\n"
        "andl $-4, %%esp\n"  // Ensure stack is 4-byte aligned
        "movl %2, -0x4(%0)\n"
        "call *%1\n"
        "movl (%0), %%esp\n"
		:
		: "b"((uintptr_t)sp), "d"(entry), "a"(arg)
		: "memory"
#endif
	);
}

static inline void co_wrapper(void *arg) {
    struct co *co = (struct co *)arg;

    co->func(co->arg);

    co->status = CO_DEAD;
    co->released = 1;
}

/*
 * Where is the entry of the thread function without wrapper?
 * And where is the end?
 *  main ->
 *    co_wait -> 
 *      co_yield -> 
 *        stack_switch_call -> 
 *          co_wrapper ->
 *            ...co running...
 *          co_wrapper ->
 *            free(co) ->
 *        stack_switch_call ->
 *          co_yield ->
 *      co_wait ->
 *        free(co) -> !!!
 */
struct co *co_start(const char *name, void (*func)(void *), void *arg) {
    assert(co_num < CO_AMOUNT);

    struct co* new_co = (struct co*)malloc(sizeof(struct co));
    memset(new_co, 0, sizeof(struct co));

    new_co->status = CO_NEW;
    strcpy(new_co->name, name);
    new_co->func = func;
    new_co->arg = arg;
    new_co->waiter = NULL;
    new_co->released = 0;
    memset(new_co->stack, 0, STACK_SIZE);
    canary_init(new_co->stack);

    // 设置包裹函数为协程的入口函数
    new_co->entry = co_wrapper;
    new_co->entry_arg = new_co;

    co_list[co_num++] = new_co;
    return new_co;
}

void co_wait(struct co *co) {
    assert(co != NULL);

    co->waiter = current;
    current->status = CO_WAITING;
    while (co->status != CO_DEAD) {
        canary_check(current);
        co_yield();
    }

    // Removes the exited coroutine from the list of coroutines.
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

    // recycle resources
    if (co->released) {
        co->released = 0;
        free(co);
        co = NULL;
    }

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

        if (next_co->status == CO_NEW) {
            next_co->status = CO_RUNNING;

            canary_check(next_co);
            stack_switch_call(next_co->stack + sizeof(next_co->stack), next_co->entry, (uintptr_t)(next_co->entry_arg));
            //stack_switch_call(next_co->stack + sizeof(next_co->stack), next_co->func, (uintptr_t)(next_co->arg));

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
    } else { // longjmp return 1...
    }
}

