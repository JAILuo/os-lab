# **2024 Nanjing University, Operating System: Design and Implementation**

- Course Homepage: https://jyywiki.cn/OS/2024/
- Lesson Video: https://www.bilibili.com/video/BV1Xm411f7CM


>The individual learning implementation version, without Online Judge, only local test

> Some notes taken during the lab：
> [Messy Notes](https://jailuo.github.io/notes/Course/University%20Course/jyy%20OS2024/Mini%20Labs/M1/M1.html)


# Progress
## Mini Labs
- [M1: 打印进程树 (pstree)](https://jyywiki.cn/OS/2024/labs/M1.md) ✅
  
  **实现 pstree 打印进程之间的树状的父子关系**

- [M2: 协程库 (libco)](https://jyywiki.cn/OS/2024/labs/M2.md) ✅
  
  **实现轻量级的用户态协程 (coroutine “协同程序”)**

- [M3: GPT-2 并行推理 (gpt.c)](https://jyywiki.cn/OS/2024/labs/M3.md) ❌

  **将 gpt.c 并行化**

- [M4: C Read-Eval-Print-Loop (crepl)](https://jyywiki.cn/OS/2024/labs/M4.md) ❌
  
  **实现交互式的 REPL(Read-Eval-Print-Loop)**

- [M5: 系统调用 Profiler (sperf)](https://jyywiki.cn/OS/2024/labs/M5.md)❌

  **实现系统调用 Profiler, 统计该程序中各个系统调用的占用时间**

- [M6: 文件系统格式化恢复 (fsrecov)](https://jyywiki.cn/OS/2024/labs/M6.md) ❌
  
  **从快速格式化的 FAT32 文件系统中恢复图片数据**

## OS Labs
- [L0: 为计算机硬件编程](https://jyywiki.cn/OS/2024/labs/L0.md) ✅
  
  **在 AbstractMachine 中显示一张图片**
  
  > 通过 xxd 获取的 bmp 的数据, 然后再手动拼起来(感觉自己写的不太符合要求...)

- [L1: 物理内存管理 (pmm)](https://jyywiki.cn/OS/2024/labs/L1.md) 🛠️

  **实现多处理器安全的内存分配和回收**

  > Slow path 和 fast path: 采用 buddy system 和 slab
  > 
  > ing...

- [L2: 内核线程管理 (kmt)](https://jyywiki.cn/OS/2024/labs/L2.md) ❌

  **实现操作系统内核的线程与同步, 增加中断和线程管理的功能。**




