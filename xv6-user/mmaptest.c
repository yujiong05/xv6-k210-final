// mmaptest.c - 测试 mmap 系统调用

#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "kernel/include/fcntl.h"
#include "user.h"

// 简单的内存映射测试
int main(int argc, char *argv[])
{
    printf("mmap test starting...\n");

    // 测试1：匿名映射（读/写）
    printf("\n=== Test 1: Anonymous R/W mapping ===\n");
    void *addr = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (addr == (void*)-1) {
        printf("FAIL: mmap failed\n");
        exit(1);
    }
    printf("mmap returned addr: %p\n", addr);

    // 尝试写入
    int *data = (int*)addr;
    *data = 0x12345678;
    printf("Wrote value: 0x%x\n", *data);

    if (*data == 0x12345678) {
        printf("PASS: Read back correct value\n");
    } else {
        printf("FAIL: Read back incorrect value\n");
    }

    // 测试 munmap
    if (munmap(addr, 4096) < 0) {
        printf("FAIL: munmap failed\n");
    } else {
        printf("PASS: munmap succeeded\n");
    }

    // 测试2：映射多个页面
    printf("\n=== Test 2: Multiple pages mapping ===\n");
    addr = mmap(0, 8192, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (addr == (void*)-1) {
        printf("FAIL: mmap failed\n");
        exit(1);
    }
    printf("mmap returned addr: %p (2 pages)\n", addr);

    // 写入第二个页面
    data = (int*)addr + 1024;  // 第二个页面
    *data = 0xABCDEF00;
    printf("Wrote to second page: 0x%x\n", *data);

    if (*data == 0xABCDEF00) {
        printf("PASS: Second page accessible\n");
    } else {
        printf("FAIL: Second page not accessible\n");
    }

    munmap(addr, 8192);

    // 测试3：只读映射
    printf("\n=== Test 3: Read-only mapping ===\n");
    addr = mmap(0, 4096, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (addr == (void*)-1) {
        printf("FAIL: mmap failed\n");
        exit(1);
    }
    printf("mmap returned addr: %p (read-only)\n", addr);

    // 尝试读取
    data = (int*)addr;
    printf("Read value: 0x%x (should be 0)\n", *data);

    munmap(addr, 4096);
    printf("PASS: Read-only mapping completed\n");

    // 测试4：测试 fork 后的 COW 行为
    printf("\n=== Test 4: Fork test (COW behavior) ===\n");
    addr = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (addr == (void*)-1) {
        printf("FAIL: mmap failed\n");
        exit(1);
    }

    data = (int*)addr;
    *data = 0x11111111;
    printf("Parent wrote: 0x%x\n", *data);

    int pid = fork();
    if (pid < 0) {
        printf("FAIL: fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        // 子进程
        printf("Child read value: 0x%x\n", *data);
        *data = 0x22222222;
        printf("Child wrote: 0x%x\n", *data);
        munmap(addr, 4096);
        exit(0);
    } else {
        // 父进程
        wait(0);
        printf("Parent read after child: 0x%x (should still be 0x11111111)\n", *data);
        if (*data == 0x11111111) {
            printf("PASS: COW works correctly\n");
        } else {
            printf("FAIL: COW not working\n");
        }
        munmap(addr, 4096);
    }

    printf("\n=== All tests completed ===\n");
    exit(0);
}
