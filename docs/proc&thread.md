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