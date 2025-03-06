## call number 

The system call number of XV6 is different from the required one (a small pitfall), just change it to the required one  
(call write before calling sys_times here)
![times_call_number](image-29.png)

## sleep syscall
The test file seems to use ``timeval`` instead of ``timespec``
 ![struct timeval](image-30.png)


## other 
1. be careful about duplicate file names when ``mkfs``, may lead to unknown error




## debug 记录

### thread.1

![panic_thread.1.1](image-52.png)
![panic_thread.1.2](image-51.png)
![panic_thread.1.3](image-53.png)

p->thread == null!!

![panic_thread.1.4](image-54.png)
![panic_thread.1.5](image-55.png)

![panic_thread.1.6](image-56.png)
scheduler刚开中断，c-> = 0时直接进kerneltrap,导致myproc()空指针引用
是时钟中断！
![panic_thread.1.7](image-57.png)

调用``myproc()``必须保证c->t != null!!
![panic_thread_1.8](image-58.png)

改为
``` c
  if(which_dev == 2 && mythread() != 0 && mythread()->state == TCB_RUNNING) {
    myproc()->ktime++;  
    thread_yield();
  }

```

### thread.2

![panic_thread.2.1](image-59.png)

![panic_thread.2.2](image-60.png)

![panic_thread.2.3](image-61.png)

![panic_thread.2.4](image-62.png)

q == null!

原因是没有`tcb_running queue`，但是使用了`tcb_q_change_state()`
因为每一个running线程一定由CPU保存，所以不需要一个额外队列保存

### thread.3

![thread.3.1](image-63.png)
lost wakeup

push sleeping queue没问题，问题应该出在遍历queue
原来是调用处写错了member name
![thread.3.2](image-64.png)


### thread.4

`usertrapret()`之后卡住

![thread.4.3](image-66.png)
![thread.4.4](image-67.png)
![thread.4.5](image-68.png)
![thread.4.6](image-69.png)
0x0处的代码：
![thread.4.11](image-74.png)  
用户页表 :
![thread.4.12](image-75.png)
![thread.4.14](image-77.png)

！！！ 问题在于sret之后不知道发生了什么。但是用户触发了一个外部中断![external intr](image-82.png)进入到uservec中，随后在uservec对栈进行了写入操作。不知为何发生了![store pgf](image-81.png),继续进入uservec，随后死循环

uservec 暂时先不写栈呢
![thread.4.15](image-79.png)

![thread.4.16](image-80.png)

![thread.4.17](image-83.png)

xv6的:
![thread.4.7](image-70.png)
![thread.4.8](image-71.png)
![thread.4.9](image-72.png)
![thread.4.10](image-73.png)
用户页表：
![thread.4.13](image-76.png)

### thread.5

do_exec无法成功返回用户态

![thread.5.1](image-84.png)
![thread.5.2](image-86.png)
![thread.5.3](image-87.png)
trapframe未映射
![thread.5.4](image-88.png)