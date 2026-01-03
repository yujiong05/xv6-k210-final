#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "kernel/include/fcntl.h"
#include "user.h"

// 全局变量标记信号处理函数是否被调用
volatile int sigusr1_received = 0;
volatile int sigusr2_received = 0;

// SIGUSR1 处理函数
void sigusr1_handler(int sig)
{
  printf("[信号处理] 收到 SIGUSR1\n");
  sigusr1_received = 1;
}

// SIGUSR2 处理函数
void sigusr2_handler(int sig)
{
  printf("[信号处理] 收到 SIGUSR2\n");
  sigusr2_received = 1;
}

// 测试1: 基本信号发送和处理
void test_basic_signal(void)
{
  printf("\n=== 测试1: 基本信号发送和处理 ===\n");

  int pid = fork();
  if (pid < 0) {
    printf("FAIL: fork 失败\n");
    return;
  }

  if (pid == 0) {
    // 子进程
    printf("[子进程] 注册 SIGUSR1 处理函数...\n");
    signal(SIGUSR1, sigusr1_handler);

    printf("[子进程] 等待信号...\n");
    sleep(20);  // 等待父进程发送信号

    if (sigusr1_received) {
      printf("[子进程] OK: SIGUSR1 信号已接收\n");
    } else {
      printf("[子进程] FAIL: 未收到 SIGUSR1\n");
    }

    exit(0);
  }

  // 父进程
  sleep(10);  // 让子进程先注册并等待
  printf("[父进程] 向子进程发送 SIGUSR1...\n");
  if (sigkill(pid, SIGUSR1) < 0) {
    printf("[父进程] FAIL: 发送信号失败\n");
  } else {
    printf("[父进程] OK: 信号已发送\n");
  }

  wait(0);
  printf("测试1 完成\n");
}

// 测试2: SIGKILL 信号
void test_sigkill(void)
{
  printf("\n=== 测试2: SIGKILL 信号 ===\n");

  int pid = fork();
  if (pid < 0) {
    printf("FAIL: fork 失败\n");
    return;
  }

  if (pid == 0) {
    // 子进程
    printf("[子进程] 运行中...\n");
    for (;;) {
      sleep(20);
    }
    exit(0);  // 不应该到达这里
  }

  // 父进程
  sleep(10);
  printf("[父进程] 发送 SIGKILL...\n");
  if (sigkill(pid, SIGKILL) < 0) {
    printf("[父进程] FAIL: 发送 SIGKILL 失败\n");
  } else {
    printf("[父进程] OK: SIGKILL 已发送\n");
  }

  int wstatus;
  wait(&wstatus);
  printf("子进程已终止\n");
  printf("测试2 完成\n");
}

// 测试3: SIGSTOP 和 SIGCONT
void test_sigstop_cont(void)
{
  printf("\n=== 测试3: SIGSTOP 和 SIGCONT ===\n");

  int pid = fork();
  if (pid < 0) {
    printf("FAIL: fork 失败\n");
    return;
  }

  if (pid == 0) {
    // 子进程
    printf("[子进程] 开始运行...\n");
    for (int i = 0; i < 5; i++) {
      printf("[子进程] 计数: %d\n", i);
      sleep(10);
    }
    printf("[子进程] 完成\n");
    exit(0);
  }

  // 父进程
  sleep(15);  // 让子进程运行一会儿
  printf("[父进程] 发送 SIGSTOP 暂停子进程...\n");
  sigkill(pid, SIGSTOP);

  sleep(20);  // 子进程应该被暂停

  printf("[父进程] 发送 SIGCONT 继续子进程...\n");
  sigkill(pid, SIGCONT);

  wait(0);
  printf("测试3 完成\n");
}

// 测试4: 多个信号排队
void test_multiple_signals(void)
{
  printf("\n=== 测试4: 多个信号排队 ===\n");

  int pid = fork();
  if (pid < 0) {
    printf("FAIL: fork 失败\n");
    return;
  }

  if (pid == 0) {
    // 子进程
    sigusr1_received = 0;
    sigusr2_received = 0;

    signal(SIGUSR1, sigusr1_handler);
    signal(SIGUSR2, sigusr2_handler);

    printf("[子进程] 等待多个信号...\n");
    sleep(50);

    printf("[子进程] SIGUSR1 收到: %s\n", sigusr1_received ? "是" : "否");
    printf("[子进程] SIGUSR2 收到: %s\n", sigusr2_received ? "是" : "否");

    if (sigusr1_received && sigusr2_received) {
      printf("[子进程] OK: 两个信号都收到\n");
    } else {
      printf("[子进程] FAIL: 部分信号未收到\n");
    }

    exit(0);
  }

  // 父进程
  sleep(10);
  printf("[父进程] 发送 SIGUSR1...\n");
  sigkill(pid, SIGUSR1);

  sleep(2);
  printf("[父进程] 发送 SIGUSR2...\n");
  sigkill(pid, SIGUSR2);

  wait(0);
  printf("测试4 完成\n");
}

// 测试5: SIG_IGN 忽略信号
void test_sig_ignore(void)
{
  printf("\n=== 测试5: SIG_IGN 忽略信号 ===\n");

  int pid = fork();
  if (pid < 0) {
    printf("FAIL: fork 失败\n");
    return;
  }

  if (pid == 0) {
    // 子进程
    printf("[子进程] 设置 SIGUSR1 为 SIG_IGN...\n");
    signal(SIGUSR1, SIG_IGN);

    printf("[子进程] 等待一段时间...\n");
    sleep(30);

    printf("[子进程] 完成（应该没有被信号中断）\n");
    exit(0);
  }

  // 父进程
  sleep(10);
  printf("[父进程] 发送 SIGUSR1（应该被忽略）...\n");
  sigkill(pid, SIGUSR1);

  wait(0);
  printf("测试5 完成\n");
}

int main(int argc, char *argv[])
{
  printf("=== 信号机制测试程序 2: 信号发送和处理 ===\n");

  test_basic_signal();
  test_sigkill();
  test_sigstop_cont();
  test_multiple_signals();
  test_sig_ignore();

  printf("\n=== 所有测试完成 ===\n");
  exit(0);
}
