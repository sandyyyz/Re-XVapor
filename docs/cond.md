用条件变量代替XV6原本的sleep - wakeup机制

linux :
https://zhuanlan.zhihu.com/p/374385534

由于实现的queue_t自带自旋锁，所以不再需要在条件变量结构体中添加一个额外的自旋锁

如何利用条件变量实现原本的逻辑？
-- 