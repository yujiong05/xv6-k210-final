/**
 * @file vma.h
 * @brief Virtual Memory Area (VMA) 管理定义
 *
 * VMA 用于管理进程的虚拟内存区域，支持 mmap 系统调用
 */

#ifndef _VMA_H
#define _VMA_H

#include "types.h"
#include "param.h"
#include "spinlock.h"

#define MAX_VMA 16  // 每个进程最多支持的 VMA 数量

/**
 * @brief mmap 保护标志
 */
#define PROT_READ   0x1  // 页可读
#define PROT_WRITE  0x2  // 页可写
#define PROT_EXEC   0x4  // 页可执行

/**
 * @brief mmap 映射标志
 */
#define MAP_SHARED  0x01  // 共享映射，写入会更新到文件
#define MAP_PRIVATE 0x02  // 私有映射（写时复制）
#define MAP_FIXED   0x04  // 强制使用 addr，不解释为提示
#define MAP_ANONYMOUS 0x08  // 匿名映射，不关联文件

/**
 * @brief Virtual Memory Area 结构体
 *
 * 描述进程地址空间中的一段连续虚拟内存区域
 */
struct vma {
    uint64 addr;      /* 起始虚拟地址（页对齐） */
    uint64 length;    /* 区域长度（字节） */
    uint64 offset;    /* 文件偏移量（文件映射使用） */
    int prot;         /* 保护标志（PROT_READ/WRITE/EXEC） */
    int flags;        /* 映射标志（MAP_SHARED/PRIVATE等） */
    struct file *f;   /* 关联的文件（NULL 表示匿名映射） */
    int used;         /* 标记是否使用 */
};

/**
 * @brief 进程的 VMA 管理器
 */
struct vma_manager {
    struct spinlock lock;     /* 保护 VMA 列表的锁 */
    struct vma vmas[MAX_VMA]; /* VMA 数组 */
    int count;                /* 当前 VMA 数量 */
};

/* VMA 管理函数 */
void vma_init(struct vma_manager *vmam);
struct vma* vma_alloc(struct vma_manager *vmam);
void vma_free(struct vma_manager *vmam, struct vma *vma);
struct vma* vma_lookup(struct vma_manager *vmam, uint64 addr);
int vma_insert(struct vma_manager *vmam, uint64 addr, uint64 length,
               uint64 offset, int prot, int flags, struct file *f);
int vma_remove(struct vma_manager *vmam, uint64 addr, uint64 length);
void vma_copy(struct vma_manager *dst, struct vma_manager *src);
void vma_cleanup(struct vma_manager *vmam);
int vma_find_free_range(struct vma_manager *vmam, uint64 hint_addr,
                        uint64 length, uint64 *result);

#endif /* _VMA_H */
