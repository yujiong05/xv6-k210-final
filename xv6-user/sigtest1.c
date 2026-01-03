#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "kernel/include/fcntl.h"
#include "user.h"

// 信号处理函数
void sig_handler(int sig)
{
  printf("[信号处理] 收到信号: %d\n", sig);
}

// 测试信号1的处理函数
void test1_handler(int sig)
{
  printf("[TEST1] 收到 SIGUSR1 信号!\n");
}

// 测试信号2的处理函数
void test2_handler(int sig)
{
  printf("[TEST2] 收到 SIGUSR2 信号!\n");
}

int main(int argc, char *argv[])
{
  void (*old_handler)(int);
  int pid;

  printf("=== 信号机制测试程序 1: 信号处理函数注册 ===\n\n");

  // 测试1: 注册 SIGUSR1 信号处理函数
  printf("[测试1] 注册 SIGUSR1 信号处理函数...\n");
  old_handler = signal(SIGUSR1, test1_handler);
  if (old_handler == SIG_ERR) {
    printf("注册 SIGUSR1 失败!\n");
    exit(1);
  }
  printf("注册成功, 旧处理函数: SIG_DFL (0x%p)\n", old_handler);

  // 测试2: 再次注册，检查返回值
  printf("\n[测试2] 再次注册 SIGUSR1，检查返回值...\n");
  old_handler = signal(SIGUSR1, sig_handler);
  if (old_handler == SIG_ERR) {
    printf("注册失败!\n");
    exit(1);
  }
  printf("注册成功, 返回的旧处理函数地址: 0x%p\n", old_handler);

  // 测试3: 注册 SIGUSR2 信号处理函数
  printf("\n[测试3] 注册 SIGUSR2 信号处理函数...\n");
  old_handler = signal(SIGUSR2, test2_handler);
  if (old_handler == SIG_ERR) {
    printf("注册 SIGUSR2 失败!\n");
    exit(1);
  }
  printf("注册成功, 旧处理函数: SIG_DFL (0x%p)\n", old_handler);

  // 测试4: 设置 SIG_IGN (忽略信号)
  printf("\n[测试4] 设置 SIGINT 为 SIG_IGN...\n");
  old_handler = signal(SIGINT, SIG_IGN);
  if (old_handler == SIG_ERR) {
    printf("设置 SIGINT 失败!\n");
    exit(1);
  }
  printf("设置成功, SIGINT 将被忽略\n");

  // 测试5: 尝试注册 SIGKILL (应该失败)
  printf("\n[测试5] 尝试注册 SIGKILL 处理函数 (应该失败)...\n");
  old_handler = signal(SIGKILL, sig_handler);
  if (old_handler == SIG_ERR) {
    printf("正确: SIGKILL 不能被捕获或忽略!\n");
  } else {
    printf("错误: SIGKILL 不应该允许注册!\n");
  }

  // 测试6: 尝试注册 SIGSTOP (应该失败)
  printf("\n[测试6] 尝试注册 SIGSTOP 处理函数 (应该失败)...\n");
  old_handler = signal(SIGSTOP, sig_handler);
  if (old_handler == SIG_ERR) {
    printf("正确: SIGSTOP 不能被捕获或忽略!\n");
  } else {
    printf("错误: SIGSTOP 不应该允许注册!\n");
  }

  // 测试7: 尝试注册无效信号号
  printf("\n[测试7] 尝试注册无效信号号 (应该失败)...\n");
  old_handler = signal(999, sig_handler);
  if (old_handler == SIG_ERR) {
    printf("正确: 无效信号号被拒绝!\n");
  } else {
    printf("错误: 无效信号号不应该被接受!\n");
  }

  // 测试8: 注册 SIGTERM
  printf("\n[测试8] 注册 SIGTERM 信号处理函数...\n");
  old_handler = signal(SIGTERM, sig_handler);
  if (old_handler == SIG_ERR) {
    printf("注册 SIGTERM 失败!\n");
    exit(1);
  }
  printf("注册成功, 旧处理函数: SIG_DFL (0x%p)\n", old_handler);

  // 测试9: 恢复默认处理
  printf("\n[测试9] 恢复 SIGUSR1 为默认处理...\n");
  old_handler = signal(SIGUSR1, SIG_DFL);
  if (old_handler == SIG_ERR) {
    printf("恢复失败!\n");
    exit(1);
  }
  printf("恢复成功, SIGUSR1 现在使用默认处理\n");

  // 测试10: 创建子进程，测试信号处理函数的继承
  printf("\n[测试10] 测试 fork 后信号处理函数的继承...\n");
  pid = fork();
  if (pid < 0) {
    printf("fork 失败!\n");
    exit(1);
  }

  if (pid == 0) {
    // 子进程
    printf("  [子进程 PID=%d] 继承了父进程的信号处理函数设置\n", getpid());
    printf("  [子进程] SIGINT 仍然是 SIG_IGN\n");
    printf("  [子进程] SIGUSR2 处理函数已设置\n");
    printf("  [子进程] 退出\n");
    exit(0);
  }

  // 父进程等待子进程
  wait(0);
  printf("子进程已退出\n");

  exit(0);
}
