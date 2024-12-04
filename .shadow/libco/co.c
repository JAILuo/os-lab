#include "co.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <assert.h>

#define CO_AMOUNT  256
#define STACK_SIZE 512

struct context {

};

enum co_status {
    CO_NEW = 1, // 新创建，还未执行过
    CO_RUNNING, // 已经执行过
    CO_WAITING, // 在 co_wait 上等待
    CO_DEAD,    // 已经结束，但还未释放资源
};

struct co {
    char name[30];
    void (*func)(void *); // co_start 指定的入口地址和参数
    void *arg;

    enum co_status status;  // 协程的状态
    struct co *    waiter;  // 是否有其他协程在等待当前协程
    //struct context context; // 寄存器现场
    jmp_buf context; // 寄存器现场
    uint8_t        stack[STACK_SIZE]; // 协程的堆栈
};

static struct co *current = NULL;
static struct co *co_list[CO_AMOUNT];;
static int co_num = 0;

__attribute__((constructor)) void init() {
  struct co* main = (struct co*)malloc(sizeof(struct co));
  strcpy(main->name, "main");
  main->status = CO_RUNNING;
  main->waiter = NULL;
  current = main;
  co_num = 1;
  memset(co_list, 0, sizeof(co_list));
  co_list[0] = main;
}

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

static inline void stack_switch_call(void *sp, void *entry, uintptr_t arg) {
    asm volatile (
#if __x86_64__
                  "movq %0, %%rsp;"
                  "movq %2, %%rdi;"
                  "call *%1"
                  :
                  : "b"((uintptr_t)sp), "d"(entry), "a"(arg)
                  : "memory"
#else
                  "movl %0, %%esp;"
                  "movl %2, 4(%0);"
                  "call *%1"
                  :
                  : "b"((uintptr_t)sp - 8), "d"(entry), "a"(arg)
                  : "memory"
#endif
    );
          asm volatile(
      #if __x86_64__
                "movq (%0), %%rdi"
                :
                : "b"((uintptr_t)sp)
                : "memory"
      #else
                "movl 0x8(%0), %%esp; movl 0x4(%0), %%ecx"
                :
                : "b"((uintptr_t)sp)
                : "memory"
      #endif
      );
}

static inline void *wrapper_(void *arg) {
    struct co *t = (struct co *)arg;
    t->func(t->name);
    return NULL;
}

struct co *co_start(const char *name, void (*func)(void *), void *arg) {
    assert(co_num < CO_AMOUNT);

    struct co *new_co = co_list[co_num];
    strcpy(new_co->name, name);
    new_co->func = func;
    new_co->arg = arg;
    new_co->status = CO_NEW;
    new_co->waiter = NULL;
    memset(new_co->stack, 0, STACK_SIZE);

    co_num++;
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


// static struct co * switch_to_co() {
//     assert(co_num != 0 && current != NULL);
//     // random switch
//     int random_index = rand() % co_num;
//     return co_list[random_index];
// }

struct co *switch_to_co() {
  int count = 0;
  for (int i = 0; i < co_num; ++i) {
    assert(co_list[i]);
    if (co_list[i]->status == CO_NEW || co_list[i]->status == CO_RUNNING) {
      ++count;
    }
  }

  int id = rand() % count, i = 0;
  for (i = 0; i < co_num; ++i) {
    if (co_list[i]->status == CO_NEW || co_list[i]->status == CO_RUNNING) {
      if (id == 0) {
        break;
      }
      --id;
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

        switch (current->status) {
        case CO_NEW:
            current->status = CO_RUNNING;
            stack_switch_call(current->stack + STACK_SIZE, current->func, (uintptr_t)(current->arg));
            // If co is here, what should it be in state? need thinking...
            // In stack_switch_call, the excute flow will switch to current->func until finish task.
            // it return here, which mean the end of task? 
            // So it should be CO_DEAD? TODO: test
            current->status = CO_DEAD;
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
