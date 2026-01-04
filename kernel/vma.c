/**
 * @file vma.c
 * @brief Virtual Memory Area (VMA) 管理
 *
 * 实现 VMA 的分配、查找、插入、删除等操作
 */

#include "include/types.h"
#include "include/riscv.h"
#include "include/defs.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/vma.h"
#include "include/file.h"

extern struct proc *myproc(void);

/**
 * @brief 初始化 VMA 管理器
 */
void vma_init(struct vma_manager *vmam) {
    initlock(&vmam->lock, "vma");
    vmam->count = 0;
    for (int i = 0; i < MAX_VMA; i++) {
        vmam->vmas[i].used = 0;
        vmam->vmas[i].f = 0;
    }
}

/**
 * @brief 分配一个新的 VMA
 * @return 成功返回 VMA 指针，失败返回 NULL
 */
struct vma* vma_alloc(struct vma_manager *vmam) {
    struct vma *vma = 0;

    acquire(&vmam->lock);

    for (int i = 0; i < MAX_VMA; i++) {
        if (!vmam->vmas[i].used) {
            vma = &vmam->vmas[i];
            vma->used = 1;
            vmam->count++;
            break;
        }
    }

    release(&vmam->lock);
    return vma;
}

/**
 * @brief 释放一个 VMA
 * @note 调用者需确保已关闭关联的文件（如果有）
 */
void vma_free(struct vma_manager *vmam, struct vma *vma) {
    acquire(&vmam->lock);

    if (vma->used) {
        vma->used = 0;
        vma->f = 0;
        vmam->count--;
    }

    release(&vmam->lock);
}

/**
 * @brief 查找包含指定地址的 VMA
 * @param addr 要查找的虚拟地址
 * @return 成功返回 VMA 指针，失败返回 NULL
 */
struct vma* vma_lookup(struct vma_manager *vmam, uint64 addr) {
    struct vma *vma = 0;

    acquire(&vmam->lock);

    for (int i = 0; i < MAX_VMA; i++) {
        struct vma *v = &vmam->vmas[i];
        if (v->used && addr >= v->addr && addr < v->addr + v->length) {
            vma = v;
            break;
        }
    }

    release(&vmam->lock);
    return vma;
}

/**
 * @brief 插入一个新的 VMA
 * @param addr 起始虚拟地址
 * @param length 长度
 * @param offset 文件偏移量
 * @param prot 保护标志
 * @param flags 映射标志
 * @param f 关联的文件（匿名映射为 NULL）
 * @return 成功返回 0，失败返回 -1
 */
int vma_insert(struct vma_manager *vmam, uint64 addr, uint64 length,
               uint64 offset, int prot, int flags, struct file *f) {
    struct vma *vma;

    // 检查参数
    if (length == 0) {
        return -1;
    }

    // 页对齐
    addr = PGROUNDDOWN(addr);
    length = PGROUNDUP(length);

    // 检查是否与现有 VMA 冲突
    acquire(&vmam->lock);

    for (int i = 0; i < MAX_VMA; i++) {
        struct vma *v = &vmam->vmas[i];
        if (v->used) {
            // 检查地址范围是否重叠
            uint64 vma_end = v->addr + v->length;
            uint64 new_end = addr + length;
            if (!(addr >= vma_end || new_end <= v->addr)) {
                release(&vmam->lock);
                return -1;  // 地址重叠
            }
        }
    }

    // 查找空闲 VMA 槽位
    vma = 0;
    for (int i = 0; i < MAX_VMA; i++) {
        if (!vmam->vmas[i].used) {
            vma = &vmam->vmas[i];
            vma->used = 1;
            vmam->count++;
            break;
        }
    }

    release(&vmam->lock);

    if (!vma) {
        return -1;  // 没有空闲槽位
    }

    // 初始化 VMA
    vma->addr = addr;
    vma->length = length;
    vma->offset = offset;
    vma->prot = prot;
    vma->flags = flags;
    vma->f = f;

    return 0;
}

/**
 * @brief 移除指定地址范围的 VMA
 * @param addr 起始地址
 * @param length 长度
 * @return 成功返回移除的 VMA 数量，失败返回 -1
 */
int vma_remove(struct vma_manager *vmam, uint64 addr, uint64 length) {
    int removed = 0;

    acquire(&vmam->lock);

    for (int i = 0; i < MAX_VMA; i++) {
        struct vma *v = &vmam->vmas[i];
        if (v->used) {
            // 检查是否在指定范围内
            if (addr <= v->addr && v->addr + v->length <= addr + length) {
                // 完全包含，移除整个 VMA
                v->used = 0;
                v->f = 0;
                vmam->count--;
                removed++;
            }
        }
    }

    release(&vmam->lock);
    return removed;
}

/**
 * @brief 查找空闲的虚拟地址范围
 * @param hint_addr 提示地址（0 表示任意位置）
 * @param length 需要的长度
 * @param result 输出找到的地址
 * @return 成功返回 0，失败返回 -1
 */
int vma_find_free_range(struct vma_manager *vmam, uint64 hint_addr,
                        uint64 length, uint64 *result) {
    uint64 addr;
    int found;

    // 页对齐
    length = PGROUNDUP(length);

    acquire(&vmam->lock);

    if (hint_addr != 0) {
        // 检查提示地址是否可用
        addr = PGROUNDDOWN(hint_addr);
        found = 1;

        for (int i = 0; i < MAX_VMA && found; i++) {
            struct vma *v = &vmam->vmas[i];
            if (v->used) {
                uint64 vma_end = v->addr + v->length;
                uint64 new_end = addr + length;
                if (!(addr >= vma_end || new_end <= v->addr)) {
                    found = 0;  // 地址重叠
                }
            }
        }

        if (found && addr + length < MAXUVA) {
            *result = addr;
            release(&vmam->lock);
            return 0;
        }
    }

    // 在高地址区域查找空闲空间
    // 从 MAXUVA 向下查找
    addr = MAXUVA - length;
    found = 0;

    while (addr >= PGSIZE && !found) {
        found = 1;

        // 检查是否与现有 VMA 冲突
        for (int i = 0; i < MAX_VMA && found; i++) {
            struct vma *v = &vmam->vmas[i];
            if (v->used) {
                uint64 vma_end = v->addr + v->length;
                uint64 new_end = addr + length;
                if (!(addr >= vma_end || new_end <= v->addr)) {
                    found = 0;
                    addr = v->addr - length;
                    if (addr < PGSIZE) {
                        break;
                    }
                }
            }
        }

        if (found && addr >= PGSIZE) {
            *result = addr;
            release(&vmam->lock);
            return 0;
        }
    }

    release(&vmam->lock);
    return -1;
}

/**
 * @brief 复制 VMA 列表（用于 fork）
 */
void vma_copy(struct vma_manager *dst, struct vma_manager *src) {
    acquire(&src->lock);
    acquire(&dst->lock);

    dst->count = 0;

    for (int i = 0; i < MAX_VMA; i++) {
        if (src->vmas[i].used) {
            dst->vmas[i] = src->vmas[i];

            // 增加文件引用计数
            if (dst->vmas[i].f) {
                filedup(dst->vmas[i].f);
            }

            dst->count++;
        } else {
            dst->vmas[i].used = 0;
            dst->vmas[i].f = 0;
        }
    }

    release(&dst->lock);
    release(&src->lock);
}

/**
 * @brief 清理所有 VMA（用于 exit）
 */
void vma_cleanup(struct vma_manager *vmam) {
    acquire(&vmam->lock);

    for (int i = 0; i < MAX_VMA; i++) {
        if (vmam->vmas[i].used) {
            // 关闭关联的文件
            if (vmam->vmas[i].f) {
                fileclose(vmam->vmas[i].f);
                vmam->vmas[i].f = 0;
            }
            vmam->vmas[i].used = 0;
        }
    }

    vmam->count = 0;

    release(&vmam->lock);
}
