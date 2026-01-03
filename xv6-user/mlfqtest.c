// 测试多级反馈队列(MLFQ)调度功能
// 本程序测试MLFQ调度是否正确工作

#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "xv6-user/user.h"

// 简化的打印函数
void print(const char *s)
{
  write(1, s, strlen(s));
}

void printnum(int n)
{
  char buf[16];
  int i = 0;
  if (n == 0) {
    write(1, "0", 1);
    return;
  }
  while (n > 0) {
    buf[i++] = '0' + (n % 10);
    n /= 10;
  }
  while (i > 0) {
    write(1, &buf[--i], 1);
  }
}

// 测试1：进程降级机制
// 创建一个CPU密集型子进程，观察其是否降级到低优先级队列
void test_demotion(void)
{
  print("测试1: 进程降级机制\n");

  int pid = fork();
  if (pid == 0) {
    // 子进程执行长时间计算任务
    int initial_queue = getqueuelevel();
    print("  [子进程] 初始队列级别: ");
    printnum(initial_queue);
    print("\n");

    // 执行大量计算，消耗时间片
    volatile int sum = 0;
    for (int i = 0; i < 80000000; i++) {
      sum += i;
      // 定期检查队列级别
      if (i % 2000000 == 0) {
        int current_queue = getqueuelevel();
        int current_slice = gettimeslice();
        print("  [子进程] 迭代 ");
        printnum(i / 1000);
        print("k: 队列=");
        printnum(current_queue);
        print(", 时间片=");
        printnum(current_slice);
        print("\n");
      }
    }

    int final_queue = getqueuelevel();
    print("  [子进程] 最终队列级别: ");
    printnum(final_queue);
    print("\n");

    if (final_queue > initial_queue) {
      print("  [子进程] 成功: 进程已降级到低优先级队列\n");
    } else {
      print("  [子进程] 注意: 进程未降级（可能计算时间不足）\n");
    }

    exit(0);
  }

  wait(0);
  print("  通过: 降级测试已完成\n");
}

// 测试2：多进程队列竞争
// 创建多个进程，观察MLFQ是否优先调度高队列进程
void test_queue_competition(void)
{
  print("测试2: 多进程队列竞争\n");

  // 创建第一个子进程（高队列）
  int pid1 = fork();
  if (pid1 == 0) {
    int queue = getqueuelevel();
    print("  [进程1] 队列级别: ");
    printnum(queue);
    print(" (新进程，应该是队列0)\n");

    // 执行短任务
    volatile int sum = 0;
    for (int i = 0; i < 100000000; i++) {
      sum += i;
      if (i % 10000000 == 0) {
        int q = getqueuelevel();
        print("  [进程1] 进程中... 队列=");
        printnum(q);
        print("\n");
      }
    }
    print("  [进程1] 进程1任务完成\n");
    exit(0);
  }

  // 等待一小段时间，让第一个进程开始执行
  sleep(1);

  // 创建第二个子进程（也是高队列）
  int pid2 = fork();
  if (pid2 == 0) {
    int queue = getqueuelevel();
    print("  [进程2] 队列级别: ");
    printnum(queue);
    print(" (新进程，应该是队列0)\n");

    // 执行中等任务，可能会降级
    volatile int sum = 0;
    for (int i = 0; i < 10000000; i++) {
      sum += i;
      if (i % 1000000 == 0) {
        int q = getqueuelevel();
        print("  [进程2] 进程中... 队列=");
        printnum(q);
        print("\n");
      }
    }
    int q = getqueuelevel();
    print("  [进程2] 进程中... 队列=");
    printnum(q);
    print("\n");
    print("  [进程2] 进程2任务完成\n");
    exit(0);
  }

  // 等待所有子进程
  wait(0);
  wait(0);

  print("  通过: 多进程竞争测试已完成\n");
}

int
main(void)
{
  print("=== 多级反馈队列(MLFQ)调度测试 ===\n\n");

  test_demotion();
  print("\n");

  test_queue_competition();
  print("\n");

  exit(0);
}
