https://www.kernel.org/doc/html/v6.13/filesystems/vfs.html

# 分层

- 哪些函数应该由VFS提供统一接口，底层文件系统具体实现？
- 哪些函数应该交由VFS管理，底层无需实现？

---

## 职责总览

| 功能类别           | 由 VFS 实现（统一入口）             | 由底层文件系统实现（通过 VFS 调用）        |
|--------------------|-------------------------------------|--------------------------------------------|
| **路径解析**       | `namei()`、`walk_path()`、处理 `.`/`..`、跨 mount 逻辑 | `lookup()` —— 在某个目录下查找 name 对应 inode |
| **文件操作**       | `open()`、`read()`、`write()` 等系统调用分发逻辑 | `file_ops->read()`、`file_ops->write()` 等 |
| **目录操作**       | `mkdir()`、`unlink()`、`rmdir()` 分发处理 | `inode_ops->mkdir()`、`inode_ops->unlink()` |
| **挂载管理**       | `mount()`、`umount()`，挂载树维护   | 文件系统提供 `read_super()`、`mount()` 初始化函数 |
| **inode 分配/管理**| inode cache、inode number 映射、挂载点切换等 | 具体文件系统中 inode 内容初始化、加载持久化信息 |
| **文件描述符管理** | `fdtable` 管理、`fd->file` 映射等   | 底层文件系统只负责文件对象的 `read`/`write` |
| **元信息接口**     | `stat()` 系统调用分发               | 各 FS 实现 `inode_ops->getattr()` 提供具体 stat |
| **缓存逻辑（可选）**| dentry 缓存、page cache             | 底层 FS 通常不需要关心（但也可优化，如 ext4 的 readahead） |


---

1. **VFS 层负责：路径解析、权限检查、挂载调度、接口抽象、统一管理。**
2. **底层文件系统负责：具体文件/目录的操作（读写、创建、删除等），通过 VFS 的接口暴露能力。**

---


## 思路

之前思路有误，不应该重新使用vfs_file和vfs_inode替代原有xv6的file和inode抽象。事实上xv6的这两个抽象已经是vfs层级中的抽象了，只是没有支持多个文件系统。应该对其进行拓展而不是新建两个抽象。(底层不存在file这个抽象，而且底层的inode以dinode的形式存储)

1. 拓展file和inode抽象，修改文件操作路径，使得vfs统一分发
2. 测试加入vfs层后原本文件系统的正确性
3. qemu启动时挂载多个磁盘镜像（可选）
4. 加入一个virtio管理系统以及一个简单的block设备表,将物理磁盘分为多个设备（不同分区）。
5. 实现mount机制将不同文件系统挂载到不同的设备上
6. 支持ext4
7. 测试ext4正确性

There are three major structures to
handle filesystem-independent information: `inode`, `superblock` and `filesystem_type`.