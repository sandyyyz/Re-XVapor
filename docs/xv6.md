# 系统启动
entry.s -> start()
## 

##  timer interrupt :

references:  https://blog.csdn.net/G129558/article/details/132571881

set sip = 2, pending a software interrupt.
![pending](image-28.png) 

timerinit() : 
1. set reg_mtvec = timervec
2. timervec set sip
3. enable machine-mode interrupt

the in main has call trapinithart():
```
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}
```

trap goes to kernelvec(kernel.s) -> kerneltrap() -> devintr() to recognize the software interrupt (and other types of interrupts)

## scheduler: 

references:  
1. https://blog.csdn.net/zzy980511/article/details/131519246?spm=1001.2014.3001.5502  
2. https://blog.csdn.net/zzy980511/article/details/137831750