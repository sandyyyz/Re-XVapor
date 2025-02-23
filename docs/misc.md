
## 开发备忘录

1. 函数变更: 
```
    void sleep(void *chan,struct spinlock*) -> void thread_sleep(...);
    void wakeup(void *chan) -> void thread_wakeup_chan(...);
    void yield(void) -> void thread_yield(...);
    void sched(void) -> thread_sched(void) ;


```

2. wait等待一个子进程退出时，应等待子进程的所有线程退出