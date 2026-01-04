# xv6-k210 操作系统改进项目

这是一个基于 xv6 的 RISC-V 操作系统改进项目。原来的项目是支持 K210 开发板的（来自 [HUST-OS/xv6-k210](https://github.com/HUST-OS/xv6-k210)）。原项目文档统一移入了doc文档，新文档统一放在docs文件夹下

## 项目结构

```
├── kernel/        # 内核代码
├── xv6-user/      # 用户空间程序
├── docs/          # 文档文件夹
├── bootloader/    # 引导加载程序
├── Makefile       # 构建脚本
└── fs.img         # 文件系统镜像
```

## 当前进度

- [X] 写时复制（Copy-on-Write）机制
- [X] 优先级调度算法
- [X] 进程间通信-基于共享存储器
- [X] 多级反馈队列
- [X] 时间戳
- [X] 进程信息与统计功能
- [X] 信号机制
- [X] 文件名排重机制
- [X] 目录项缓存优化
- [X] FAT表缓存优化
- [X] 内存管理-惰性分配
- [X] 内存管理-mmap

## 改进计划

按模块分工，每个人负责一块：

### 进程部分（余炅负责）

- 进程调度
- 进程间通信

### 文件部分（李旺磊负责）

- 文件系统功能扩展
- 时间戳

### 内存部分（汪博负责）

- 内存管理优化

## 运行项目

项目在 QEMU 上运行，在Windows上使用wsl按照Ubuntu，下载riscv64-linux-gnu和qemu工具，运行下面命令：

```bash
make clean
make fs         # 生成文件系统镜像（首次或需要更新时）
make run       # 启动 QEMU
```

或直接运行脚本：

```bash
./run.bash
```

按 Ctrl+A 然后 X 退出 QEMU。
