
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

## tmux 

`tmux` 是一个强大的终端复用工具，可以在一个终端窗口中管理多个会话、窗口和面板，非常适合远程开发、调试、以及多任务操作。  

---

## **1. `tmux` 的基本概念**
在 `tmux` 中，有以下几个核心概念：
- **会话（Session）**：一个 `tmux` 进程可以包含多个会话，每个会话类似于一个独立的终端环境。
- **窗口（Window）**：每个 `tmux` 会话可以包含多个窗口，类似于 `screen` 里的 tab。
- **面板（Pane）**：每个 `tmux` 窗口可以拆分成多个面板（分屏），每个面板运行一个 Shell。

---

## **2. `tmux` 的基本命令**
### **(1) 启动 `tmux`**
```sh
tmux  # 启动 tmux，并创建一个新的会话
tmux new -s mysession  # 启动并命名会话
```

### **(2) 分离和重新连接 `tmux`**
```sh
tmux detach  # 退出 tmux 但不关闭会话（使用 Ctrl + B, D 组合键）
tmux attach -t mysession  # 重新连接到指定的会话
tmux attach  # 重新连接上一个会话
```

### **(3) 会话管理**
```sh
tmux ls  # 列出所有 tmux 会话
tmux new -s mysession  # 创建一个名为 "mysession" 的会话
tmux kill-session -t mysession  # 关闭指定的会话
tmux kill-server  # 关闭所有会话并退出 tmux
```

### **(4) 窗口管理**
```sh
Ctrl + B, C  # 创建新窗口
Ctrl + B, W  # 列出所有窗口
Ctrl + B, N  # 切换到下一个窗口
Ctrl + B, P  # 切换到上一个窗口
Ctrl + B, 0~9  # 直接跳转到指定窗口
Ctrl + B, &  # 关闭当前窗口
```

### **(5) 面板（Pane）管理**
```sh
Ctrl + B, %  # 垂直分割窗口（左右）
Ctrl + B, "  # 水平分割窗口（上下）
Ctrl + B, X  # 关闭当前面板
Ctrl + B, Z  # 最大化/恢复当前面板
Ctrl + B, O  # 交换面板
Ctrl + B, {  # 向左移动当前面板
Ctrl + B, }  # 向右移动当前面板
```

### **(6) 切换面板**
```sh
Ctrl + B, 方向键  # 移动到相邻的面板
Ctrl + B, Q  # 显示面板编号
Ctrl + B, 空格  # 切换不同的布局
```

---

## **3. `tmux` 的高级用法**
### **(1) 持久化会话**
有时候，你希望 `tmux` 在服务器断开后仍然保持运行，可以用：
```sh
tmux new -s mysession  # 启动会话
tmux detach  # 断开会话（保持后台运行）
tmux attach -t mysession  # 重新连接
```

### **(2) 共享 `tmux` 会话**
```sh
tmux new -s shared_session  # 创建一个共享会话
tmux attach -t shared_session  # 让另一位用户加入此会话
tmux list-clients  # 查看当前会话的连接用户
```

### **(3) 在 `tmux` 内部调整面板大小**
```sh
Ctrl + B, :  # 进入 tmux 命令模式
resize-pane -D 5  # 向下调整 5 行
resize-pane -U 5  # 向上调整 5 行
resize-pane -L 5  # 向左调整 5 列
resize-pane -R 5  # 向右调整 5 列
```

### **(4) 复制粘贴模式**
```sh
Ctrl + B, [  # 进入复制模式
方向键 ↑/↓/←/→  # 移动光标
空格键  # 开始选择文本
Enter  # 复制
Ctrl + B, ]  # 粘贴
```

### **(5) 保存 `tmux` 会话并自动恢复**
如果你想让 `tmux` 在下次启动时恢复之前的会话，可以使用 `tmux-resurrect`：
```sh
# 先安装插件 (使用 tpm - tmux plugin manager)
git clone https://github.com/tmux-plugins/tpm ~/.tmux/plugins/tpm

# 在 ~/.tmux.conf 中添加:
set -g @plugin 'tmux-plugins/tmux-resurrect'
run '~/.tmux/plugins/tpm/tpm'

# 启动 tmux 并按 Ctrl + B, I 安装插件
```
然后使用：
```sh
Ctrl + B, Ctrl + S  # 保存会话
Ctrl + B, Ctrl + R  # 恢复会话
```

---

## **4. `tmux` 的配置**
你可以在 `~/.tmux.conf` 文件中配置 `tmux`，比如：
```sh
# 设置默认窗口编号从 1 开始（默认是 0）
set -g base-index 1

# 重新绑定分割窗口快捷键
bind | split-window -h  # 用 | 代替 Ctrl + B, %
bind - split-window -v  # 用 - 代替 Ctrl + B, "

# 启用鼠标支持（可以用鼠标选择窗口、调整大小）
set -g mouse on
```
然后应用配置：
```sh
tmux source ~/.tmux.conf
```

---

## **5. `tmux` vs `screen`**
| 功能 | `tmux` | `screen` |
|------|--------|----------|
| **窗口管理** | 支持多个窗口和面板 | 仅支持多个窗口 |
| **分屏** | 支持 | 需要手动配置 |
| **鼠标支持** | 内置支持 | 需要手动启用 |
| **快捷键** | `Ctrl + B` | `Ctrl + A` |
| **状态栏** | 更强大 | 相对简单 |
| **配置** | `.tmux.conf` | `.screenrc` |

---

## **总结**
- `tmux` 适用于 **远程开发**、**后台任务管理**、**多窗口操作**，提高终端效率。
- `tmux` 的 **会话** 持久化功能，适用于长时间运行的任务，比如服务器维护。
- `tmux` **窗口/面板管理** 强大，适用于需要在多个终端间快速切换的场景。
- `tmux` 结合 `tmux-resurrect` 可以 **自动恢复会话**，避免手动重新创建环境。


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

