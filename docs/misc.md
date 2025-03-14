
## 开发备忘录

1. 函数变更: 
```
    void sleep(void *chan,struct spinlock*) -> void thread_sleep(...);
    void wakeup(void *chan) -> void thread_wakeup_chan(...);
    void yield(void) -> void thread_yield(...);
    void sched(void) -> thread_sched(void) ;


```

2. wait等待一个子进程退出时，应等待子进程的所有线程退出

todo list:

- 条件变量
- 信号量系统
- 退出进程
- exec函数杀死其余线程
- exec()线程trapframe映射
- trap.c 检查thread_killed， last thread负责退出整个进程
- exit的语义？。。。
- proc_pagetable 现在不会分配并映射trapframe
- 如何管理线程的用户地址空间？。。
## 如何使用gdb? && 如何在vscode中使用gdb调试内核？

一些有用的链接：
1. https://zhuanlan.zhihu.com/p/354794701

2. https://www.cnblogs.com/KatyuMarisaBlog/p/13727565.html

成功：
https://www.zixiangcode.top/article/how-to-debug-xv6-in-vscode

记得注释掉代码目录下.gdbinit中的target remote port行，原因如下：
https://zhuanlan.zhihu.com/p/659251337

不必输入-exec:
https://github.com/Microsoft/vscode-cpptools/issues/106#issuecomment-678403249



## vscode常用快捷键
vs 和 vs code主要使用 gdb 调试，将常用调试快捷键记录如下：

- f5 启动并进入断点模式（想要 debug 就不要加 ctrl）
- ctrl+f5 开始执行，不进入断点
- shift+f5 停止调试
- ctrl+shift+f5 重启调试
- f10 逐过程执行（不需要加ctrl）
- f11 逐语句执行（不需要加ctrl）
- f9 切换断点
- ctrl+f9 启用/停止断点
- ctrl+shift+f9 删除全部断点
————————————————

                        
原文链接：https://blog.csdn.net/qq_43152052/article/details/122310825

## makefile 

|变量|作用|
|---|---|
|$@	|目标文件的名称（target）|
|$^	|所有依赖文件（prerequisites）|
|$<	|第一个依赖文件|
|$?	|所有比目标文件更新的依赖文件|
|$*	|目标文件的通配符部分（去掉扩展名）|


|关键字	|作用|
|---|---|
|wildcard|获取匹配的文件列表|
|foreach|遍历列表，应用表达式|
|patsubst|批量字符串替换|

$(wildcard pattern)
  
$(foreach var, list, expression)  

$(patsubst pattern, replacement, text)
