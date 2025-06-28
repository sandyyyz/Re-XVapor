
---

# Linux VFS 分层与职责说明（Re-XVapor 适配）

参考自 Linux 内核文档：https://www.kernel.org/doc/html/v6.13/filesystems/vfs.html

本项目当前正基于 xv6 引入虚拟文件系统（VFS）框架，目的是：

- 支持挂载多个文件系统（如 xv6 自带文件系统 + ext4）
- 实现统一的文件/目录抽象接口
- 解耦文件系统实现与用户空间操作（open/read/write/mkdir...）

---

## 分层目标与接口划分

### 核心问题

- **哪些功能应该由 VFS 统一提供接口？**
- **哪些功能由具体文件系统实现？**

我们可以通过以下表格来划分各自职责：

### 职责总览

| 功能类别           | 由 VFS 实现（统一入口）                                       | 由底层文件系统实现（通过 VFS 分发调用）              |
|--------------------|---------------------------------------------------------------|------------------------------------------------------|
| **路径解析**       | `namei()`、`walk_path()`、解析 `.`/`..`、跨挂载点跳转         | `lookup()`：在目录 inode 中查找特定子项             |
| **文件操作**       | `open()`/`read()`/`write()` 分发逻辑                          | `file_ops->read()` / `file_ops->write()`            |
| **目录操作**       | `mkdir()`/`unlink()`/`rmdir()` 等分发                         | `inode_ops->mkdir()` / `inode_ops->unlink()`        |
| **挂载管理**       | `mount()`/`umount()` 调度与挂载树维护                         | `fs_ops->mount()` / `read_super()` 初始化挂载点     |
| **inode 分配与生命周期** | inode cache 管理、设备编号、跨 FS 切换、VFS 层 i-node 抽象 | `iget()`、从磁盘加载 dinode 结构、释放资源          |
| **文件描述符管理** | `fdtable` 管理，`fd -> file` 映射逻辑                         | 底层不关心 fd 映射，只负责 `struct file` 语义       |
| **元信息访问**     | `stat()` 分发、路径转换                                        | `inode_ops->getattr()` 提供文件类型、大小、时间戳等 |
| **缓存机制**       | dentry 缓存、页缓存（page cache）、读写缓存统一接口          | 底层可选优化，如 ext4 readahead 或 journaling      |

---

### 职责划分总结

1. **VFS 主要职责：**
   - 路径解析与权限检查
   - 统一系统调用接口分发（open/read/write/stat 等）
   - mount/umount 管理（挂载树与设备映射）
   - inode cache / dentry cache 管理
   - file/inode 抽象与操作封装

2. **底层文件系统主要职责：**
   - 在 inode 层实现具体文件、目录、元信息访问
   - 处理实际的数据读写（块读、写入磁盘）
   - 提供 mount/read_super 格式识别与初始化挂载点
   - 提供自定义 inode_ops/file_ops 接口

---

## 当前设计思路调整

在初期的设计中，尝试用 `vfs_file` / `vfs_inode` 重新设计文件和 inode 抽象层，后经分析发现不符合 Linux 的 VFS 设计原则：

> ✔️ xv6 原有的 `struct file` 和 `struct inode` 已具备“文件系统无关抽象”的性质，可直接作为 VFS 层的抽象，只需扩展结构、加入接口函数指针、支持 mount 切换和 superblock 指针，即可承载多文件系统。

因此改进方案如下：

- 不新建 `vfs_inode`/`vfs_file`，而是直接在 `inode`/`file` 中添加：
  - 文件系统相关操作指针（`inode_ops`, `file_ops`）
  - `superblock` 指针用于查找 mount 根目录
  - `dentry` 指针用于路径缓存与命名空间管理

---

## 实际开发路径建议（Re-XVapor 适配）

1. **扩展 `inode` 和 `file` 结构，加入操作表与 superblock 指针**
2. **修改原有 `sys_open`, `sys_read`, `sys_write` 等路径，支持通过 VFS 分发**
3. **重构 `namei()` 逻辑，支持跨挂载点路径跳转，支持 `/mnt/ext4/foo.txt` 解析**
4. **测试现有 xv6 文件系统在接入 VFS 后的兼容性（不挂载 ext4）**
5. **支持 QEMU 启动挂载多个磁盘镜像，模拟多个设备**
6. **实现一个 `virtio_block[]` 设备表和简单设备 ID 分发机制**
7. **实现 `mount()` 接口，根据路径和设备号挂载 ext4 等文件系统**
8. **完成 ext4 适配，基于 `lwext4` 实现 `lookup`, `read`, `readdir` 等接口**
9. **测试 ext4 文件读写、stat、路径遍历等基本功能是否正常**

---

## 核心结构补充

VFS 框架围绕三大核心结构展开：

| 结构体         | 说明                                                             |
|----------------|------------------------------------------------------------------|
| `inode`        | 表示文件或目录对象，与底层 dinode 关联                          |
| `superblock`   | 表示文件系统实例，对应一个挂载点，管理该文件系统的全局信息     |
| `filesystem_type` | 文件系统类型，如 ext4/xv6fs/fat32，每种类型注册其挂载方法 |

目前已实现或正在实现 `inode_ops`, `superblock_ops`, `filesystem_type` 三种主要操作表。
---

## 后续可拓展方向

- 实现 dentry cache，提高路径访问效率
- 增加 write-back 机制与页缓存（page cache）层
- 支持更复杂的 mount 类型，如 bind-mount、overlay 等
- 多用户支持、权限管理与 namespace 支持
- 编写 `mount`, `umount`, `df`, `lsblk` 等用户态工具

---
