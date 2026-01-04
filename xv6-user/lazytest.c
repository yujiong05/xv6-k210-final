#include "kernel/include/types.h"
#include "xv6-user/user.h"

#define PGSIZE 4096

static void
fail(const char *msg)
{
  printf("lazytest FAIL: %s\n", msg);
  exit(1);
}

static void
test_big_sparse_sbrk()
{
  printf("[1] big sparse sbrk...\n");

  int big = 64 * 1024 * 1024; // 64MB
  char *base = sbrk(big);
  if(base == (char*)-1)
    fail("sbrk(64MB) failed (should succeed with lazy alloc)");

  // 只触碰两页：第一页和最后一页（中间应保持未映射）
  base[0] = 'A';
  base[big - 1] = 'Z';
  if(base[0] != 'A' || base[big - 1] != 'Z')
    fail("touching sparse pages produced wrong values");

  printf("    OK\n");
}

static void
test_read_into_lazy_page()
{
  printf("[2] read() into never-touched page...\n");

  char *buf = sbrk(PGSIZE);
  if(buf == (char*)-1)
    fail("sbrk(PGSIZE) failed");

  // 不触碰 buf，直接让内核 read() 写入它
  int fds[2];
  if(pipe(fds) < 0)
    fail("pipe failed");

  const char *msg = "hello_lazy";
  if(write(fds[1], msg, 10) != 10)
    fail("pipe write failed");

  if(read(fds[0], buf, 10) != 10)
    fail("pipe read failed");

  if(memcmp(buf, msg, 10) != 0)
    fail("read content mismatch (copyout/lazy handling broken)");

  close(fds[0]);
  close(fds[1]);

  printf("    OK\n");
}

static void
test_fork_unmapped_semantics()
{
  printf("[3] fork with unmapped lazy page...\n");

  char *p = sbrk(PGSIZE);
  if(p == (char*)-1)
    fail("sbrk(PGSIZE) failed");

  // 不触碰 p：此时页应未映射，fork 后父子都未映射
  int pid = fork();
  if(pid < 0)
    fail("fork failed");

  if(pid == 0){
    // 子进程首次写入，应触发缺页分配并成功
    p[0] = 'C';
    if(p[0] != 'C')
      fail("child write after lazy fault failed");
    exit(0);
  }

  wait(0);

  // 父进程此时第一次读，会触发分配并应为 0（因为 demand-zero）
  if(p[0] != 0)
    fail("parent saw child's data; unmapped lazy page should not be shared");

  printf("    OK\n");
}

static void
test_negative_sbrk_unmap()
{
  printf("[4] negative sbrk unmap without panic...\n");

  char *p = sbrk(8 * PGSIZE);
  if(p == (char*)-1)
    fail("sbrk(8 pages) failed");

  // 只触碰一页，其他页保持 lazy 空洞
  p[0] = 1;

  // 收缩回去，要求 vmunmap 不因"未映射页" panic
  char *q = sbrk(-8 * PGSIZE);
  if(q == (char*)-1)
    fail("negative sbrk failed");

  printf("    OK\n");
}

int
main(void)
{
  printf("lazytest starting\n");

  test_big_sparse_sbrk();
  test_read_into_lazy_page();
  test_fork_unmapped_semantics();
  test_negative_sbrk_unmap();

  printf("lazytest PASS\n");
  exit(0);
}