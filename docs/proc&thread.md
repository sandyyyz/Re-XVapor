# linux :  
https://www.informit.com/articles/article.aspx?p=370047  

https://www.scaler.com/topics/linux-thread/

## thread_info

![thread_info](image-35.png)

## state

![state_machine](image-36.png)

## process famaliy tree
**Each task_struct has a pointer to the parent's task_struct, named parent, and a list of children, named children.**  
比xv6的实现多了一个孩子链表

![thread](image-37.png)


### 如何实现？

process作为最小的调度和资源分配单位 change to -->  

1. process作为一个最小的资源分配单位
2. thread作为一个最小的调度单位
3. 如何分配和管理内存？哪些内存由线程共享？
4. 线程的状态机应该是怎样的？互相之间如何切换？
5. 进程和进程之间的关系是怎样的？线程和线程之间的关系是怎样的？进程和线程之间的关系又是怎样的？用怎样的数据结构维护它们之间的关系？


### thread state machine: (referencing lostwakeup)
![thstate_machine](image-38.png)  

### process state machine (referencing lostwakeup)
![procstate_machine](image-39.png)

- 线程之间完全并行，也就是可以同时在多个CPU核上并行执行，而不是局限于在进程内部。由此应该需要一个全局的调度队列，而不是进程内部的调度队列。
- 为了实现线程间的完全并行，每个线程需要有自己的trapframe，而trampoline应当还是共享的
- 由于进程不作为调度单位，所以context应该放在每一个线程中