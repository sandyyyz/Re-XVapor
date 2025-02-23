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
6. 实现线程之后，fork()该复制哪些部分？-- 先默认拷贝leader-thread?
7. 线程的两态切换和上下文切换该如何做？和进程有何不同？

### thread state machine: (referencing lostwakeup)
![thstate_machine](image-38.png)  

thread states:  
1. unused   ：线程init后视为unused
2. used     ：alloc_thread创建的线程,但是尚未能运行（还未完全创建完毕）
3. runnable ：创建完成并且准备可以运行的线程
4. running  ：每个cpu进入thread_shed()之后，从runnable队列中选择线程进行调度，转为running
5. sleeping ：

### process state machine (referencing lostwakeup)
![procstate_machine](image-39.png)

process state:  
1. unused   :资源已经被回收的进程
2. used     :只要进程上存在活着的线程，就将其视为used
3. zombie   :资源还未被父进程回收的进程

 **remember to change the state !!**, use the function `` tcb_q_change_state `` or  `` pcb_q_change_state ``

- 线程之间完全并行，也就是可以同时在多个CPU核上并行执行，而不是局限于在进程内部。由此应该需要一个全局的调度队列，而不是进程内部的调度队列。
- 为了实现线程间的完全并行，每个线程需要有自己的trapframe，而trampoline应当还是共享的
- 由于进程不作为调度单位，所以context应该放在每一个线程中
- swtch.s 需要修改，需要切换现成的上下文而非进程的上下文
- sched() and scheduler() ----> thread_sched() and scheduler() ,因为调度单位变化了，我们不再调度进程
- trap流程中所有trap的实体由进程改为线程


线程调度需要修改的部分:
1. initproc切换到sched()，以线程为执行单位进行调度
2. 注意原本进程的上下文应保存在线程中，主要包括trapframe中的寄存器状态


## exit

exit()退出整个进程，包括其所有子进程和线程，并且回收所有分配的资源
而thread_exit()只退出单个线程

![exit](image-41.png)

## wait 

![wait](image-43.png)

## 关于线程切换
swtch(&save_des, &des_context)存储callee-saved regs, 然后切换到目标的context：

![swtch](image-42.png)

## wait_lock

Helps wait avoid lost wakeups

- wait函数需要找到自己的子进程，然后再进入sleep阶段，让出时钟，等待子进程唤醒。如果在 ***父进程尚且处在wait函数中，还未改变自己的状态为sleeping时*** 子进程退出，并且尝试唤醒了父进程，这个时候就会发生*lostwakeup*，所以为了防止这种情况：

1. 子进程在exit函数中wakeup父进程之前，先获取wait_lock
![child_exit](image-47.png)
2. 父进程在sleep中，先获取进程锁，再释放wait_lock，保证父进程的状态不被改变。在改变完状态，即进入睡眠状态后，**带进程锁**进入`` sched() ``函数进行调度。该进程锁将在context swtch之后释放：
![跨进程锁](image-44.png)
- 为什么context swtch之后原有的进程锁仍然可以访问？：  
- **sp（栈指针） 只影响当前正在运行的栈。
但是 p->lock 是在 p 的 struct proc 里，而 struct proc 存在堆或静态区，不在栈上，所以 p->lock 不受 sp 影响。**
- 注意，调用swtch时，ra和sp寄存器的切换实际上完成了控制流和数据流的切换
![parent_sleep](image-45.png)
3. 子进程在wakeup父进程之前，必须先获取父进程锁
![child_wakeup](image-46.png)