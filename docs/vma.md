## 使用基于VMA的内存管理

![vma](image-40.png)

1. 各进程共享的内存区域
- sigreturn 
- trampline

2. trapframe for each thread
3. vma  
- text
- heap
- anon or file (for mmap)
- interp (for dynamic link)

现在的问题就是如何管理各个线程的独有内存空间？

参照https://www.cnblogs.com/LoyenWang/p/12037658.html
