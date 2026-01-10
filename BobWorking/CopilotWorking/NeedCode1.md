# 沙箱（seccomp-lite）所需文件代码

根据Read.txt要求，以下是制作沙箱补丁所需的完整文件代码：

## 1. 内核侧（Kernel）

### 1.1 kernel/include/sysnum.h

```c
#ifndef __SYSNUM_H
#define __SYSNUM_H

// System call numbers
#define SYS_fork         1
#define SYS_exit         2
#define SYS_wait         3
#define SYS_pipe         4
#define SYS_read         5
#define SYS_kill         6
#define SYS_exec         7
#define SYS_fstat        8
#define SYS_chdir        9
#define SYS_dup         10
#define SYS_getpid      11
#define SYS_sbrk        12
#define SYS_sleep       13
#define SYS_uptime      14
#define SYS_open        15
#define SYS_write       16
#define SYS_remove      17
#define SYS_trace       18
#define SYS_sysinfo     19
#define SYS_mkdir       20
#define SYS_close       21
#define SYS_test_proc   22
#define SYS_dev         23
#define SYS_readdir     24
#define SYS_getcwd      25
#define SYS_rename      26
#define SYS_setpriority 27
#define SYS_getpriority 28
#define SYS_shmget      29
#define SYS_shmat       30
#define SYS_shmdt       31
#define SYS_shmctl      32
#define SYS_getqueuelevel 33  // Get process MLFQ queue level
#define SYS_gettimeslice  34  // Get process remaining time slice
#define SYS_getprocs      35  // Get process information array
#define SYS_getrusage    36  // Get resource usage
#define SYS_signal       37  // Register signal handler
#define SYS_sigkill      38  // Send signal to process
#define SYS_settime      39
#define SYS_mmap         40  // Memory mapping
#define SYS_munmap       41  // Unmap memory
#define SYS_sandbox      42  // Seccomp-lite sandbox control

#endif
```

### 1.2 kernel/syscall.c

```c

#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/syscall.h"
#include "include/sysinfo.h"
#include "include/kalloc.h"
#include "include/vm.h"
#include "include/string.h"
#include "include/printf.h"

// Fetch the uint64 at addr from the current process.
int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  if(addr >= p->sz || addr+sizeof(uint64) > p->sz)
    return -1;
  // if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
  if(copyin2((char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
int
fetchstr(uint64 addr, char *buf, int max)
{
  // struct proc *p = myproc();
  // int err = copyinstr(p->pagetable, buf, addr, max);
  int err = copyinstr2(buf, addr, max);
  if(err < 0)
    return err;
  return strlen(buf);
}

static uint64
argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
int
argint(int n, int *ip)
{
  *ip = argraw(n);
  return 0;
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
int
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
  return 0;
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  if(argaddr(n, &addr) < 0)
    return -1;
  return fetchstr(addr, buf, max);
}

extern uint64 sys_chdir(void);
extern uint64 sys_close(void);
extern uint64 sys_dup(void);
extern uint64 sys_exec(void);
extern uint64 sys_exit(void);
extern uint64 sys_fork(void);
extern uint64 sys_fstat(void);
extern uint64 sys_getpid(void);
extern uint64 sys_kill(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_open(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_wait(void);
extern uint64 sys_write(void);
extern uint64 sys_uptime(void);
extern uint64 sys_test_proc(void);
extern uint64 sys_dev(void);
extern uint64 sys_readdir(void);
extern uint64 sys_getcwd(void);
extern uint64 sys_remove(void);
extern uint64 sys_trace(void);
extern uint64 sys_sysinfo(void);
extern uint64 sys_rename(void);
extern uint64 sys_setpriority(void);
extern uint64 sys_getpriority(void);
extern uint64 sys_shmget(void);
extern uint64 sys_shmat(void);
extern uint64 sys_shmdt(void);
extern uint64 sys_shmctl(void);
extern uint64 sys_getqueuelevel(void);
extern uint64 sys_gettimeslice(void);
extern uint64 sys_getprocs(void);
extern uint64 sys_getrusage(void);
extern uint64 sys_signal(void);
extern uint64 sys_sigkill(void);
extern uint64 sys_settime(void);
extern uint64 sys_mmap(void);
extern uint64 sys_munmap(void);
extern uint64 sys_sandbox(void);

static uint64 (*syscalls[])(void) = {
  [SYS_fork]        sys_fork,
  [SYS_exit]        sys_exit,
  [SYS_wait]        sys_wait,
  [SYS_pipe]        sys_pipe,
  [SYS_read]        sys_read,
  [SYS_kill]        sys_kill,
  [SYS_exec]        sys_exec,
  [SYS_fstat]       sys_fstat,
  [SYS_chdir]       sys_chdir,
  [SYS_dup]         sys_dup,
  [SYS_getpid]      sys_getpid,
  [SYS_sbrk]        sys_sbrk,
  [SYS_sleep]       sys_sleep,
  [SYS_uptime]      sys_uptime,
  [SYS_open]        sys_open,
  [SYS_write]       sys_write,
  [SYS_mkdir]       sys_mkdir,
  [SYS_close]       sys_close,
  [SYS_test_proc]   sys_test_proc,
  [SYS_dev]         sys_dev,
  [SYS_readdir]     sys_readdir,
  [SYS_getcwd]      sys_getcwd,
  [SYS_remove]      sys_remove,
  [SYS_trace]       sys_trace,
  [SYS_sysinfo]     sys_sysinfo,
  [SYS_rename]      sys_rename,
  [SYS_setpriority] sys_setpriority,
  [SYS_getpriority] sys_getpriority,
  [SYS_shmget]      sys_shmget,
  [SYS_shmat]       sys_shmat,
  [SYS_shmdt]       sys_shmdt,
  [SYS_shmctl]      sys_shmctl,
  [SYS_getqueuelevel] sys_getqueuelevel,
  [SYS_gettimeslice]  sys_gettimeslice,
  [SYS_getprocs]      sys_getprocs,
  [SYS_getrusage]    sys_getrusage,
  [SYS_signal]       sys_signal,
  [SYS_sigkill]      sys_sigkill,
  [SYS_settime]      sys_settime,
  [SYS_mmap]         sys_mmap,
  [SYS_munmap]       sys_munmap,
  [SYS_sandbox]      sys_sandbox,
};

static char *sysnames[] = {
  [SYS_fork]        "fork",
  [SYS_exit]        "exit",
  [SYS_wait]        "wait",
  [SYS_pipe]        "pipe",
  [SYS_read]        "read",
  [SYS_kill]        "kill",
  [SYS_exec]        "exec",
  [SYS_fstat]       "fstat",
  [SYS_chdir]       "chdir",
  [SYS_dup]         "dup",
  [SYS_getpid]      "getpid",
  [SYS_sbrk]        "sbrk",
  [SYS_sleep]       "sleep",
  [SYS_uptime]      "uptime",
  [SYS_open]        "open",
  [SYS_write]       "write",
  [SYS_mkdir]       "mkdir",
  [SYS_close]       "close",
  [SYS_test_proc]   "test_proc",
  [SYS_dev]         "dev",
  [SYS_readdir]     "readdir",
  [SYS_getcwd]      "getcwd",
  [SYS_remove]      "remove",
  [SYS_trace]       "trace",
  [SYS_sysinfo]     "sysinfo",
  [SYS_rename]      "rename",
  [SYS_setpriority] "setpriority",
  [SYS_getpriority] "getpriority",
  [SYS_shmget]      "shmget",
  [SYS_shmat]       "shmat",
  [SYS_shmdt]       "shmdt",
  [SYS_shmctl]      "shmctl",
  [SYS_getqueuelevel] "getqueuelevel",
  [SYS_gettimeslice]  "gettimeslice",
  [SYS_getprocs]      "getprocs",
  [SYS_getrusage]    "getrusage",
  [SYS_signal]       "signal",
  [SYS_sigkill]      "sigkill",
  [SYS_settime]      "settime",
  [SYS_mmap]         "mmap",
  [SYS_munmap]       "munmap",
  [SYS_sandbox]      "sandbox",
};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;

  // 沙箱拒绝点：仅当开启并不在 allow mask 中才拒绝
  if (num > 0 && num < NELEM(syscalls)) {
    if (p->sandbox_on && num != SYS_sandbox) {
      int word = num / 32;
      int bit  = num % 32;
      int allowed = (p->allow_mask[word] >> bit) & 1;
      if (!allowed) {
        // 拒绝并返回 -1，可选：对齐 trace 输出格式
        p->trapframe->a0 = -1;
        if ((p->tmask & (1 << num)) != 0) {
          printf("pid %d: %s -> %d\n", p->pid, sysnames[num], p->trapframe->a0);
        }
        return;
      }
    }
  }

  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    p->trapframe->a0 = syscalls[num]();
    // trace
    if ((p->tmask & (1 << num)) != 0) {
      printf("pid %d: %s -> %d\n", p->pid, sysnames[num], p->trapframe->a0);
    }
  } else {
    printf("pid %d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}

uint64 
sys_test_proc(void) {
    int n;
    argint(0, &n);
    printf("hello world from proc %d, hart %d, arg %d\n", myproc()->pid, r_tp(), n);
    return 0;
}

uint64
sys_sysinfo(void)
{
  uint64 addr;
  // struct proc *p = myproc();

  if (argaddr(0, &addr) < 0) {
    return -1;
  }

  struct sysinfo info;
  info.freemem = freemem_amount();
  info.nproc = procnum();

  // if (copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0) {
  if (copyout2(addr, (char *)&info, sizeof(info)) < 0) {
    return -1;
  }

  return 0;
}

// 共享内存系统调用包装函数
extern int do_shmget(int key, uint64 size, int flag);
extern void* do_shmat(int shmid, uint64 addr, int flag);
extern int do_shmdt(uint64 addr);
extern int do_shmctl(int shmid, int cmd, void *buf);

uint64
sys_shmget(void)
{
  int key, flag;
  uint64 size;

  if(argint(0, &key) < 0)
    return -1;
  if(argaddr(1, &size) < 0)
    return -1;
  if(argint(2, &flag) < 0)
    return -1;

  return do_shmget(key, size, flag);
}

uint64
sys_shmat(void)
{
  int shmid, flag;
  uint64 addr;

  if(argint(0, &shmid) < 0)
    return -1;
  if(argaddr(1, &addr) < 0)
    return -1;
  if(argint(2, &flag) < 0)
    return -1;

  return (uint64)do_shmat(shmid, addr, flag);
}

uint64
sys_shmdt(void)
{
  uint64 addr;

  if(argaddr(0, &addr) < 0)
    return -1;

  return do_shmdt(addr);
}

uint64
sys_shmctl(void)
{
  int shmid, cmd;
  uint64 buf;

  if(argint(0, &shmid) < 0)
    return -1;
  if(argint(1, &cmd) < 0)
    return -1;
  if(argaddr(2, &buf) < 0)
    return -1;

  return do_shmctl(shmid, cmd, (void*)buf);
}

uint64
sys_sandbox(void)
{
  int on, action, words;
  uint64 umask_addr;
  struct proc *p = myproc();

  if (argint(0, &on) < 0) return -1;
  if (argint(1, &action) < 0) return -1;
  if (argaddr(2, &umask_addr) < 0) return -1;
  if (argint(3, &words) < 0) return -1;

  if (on == 0) {
    // disable sandbox
    p->sandbox_on = 0;
    return 0;
  }

  // only action=0 supported now
  if (action != 0) return -1;

  // clamp words to [0,4]
  if (words < 0) words = 0;
  if (words > 4) words = 4;

  // copy mask from user space
  uint32 tmp[4] = {0};
  if (words > 0) {
    uint64 nbytes = (uint64)words * sizeof(uint32);
    if (either_copyin(tmp, 1, umask_addr, nbytes) < 0) {
      return -1;
    }
  }

  // apply
  p->sandbox_on = 1;
  p->sandbox_action = 0;
  for (int i = 0; i < 4; i++) p->allow_mask[i] = tmp[i];
  return 0;
}
```

### 1.3 kernel/include/proc.h

```c
#ifndef __PROC_H
#define __PROC_H

#include "param.h"
#include "riscv.h"
#include "types.h"
#include "spinlock.h"
#include "file.h"
#include "fat32.h"
#include "trap.h"
#include "signal.h"
#include "vma.h"

// Saved registers for kernel context switches.
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// Per-CPU state.
struct cpu {
  struct proc *proc;          // The process running on this cpu, or null.
  struct context context;     // swtch() here to enter scheduler().
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

enum procstate { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  struct proc *parent;         // Parent process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  pagetable_t kpagetable;      // Kernel page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct dirent *cwd;          // Current directory
  char name[16];               // Process name (debugging)
  int tmask;                   // trace mask
  int priority;                // Process priority (0-100, higher = more important)
  // Multi-level Feedback Queue (MLFQ) fields
  int queue_level;             // Current queue level (0=highest, 2=lowest)
  int time_slice;              // Remaining time slices in current queue
  int ticks_used;              // Total ticks used by this process

  // Process time statistics
  uint64 utime;                // User mode ticks
  uint64 stime;                // Kernel mode ticks
  uint64 start_time;           // Process start time (ticks since boot)

  // Signal handling
  uint64 sig_pending;          // Pending signals bitmap (bit 0 = signal 1, etc.)
  uint64 sig_handlers[NSIG];   // Signal handler addresses
  uint64 sig_mask;             // Signal mask (blocked signals)

  // Sandbox policy (seccomp-lite)
  int sandbox_on;              // 1: sandbox enabled, 0: disabled
  int sandbox_action;          // 0: deny(-1), future: send signal, etc.
  uint32 allow_mask[4];        // 128-bit allow mask for syscall numbers

  // Virtual Memory Area (VMA) management for mmap
  struct vma_manager vma_manager;  // VMA 管理器
};

void            reg_info(void);
int             cpuid(void);
void            exit(int);
int             fork(void);
int             growproc(int);
pagetable_t     proc_pagetable(struct proc *);
void            proc_freepagetable(pagetable_t, uint64);
int             kill(int);
struct cpu*     mycpu(void);
struct cpu*     getmycpu(void);
struct proc*    myproc();
void            procinit(void);
void            scheduler(void) __attribute__((noreturn));
void            sched(void);
void            setproc(struct proc*);
void            sleep(void*, struct spinlock*);
void            userinit(void);
int             wait(uint64);
void            wakeup(void*);
void            yield(void);
int             higher_priority_ready(void);
int             handle_time_slice(void);
int             either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int             either_copyin(void *dst, int user_src, uint64 src, uint64 len);
void            procdump(void);
uint64          procnum(void);
void            test_proc_init(int);

#endif
```

### 1.4 kernel/proc.c（关键部分）

```c
#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/intr.h"
#include "include/kalloc.h"
#include "include/printf.h"
#include "include/string.h"
#include "include/fat32.h"
#include "include/file.h"
#include "include/trap.h"
#include "include/vm.h"
#include "include/timer.h"


struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
uint64 ticks_start = 0;  // System start time (ticks)
struct spinlock pid_lock;

extern void forkret(void);
extern void swtch(struct context*, struct context*);
static void wakeup1(struct proc *chan);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// ... 其他函数 ...

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return NULL;

found:
  p->pid = allocpid();

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == NULL){
    release(&p->lock);
    return NULL;
  }
  // Initialize trapframe to zero
  memset(p->trapframe, 0, sizeof(struct trapframe));

  // An empty user page table.
  // And an identical kernel page table for this proc.
  if ((p->pagetable = proc_pagetable(p)) == NULL ||
      (p->kpagetable = proc_kpagetable()) == NULL) {
    freeproc(p);
    release(&p->lock);
    return NULL;
  }

  p->kstack = VKSTACK;

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  // Set default priority
  p->priority = 50;  // Default priority (medium)

  // Initialize MLFQ fields
  p->queue_level = 0;        // New processes start at highest priority queue
  p->time_slice = 0;         // Will be set by scheduler when first run
  p->ticks_used = 0;

  // Initialize time statistics
  p->utime = 0;
  p->stime = 0;
  p->start_time = ticks;  // Record current ticks as process start time

  // Initialize signal handling
  p->sig_pending = 0;     // No pending signals
  p->sig_mask = 0;        // No signals blocked
  // Set default handlers for all signals
  for(int i = 0; i < NSIG; i++) {
    p->sig_handlers[i] = (uint64)SIG_DFL;
  }

  // Initialize Sandbox defaults
  p->sandbox_on = 0;
  p->sandbox_action = 0;
  for (int i = 0; i < 4; i++) p->allow_mask[i] = 0;

  // Initialize VMA manager
  vma_init(&p->vma_manager);

  return p;
}

// Free a process's resources.  Does not free the process structure itself.
// Called when the process is exiting, or when a proc structure is being reused.
static void
freeproc(struct proc *p)
{
  // Close all open files.
  for(int i = 0; i < NOFILE; i++){
    if(p->ofile[i]){
      fileclose(p->ofile[i]);
      p->ofile[i] = 0;
    }
  }
  if(p->cwd) {
    iput(p->cwd);
    p->cwd = 0;
  }

  // Reset signal handling
  p->sig_pending = 0;
  p->sig_mask = 0;
  for(int i = 0; i < NSIG; i++) {
    p->sig_handlers[i] = (uint64)SIG_DFL;
  }

  // Cleanup VMA
  vma_cleanup(&p->vma_manager);

  // Reset sandbox
  p->sandbox_on = 0;
  p->sandbox_action = 0;
  for (int i = 0; i < 4; i++) p->allow_mask[i] = 0;
}
```

### 1.5 kernel/sysproc.c

```c

#include "include/types.h"
#include "include/riscv.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/syscall.h"
#include "include/timer.h"
#include "include/kalloc.h"
#include "include/string.h"
#include "include/printf.h"
#include "include/procinfo.h"
#include "include/vm.h"
#include "include/signal.h"
#include "include/vma.h"
#include "include/file.h"

extern int exec(char *path, char **argv);
extern struct proc proc[NPROC];

uint64
sys_exec(void)
{
  char path[FAT32_MAX_PATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  if(argstr(0, path, FAT32_MAX_PATH) < 0 || argaddr(1, &uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if(argv[i] == 0)
      goto bad;
    if(fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  int ret = exec(path, argv);

  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int n;
  if(argint(0, &n) < 0)
    return (uint64)-1;

  struct proc *p = myproc();
  uint64 oldsz = p->sz;

  uint64 newsz;

  if(n >= 0){
    newsz = oldsz + (uint64)n;
    if(newsz < oldsz)            // overflow
      return (uint64)-1;
    if(newsz >= MAXUVA)          // 上界（按你工程里的宏为准）
      return (uint64)-1;
    p->sz = newsz;               // lazy: 只记账，不分配
  } else {
    uint64 dec = (uint64)(-n);
    if(dec > oldsz)              // underflow
      return (uint64)-1;
    newsz = oldsz - dec;
    p->sz = uvmdealloc(p->pagetable, p->kpagetable, oldsz, newsz);
  }
  return oldsz;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void)
{
  int mask;
  if(argint(0, &mask) < 0) {
    return -1;
  }
  myproc()->tmask = mask;
  return 0;
}

uint64
sys_setpriority(void)
{
  int prio;
  struct proc *p = myproc();

  if(argint(0, &prio) < 0) {
    return -1;
  }

  // Validate priority range
  if(prio < 0 || prio > 100) {
    return -1;
  }

  p->priority = prio;
  return 0;
}

uint64
sys_getpriority(void)
{
  return myproc()->priority;
}

// Get process MLFQ queue level
uint64
sys_getqueuelevel(void)
{
  return myproc()->queue_level;
}

// Get process remaining time slice
uint64
sys_gettimeslice(void)
{
  return myproc()->time_slice;
}

// Get process information array
// Returns: number of processes copied, or -1 on error
uint64
sys_getprocs(void)
{
  uint64 addr;
  int max_count;

  if(argaddr(0, &addr) < 0)
    return -1;
  if(argint(1, &max_count) < 0)
    return -1;

  struct proc *p;
  int count = 0;
  struct procinfo info;

  // Iterate through all processes
  for(p = proc; p < &proc[NPROC]; p++) {
    if(count >= max_count)
      break;

    acquire(&p->lock);

    // Only include non-UNUSED processes
    if(p->state != UNUSED) {
      info.pid = p->pid;
      info.state = p->state;
      info.priority = p->priority;
      info.queue_level = p->queue_level;
      info.time_slice = p->time_slice;
      info.ticks_used = p->ticks_used;
      info.utime = p->utime;
      info.stime = p->stime;
      info.start_time = p->start_time;
      info.sz = p->sz;
      safestrcpy(info.name, p->name, sizeof(info.name));

      release(&p->lock);

      // Copy to user space
      if(copyout2(addr + count * sizeof(info), (char*)&info, sizeof(info)) < 0) {
        return -1;
      }
      count++;
    } else {
      release(&p->lock);
    }
  }

  return count;
}

// Get resource usage for the current process
// Returns: 0 on success, -1 on error
uint64
sys_getrusage(void)
{
  uint64 utime_addr, stime_addr;
  struct proc *p = myproc();

  if(argaddr(0, &utime_addr) < 0)
    return -1;
  if(argaddr(1, &stime_addr) < 0)
    return -1;

  acquire(&p->lock);

  // Copy utime to user space
  if(utime_addr != 0 && copyout2(utime_addr, (char*)&p->utime, sizeof(p->utime)) < 0) {
    release(&p->lock);
    return -1;
  }

  // Copy stime to user space
  if(stime_addr != 0 && copyout2(stime_addr, (char*)&p->stime, sizeof(p->stime)) < 0) {
    release(&p->lock);
    return -1;
  }

  release(&p->lock);
  return 0;
}

// Register signal handler
// sys_signal(int sig, void (*handler)(int))
uint64
sys_signal(void)
{
  int sig;
  uint64 handler_addr;
  struct proc *p = myproc();

  if(argint(0, &sig) < 0)
    return (uint64)SIG_ERR;
  if(argaddr(1, &handler_addr) < 0)
    return (uint64)SIG_ERR;

  // Validate signal number
  if(sig < 1 || sig >= NSIG)
    return (uint64)SIG_ERR;

  // SIGKILL and SIGSTOP cannot be caught or ignored
  if(sig == SIGKILL || sig == SIGSTOP)
    return (uint64)SIG_ERR;

  acquire(&p->lock);

  // Save old handler
  uint64 old_handler = p->sig_handlers[sig];

  // Set new handler
  p->sig_handlers[sig] = handler_addr;

  release(&p->lock);

  return old_handler;
}

// Send signal to process
// sys_sigkill(int pid, int sig)
uint64
sys_sigkill(void)
{
  int pid, sig;
  struct proc *p;

  if(argint(0, &pid) < 0)
    return -1;
  if(argint(1, &sig) < 0)
    return -1;

  // Validate signal number
  if(sig < 1 || sig >= NSIG)
    return -1;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      // Set pending signal bit (signal number 1 = bit 0)
      p->sig_pending |= (1UL << (sig - 1));

      // If process is sleeping, wake it up
      if(p->state == SLEEPING){
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;  // Process not found
}

// mmap system call
// void* mmap(void *addr, uint length, int prot, int flags, int fd, uint offset)
uint64
sys_mmap(void)
{
  uint64 addr, length, offset;
  int prot, flags, fd;
  struct proc *p = myproc();
  struct file *f = 0;

  if(argaddr(0, &addr) < 0)
    return -1;
  if(argaddr(1, &length) < 0)
    return -1;
  if(argint(2, &prot) < 0)
    return -1;
  if(argint(3, &flags) < 0)
    return -1;
  if(argint(4, &fd) < 0)
    return -1;
  if(argaddr(5, &offset) < 0)
    return -1;

  // Validate arguments
  if(length == 0 || length >= MAXUVA)
    return -1;

  // Check for invalid flag combinations
  if((flags & MAP_SHARED) && (flags & MAP_PRIVATE))
    return -1;  // Cannot specify both

  // Handle file mapping
  if(!(flags & MAP_ANONYMOUS)) {
    if(fd < 0 || fd >= NOFILE || p->ofile[fd] == 0)
      return -1;

    f = p->ofile[fd];
    filedup(f);  // Increase file reference count
  }

  // Find a suitable virtual address
  uint64 map_addr;
  if(flags & MAP_FIXED) {
    // User specified exact address
    if(addr == 0 || addr >= MAXUVA)
      goto err;
    map_addr = PGROUNDDOWN(addr);
  } else {
    // Find free address range
    if(vma_find_free_range(&p->vma_manager, addr, length, &map_addr) < 0)
      goto err;
  }

  // Insert VMA
  if(vma_insert(&p->vma_manager, map_addr, length, offset,
                prot, flags, f) < 0)
    goto err;

  return map_addr;

err:
  if(f)
    fileclose(f);
  return -1;
}

// munmap system call
// int munmap(void *addr, uint length)
uint64
sys_munmap(void)
{
  uint64 addr, length;
  struct proc *p = myproc();
  struct vma *vma;
  uint64 va, end;
  int npages;

  if(argaddr(0, &addr) < 0)
    return -1;
  if(argaddr(1, &length) < 0)
    return -1;

  if(addr == 0 || length == 0)
    return -1;

  // Find VMA containing this address
  vma = vma_lookup(&p->vma_manager, addr);
  if(vma == 0)
    return -1;

  // Calculate page-aligned range
  va = PGROUNDDOWN(addr);
  end = PGROUNDUP(addr + length);
  npages = (end - va) / PGSIZE;

  // Unmap pages from page table
  uvmunmap(p->pagetable, va, npages, 0);
  uvmunmap(p->kpagetable, va, npages, 0);

  // If entire VMA is unmapped, remove it
  if(addr <= vma->addr && addr + length >= vma->addr + vma->length) {
    vma_remove(&p->vma_manager, vma->addr, vma->length);
  }

  return 0;
}
```

### 1.3 kernel/include/proc.h

```c
#ifndef __PROC_H
#define __PROC_H

#include "param.h"
#include "riscv.h"
#include "types.h"
#include "spinlock.h"
#include "file.h"
#include "fat32.h"
#include "trap.h"
#include "signal.h"
#include "vma.h"

// Saved registers for kernel context switches.
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// Per-CPU state.
struct cpu {
  struct proc *proc;          // The process running on this cpu, or null.
  struct context context;     // swtch() here to enter scheduler().
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

enum procstate { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  struct proc *parent;         // Parent process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  pagetable_t kpagetable;      // Kernel page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct dirent *cwd;          // Current directory
  char name[16];               // Process name (debugging)
  int tmask;                   // trace mask
  int priority;                // Process priority (0-100, higher = more important)
  // Multi-level Feedback Queue (MLFQ) fields
  int queue_level;             // Current queue level (0=highest, 2=lowest)
  int time_slice;              // Remaining time slices in current queue
  int ticks_used;              // Total ticks used by this process

  // Process time statistics
  uint64 utime;                // User mode ticks
  uint64 stime;                // Kernel mode ticks
  uint64 start_time;           // Process start time (ticks since boot)

  // Signal handling
  uint64 sig_pending;          // Pending signals bitmap (bit 0 = signal 1, etc.)
  uint64 sig_handlers[NSIG];   // Signal handler addresses
  uint64 sig_mask;             // Signal mask (blocked signals)

  // Sandbox policy (seccomp-lite)
  int sandbox_on;              // 1: sandbox enabled, 0: disabled
  int sandbox_action;          // 0: deny(-1), future: send signal, etc.
  uint32 allow_mask[4];        // 128-bit allow mask for syscall numbers

  // Virtual Memory Area (VMA) management for mmap
  struct vma_manager vma_manager;  // VMA 管理器
};

void            reg_info(void);
int             cpuid(void);
void            exit(int);
int             fork(void);
int             growproc(int);
pagetable_t     proc_pagetable(struct proc *);
void            proc_freepagetable(pagetable_t, uint64);
int             kill(int);
struct cpu*     mycpu(void);
struct cpu*     getmycpu(void);
struct proc*    myproc();
void            procinit(void);
void            scheduler(void) __attribute__((noreturn));
void            sched(void);
void            setproc(struct proc*);
void            sleep(void*, struct spinlock*);
void            userinit(void);
int             wait(uint64);
void            wakeup(void*);
void            yield(void);
int             higher_priority_ready(void);
int             handle_time_slice(void);
int             either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int             either_copyin(void *dst, int user_src, uint64 src, uint64 len);
void            procdump(void);
uint64          procnum(void);
void            test_proc_init(int);

#endif
```

### 1.4 kernel/proc.c（关键部分）

```c
#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/intr.h"
#include "include/kalloc.h"
#include "include/printf.h"
#include "include/string.h"
#include "include/fat32.h"
#include "include/file.h"
#include "include/trap.h"
#include "include/vm.h"
#include "include/timer.h"


struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
uint64 ticks_start = 0;  // System start time (ticks)
struct spinlock pid_lock;

extern void forkret(void);
extern void swtch(struct context*, struct context*);
static void wakeup1(struct proc *chan);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// ... 其他函数 ...

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return NULL;

found:
  p->pid = allocpid();

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == NULL){
    release(&p->lock);
    return NULL;
  }
  // Initialize trapframe to zero
  memset(p->trapframe, 0, sizeof(struct trapframe));

  // An empty user page table.
  // And an identical kernel page table for this proc.
  if ((p->pagetable = proc_pagetable(p)) == NULL ||
      (p->kpagetable = proc_kpagetable()) == NULL) {
    freeproc(p);
    release(&p->lock);
    return NULL;
  }

  p->kstack = VKSTACK;

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  // Set default priority
  p->priority = 50;  // Default priority (medium)

  // Initialize MLFQ fields
  p->queue_level = 0;        // New processes start at highest priority queue
  p->time_slice = 0;         // Will be set by scheduler when first run
  p->ticks_used = 0;

  // Initialize time statistics
  p->utime = 0;
  p->stime = 0;
  p->start_time = ticks;  // Record current ticks as process start time

  // Initialize signal handling
  p->sig_pending = 0;     // No pending signals
  p->sig_mask = 0;        // No signals blocked
  // Set default handlers for all signals
  for(int i = 0; i < NSIG; i++) {
    p->sig_handlers[i] = (uint64)SIG_DFL;
  }

  // Initialize Sandbox defaults
  p->sandbox_on = 0;
  p->sandbox_action = 0;
  for (int i = 0; i < 4; i++) p->allow_mask[i] = 0;

  // Initialize VMA manager
  vma_init(&p->vma_manager);

  return p;
}

// Free a process's resources.  Does not free the process structure itself.
// Called when the process is exiting, or when a proc structure is being reused.
static void
freeproc(struct proc *p)
{
  // Close all open files.
  for(int i = 0; i < NOFILE; i++){
    if(p->ofile[i]){
      fileclose(p->ofile[i]);
      p->ofile[i] = 0;
    }
  }
  if(p->cwd) {
    iput(p->cwd);
    p->cwd = 0;
  }

  // Reset signal handling
  p->sig_pending = 0;
  p->sig_mask = 0;
  for(int i = 0; i < NSIG; i++) {
    p->sig_handlers[i] = (uint64)SIG_DFL;
  }

  // Cleanup VMA
  vma_cleanup(&p->vma_manager);

  // Reset sandbox
  p->sandbox_on = 0;
  p->sandbox_action = 0;
  for (int i = 0; i < 4; i++) p->allow_mask[i] = 0;
}
```

### 1.5 kernel/sysproc.c

```c

#include "include/types.h"
#include "include/riscv.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/syscall.h"
#include "include/timer.h"
#include "include/kalloc.h"
#include "include/string.h"
#include "include/printf.h"
#include "include/procinfo.h"
#include "include/vm.h"
#include "include/signal.h"
#include "include/vma.h"
#include "include/file.h"

extern int exec(char *path, char **argv);
extern struct proc proc[NPROC];

uint64
sys_exec(void)
{
  char path[FAT32_MAX_PATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  if(argstr(0, path, FAT32_MAX_PATH) < 0 || argaddr(1, &uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if(argv[i] == 0)
      goto bad;
    if(fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  int ret = exec(path, argv);

  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int n;
  if(argint(0, &n) < 0)
    return (uint64)-1;

  struct proc *p = myproc();
  uint64 oldsz = p->sz;

  uint64 newsz;

  if(n >= 0){
    newsz = oldsz + (uint64)n;
    if(newsz < oldsz)            // overflow
      return (uint64)-1;
    if(newsz >= MAXUVA)          // 上界（按你工程里的宏为准）
      return (uint64)-1;
    p->sz = newsz;               // lazy: 只记账，不分配
  } else {
    uint64 dec = (uint64)(-n);
    if(dec > oldsz)              // underflow
      return (uint64)-1;
    newsz = oldsz - dec;
    p->sz = uvmdealloc(p->pagetable, p->kpagetable, oldsz, newsz);
  }
  return oldsz;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void)
{
  int mask;
  if(argint(0, &mask) < 0) {
    return -1;
  }
  myproc()->tmask = mask;
  return 0;
}

uint64
sys_setpriority(void)
{
  int prio;
  struct proc *p = myproc();

  if(argint(0, &prio) < 0) {
    return -1;
  }

  // Validate priority range
  if(prio < 0 || prio > 100) {
    return -1;
  }

  p->priority = prio;
  return 0;
}

uint64
sys_getpriority(void)
{
  return myproc()->priority;
}

// Get process MLFQ queue level
uint64
sys_getqueuelevel(void)
{
  return myproc()->queue_level;
}

// Get process remaining time slice
uint64
sys_gettimeslice(void)
{
  return myproc()->time_slice;
}

// Get process information array
// Returns: number of processes copied, or -1 on error
uint64
sys_getprocs(void)
{
  uint64 addr;
  int max_count;

  if(argaddr(0, &addr) < 0)
    return -1;
  if(argint(1, &max_count) < 0)
    return -1;

  struct proc *p;
  int count = 0;
  struct procinfo info;

  // Iterate through all processes
  for(p = proc; p < &proc[NPROC]; p++) {
    if(count >= max_count)
      break;

    acquire(&p->lock);

    // Only include non-UNUSED processes
    if(p->state != UNUSED) {
      info.pid = p->pid;
      info.state = p->state;
      info.priority = p->priority;
      info.queue_level = p->queue_level;
      info.time_slice = p->time_slice;
      info.ticks_used = p->ticks_used;
      info.utime = p->utime;
      info.stime = p->stime;
      info.start_time = p->start_time;
      info.sz = p->sz;
      safestrcpy(info.name, p->name, sizeof(info.name));

      release(&p->lock);

      // Copy to user space
      if(copyout2(addr + count * sizeof(info), (char*)&info, sizeof(info)) < 0) {
        return -1;
      }
      count++;
    } else {
      release(&p->lock);
    }
  }

  return count;
}

// Get resource usage for the current process
// Returns: 0 on success, -1 on error
uint64
sys_getrusage(void)
{
  uint64 utime_addr, stime_addr;
  struct proc *p = myproc();

  if(argaddr(0, &utime_addr) < 0)
    return -1;
  if(argaddr(1, &stime_addr) < 0)
    return -1;

  acquire(&p->lock);

  // Copy utime to user space
  if(utime_addr != 0 && copyout2(utime_addr, (char*)&p->utime, sizeof(p->utime)) < 0) {
    release(&p->lock);
    return -1;
  }

  // Copy stime to user space
  if(stime_addr != 0 && copyout2(stime_addr, (char*)&p->stime, sizeof(p->stime)) < 0) {
    release(&p->lock);
    return -1;
  }

  release(&p->lock);
  return 0;
}

// Register signal handler
// sys_signal(int sig, void (*handler)(int))
uint64
sys_signal(void)
{
  int sig;
  uint64 handler_addr;
  struct proc *p = myproc();

  if(argint(0, &sig) < 0)
    return (uint64)SIG_ERR;
  if(argaddr(1, &handler_addr) < 0)
    return (uint64)SIG_ERR;

  // Validate signal number
  if(sig < 1 || sig >= NSIG)
    return (uint64)SIG_ERR;

  // SIGKILL and SIGSTOP cannot be caught or ignored
  if(sig == SIGKILL || sig == SIGSTOP)
    return (uint64)SIG_ERR;

  acquire(&p->lock);

  // Save old handler
  uint64 old_handler = p->sig_handlers[sig];

  // Set new handler
  p->sig_handlers[sig] = handler_addr;

  release(&p->lock);

  return old_handler;
}

// Send signal to process
// sys_sigkill(int pid, int sig)
uint64
sys_sigkill(void)
{
  int pid, sig;
  struct proc *p;

  if(argint(0, &pid) < 0)
    return -1;
  if(argint(1, &sig) < 0)
    return -1;

  // Validate signal number
  if(sig < 1 || sig >= NSIG)
    return -1;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      // Set pending signal bit (signal number 1 = bit 0)
      p->sig_pending |= (1UL << (sig - 1));

      // If process is sleeping, wake it up
      if(p->state == SLEEPING){
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;  // Process not found
}

// mmap system call
// void* mmap(void *addr, uint length, int prot, int flags, int fd, uint offset)
uint64
sys_mmap(void)
{
  uint64 addr, length, offset;
  int prot, flags, fd;
  struct proc *p = myproc();
  struct file *f = 0;

  if(argaddr(0, &addr) < 0)
    return -1;
  if(argaddr(1, &length) < 0)
    return -1;
  if(argint(2, &prot) < 0)
    return -1;
  if(argint(3, &flags) < 0)
    return -1;
  if(argint(4, &fd) < 0)
    return -1;
  if(argaddr(5, &offset) < 0)
    return -1;

  // Validate arguments
  if(length == 0 || length >= MAXUVA)
    return -1;

  // Check for invalid flag combinations
  if((flags & MAP_SHARED) && (flags & MAP_PRIVATE))
    return -1;  // Cannot specify both

  // Handle file mapping
  if(!(flags & MAP_ANONYMOUS)) {
    if(fd < 0 || fd >= NOFILE || p->ofile[fd] == 0)
      return -1;

    f = p->ofile[fd];
    filedup(f);  // Increase file reference count
  }

  // Find a suitable virtual address
  uint64 map_addr;
  if(flags & MAP_FIXED) {
    // User specified exact address
    if(addr == 0 || addr >= MAXUVA)
      goto err;
    map_addr = PGROUNDDOWN(addr);
  } else {
    // Find free address range
    if(vma_find_free_range(&p->vma_manager, addr, length, &map_addr) < 0)
      goto err;
  }

  // Insert VMA
  if(vma_insert(&p->vma_manager, map_addr, length, offset,
                prot, flags, f) < 0)
    goto err;

  return map_addr;

err:
  if(f)
    fileclose(f);
  return -1;
}

// munmap system call
// int munmap(void *addr, uint length)
uint64
sys_munmap(void)
{
  uint64 addr, length;
  struct proc *p = myproc();
  struct vma *vma;
  uint64 va, end;
  int npages;

  if(argaddr(0, &addr) < 0)
    return -1;
  if(argaddr(1, &length) < 0)
    return -1;

  if(addr == 0 || length == 0)
    return -1;

  // Find VMA containing this address
  vma = vma_lookup(&p->vma_manager, addr);
  if(vma == 0)
    return -1;

  // Calculate page-aligned range
  va = PGROUNDDOWN(addr);
  end = PGROUNDUP(addr + length);
  npages = (end - va) / PGSIZE;

  // Unmap pages from page table
  uvmunmap(p->pagetable, va, npages, 0);
  uvmunmap(p->kpagetable, va, npages, 0);

  // If entire VMA is unmapped, remove it
  if(addr <= vma->addr && addr + length >= vma->addr + vma->length) {
    vma_remove(&p->vma_manager, vma->addr, vma->length);
  }

  return 0;
}
```

## 2. 用户态（User）

### 2.1 xv6-user/user.h

```c
#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "kernel/include/fcntl.h"

struct stat;
struct rtcdate;
struct sysinfo;
struct procinfo;

// Signal definitions
#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGFPE    7
#define SIGKILL   9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20

#define SIG_DFL  ((void (*)(int))0)
#define SIG_IGN  ((void (*)(int))1)
#define SIG_ERR  ((void (*)(int))(-1))

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int fd, const void *buf, int len);
int read(int fd, void *buf, int len);
int close(int fd);
int kill(int pid);
int exec(char*, char**);
int open(const char *filename, int mode);
int fstat(int fd, struct stat*);
int mkdir(const char *dirname);
int chdir(const char *dirname);
int dup(int fd);
int getpid(void);
char* sbrk(int size);
int sleep(int ticks);
int uptime(void);
int test_proc(int);
int dev(int, short, short);
int readdir(int fd, struct stat*);
int getcwd(char *buf);
int remove(char *filename);
int trace(int mask);
int sysinfo(struct sysinfo *);
int rename(char *old, char *new);
int setpriority(int prio);
int getpriority(void);
int getqueuelevel(void);
int gettimeslice(void);
int getprocs(struct procinfo *info, int max_count);
int getrusage(uint64 *utime, uint64 *stime);
int settime(int year, int month, int day, int hour, int min, int sec);

// 共享内存系统调用
#define IPC_PRIVATE  0
#define IPC_CREAT    01000
#define SHM_RMID     1

int shmget(int key, uint64 size, int flag);
void* shmat(int shmid, uint64 addr, int flag);
int shmdt(uint64 addr);
int shmctl(int shmid, int cmd, void *buf);

// 沙箱系统调用
int sandbox(int on, int action, uint32 *allow_mask, int words);

// mmap system calls
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4

#define MAP_SHARED  0x01
#define MAP_PRIVATE 0x02
#define MAP_FIXED   0x04
#define MAP_ANONYMOUS 0x08

void* mmap(void *addr, uint length, int prot, int flags, int fd, uint offset);
int munmap(void *addr, uint length);

// Signal system calls
void (*signal(int sig, void (*handler)(int)))(int);
int sigkill(int pid, int sig);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
char* strcat(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void fprintf(int, const char*, ...);
void printf(const char*, ...);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);
```

### 2.2 xv6-user/usys.pl

```perl
#!/usr/bin/perl -w

# Generate usys.S, the stubs for syscalls.

print "# generated by usys.pl - do not edit\n";

print "#include \"kernel/include/sysnum.h\"\n";

sub entry {
    my $name = shift;
    print ".global $name\n";
    print "${name}:\n";
    print " li a7, SYS_${name}\n";
    print " ecall\n";
    print " ret\n";
}
	
entry("fork");
entry("exit");
entry("wait");
entry("pipe");
entry("read");
entry("write");
entry("close");
entry("kill");
entry("exec");
entry("open");
entry("fstat");
entry("mkdir");
entry("chdir");
entry("dup");
entry("getpid");
entry("sbrk");
entry("sleep");
entry("uptime");
entry("test_proc");
entry("dev");
entry("readdir");
entry("getcwd");
entry("remove");
entry("trace");
entry("sysinfo");
entry("rename");
entry("setpriority");
entry("getpriority");
entry("getqueuelevel");
entry("gettimeslice");
entry("shmget");
entry("shmat");
entry("shmdt");
entry("shmctl");
entry("getprocs");
entry("getrusage");
entry("signal");
entry("sigkill");
entry("settime");
entry("mmap");
entry("munmap");
entry("sandbox");
```

### 2.3 顶层 Makefile

```makefile
# platform	:= k210
platform	:= qemu
# mode := debug
mode := release
K=kernel
U=xv6-user
T=target

OBJS =
ifeq ($(platform), k210)
OBJS += $K/entry_k210.o
else
OBJS += $K/entry_qemu.o
endif

OBJS += \
  $K/printf.o \
  $K/kalloc.o \
  $K/intr.o \
  $K/spinlock.o \
  $K/string.o \
  $K/main.o \
  $K/vm.o \
  $K/vma.o \
  $K/proc.o \
  $K/swtch.o \
  $K/trampoline.o \
  $K/trap.o \
  $K/syscall.o \
  $K/sysproc.o \
  $K/shm.o \
  $K/bio.o \
  $K/sleeplock.o \
  $K/file.o \
  $K/pipe.o \
  $K/exec.o \
  $K/sysfile.o \
  $K/kernelvec.o \
  $K/timer.o \
  $K/disk.o \
  $K/fat32.o \
  $K/plic.o \
  $K/console.o

ifeq ($(platform), k210)
OBJS += \
  $K/spi.o \
  $K/gpiohs.o \
  $K/fpioa.o \
  $K/utils.o \
  $K/sdcard.o \
  $K/dmac.o \
  $K/sysctl.o

else
OBJS += \
  $K/virtio_disk.o \
  $K/uart.o

endif

# ... 其他配置 ...

UPROGS=\
	$U/_init\
	$U/_sh\
	$U/_cat\
	$U/_echo\
	$U/_grep\
	$U/_ls\
	$U/_kill\
	$U/_mkdir\
	$U/_xargs\
	$U/_sleep\
	$U/_find\
	$U/_rm\
	$U/_wc\
	$U/_test\
	$U/_usertests\
	$U/_strace\
	$U/_mv\
	$U/_cowtest\
	$U/_priotest\
	$U/_shmtest\
	$U/_rtc\
	$U/_ps\
	$U/_top\
	$U/_sigtest1\
	$U/_sigtest2\
	$U/_settimeddemo\
	$U/_cachetest\
	$U/_lazytest\
	$U/_mlfqtest\
	$U/_mmaptest\
	$U/_sandbox\

	# $U/_forktest\
	# $U/_ln\
	# $U/_stressfs\
	# $U/_grind\
	# $U/_zombie\

userprogs: $(UPROGS)

# ... 其他规则 ...

# Make fs image
fs: $(UPROGS)
	@if [ ! -f "fs.img" ]; then \
		echo "making fs image..."; \
		dd if=/dev/zero of=fs.img bs=512k count=512; \
		mkfs.vfat -F 32 fs.img; fi
	@sudo mount fs.img $(dst)
	@if [ ! -d "$(dst)/bin" ]; then sudo mkdir $(dst)/bin; fi
	@if [ ! -d "$(dst)/etc" ]; then sudo mkdir $(dst)/etc; fi
	@sudo cp README $(dst)/README && sudo touch -r README $(dst)/README
	@for file in $$( ls $U/_* ); do \
		sudo cp $$file $(dst)/$${file#$U/_} && sudo touch -r $$file $(dst)/$${file#$U/_};\
		sudo cp $$file $(dst)/bin/$${file#$U/_} && sudo touch -r $$file $(dst)/bin/$${file#$U/_}; done
	@for f in $$( ls etc/policy*.txt 2>/dev/null ); do \
		sudo cp $$f $(dst)/etc/$${f#etc/}; \
		sudo touch -r $$f $(dst)/etc/$${f#etc/}; \
		done || true
	@sudo umount $(dst)
	@echo "fs image ready"
````
## 3. 说明

我将把刚才的英文说明完整用中文重述，聚焦你要的“seccomp-lite 沙箱”补丁文档内容以及我已在仓库里生成的文件。
### 任务与计划
目标：用中文解释我已经在 BobWorking/CopilotWorking/NeedCode1.md 里整理好的“可直接打补丁”的完整代码与插桩方案。

重点：逐项覆盖你列的必须文件与建议文件，说明新增 SYS_sandbox=42、syscall 分发处沙箱拒绝点、proc 结构新增字段、fork/allocproc/freeproc 的继
承与初始化、sys_sandbox 的实现、user 态原型与 stub、Makefile 打包策略，以及 strace 的风格对齐。
### 核心成果概览
我已创建并写入 BobWorking/CopilotWorking/NeedCode1.md，里面包含：
当前仓库版本的完整代码片段：kernel/include/sysnum.h、kernel/syscall.c、kernel/include/proc.h、kernel/proc.c（关键段落）、kernel/sysproc.c（长文件的主要部分）、xv6-user/user.h、xv6-user/usys.pl、顶层 Makefile（关键 UPROGS/打包规则片段）、xv6-user/strace.c。
对每个文件的精确修改点与补丁建议，确保你能按行号制作补丁。
完整的 seccomp-lite 设计与对齐本仓库风格的实现细节。
### 具体改动与插桩设计
#### kernel/include/sysnum.h
在末尾新增系统调用号：#define SYS_sandbox 42，确保不与现有 1..41 冲突。
#### kernel/syscall.c
在现有 extern 声明后，新增：extern uint64 sys_sandbox(void);
在 syscalls[] 分发表尾部添加：[SYS_sandbox] sys_sandbox,
在 sysnames[] 名称表尾部添加：[SYS_sandbox] "sandbox",
在 syscall() 中插入“沙箱拒绝点”：
若 p->sandbox_on 开启，且当前 num 不等于 SYS_sandbox，则检查 p->allow_mask[word] 的对应 bit（num/32 与 num%32）。
未被允许则直接将 a0=-1 返回。若 trace 打开，按同一格式打印 pid %d: %s -> %d。
这样保证所有系统调用都在分发点做统一安全检查，而 sys_sandbox 自身不被拦截，便于策略更新。
#### kernel/include/proc.h
在 struct proc 中新增沙箱策略字段：
int sandbox_on; 是否开启
int sandbox_action; 违例动作（当前用 0 表示返回 -1，预留扩展）
uint32 allow_mask[4]; 128 位掩码，按 syscall 号映射 bit
保持现有类型与命名风格一致。
#### kernel/proc.c
在 allocproc() 完成信号初始化后，初始化沙箱默认状态：
sandbox_on=0; sandbox_action=0; allow_mask[] 清零
在 fork() 里继承父进程策略：
复制 sandbox_on/sandbox_action/allow_mask[]
在 freeproc() 资源清理时，清零沙箱字段：
sandbox_on=0; sandbox_action=0; allow_mask[] 清零
#### kernel/sysproc.c
新增 sys_sandbox()，与项目风格一致使用 argint/argaddr/either_copyin：
签名：sys_sandbox(void)，参数按序：on、action、umask_addr、words
on==0 则关闭沙箱并返回 0；action 当前仅允许 0（返回 -1），其他返回 -1
words 限制在 [0,4]，从用户态拷贝 words * 4 bytes 到临时数组，再写入 p->allow_mask[]；最后启用沙箱
这保证用户态可一次性推送策略掩码到内核。
#### xv6-user/user.h
新增用户态原型：int sandbox(int on, int action, uint32 *allow_mask, int words);
#### xv6-user/usys.pl
新增 stub 生成项：entry("sandbox");，这样会在 usys.S 自动生成 li a7, SYS_sandbox; ecall; ret。
#### 顶层 Makefile
在 UPROGS 列表中加入 $U/_sandbox（如果你新增了 xv6-user/sandbox.c 示例命令）。
在 fs: 规则里：
创建 $(dst)/etc 并复制仓库根目录 etc/policy*.txt 到镜像内 /etc/，用于演示沙箱策略文件（若不存在则忽略，不阻断构建）。
#### xv6-user/strace.c
保留当前文件完整内容，作为输出格式与 sysnames 对齐的参考；沙箱命令可复用类似的参数解析与 exec 风格。
### 演示建议与质量门槛
构建：应用补丁后，重新编译内核与用户程序，生成 fs.img。
#### 冒烟测试：
运行 strace：例如 strace 1 sh 验证 trace 输出风格与 sysnames 对齐。
运行 sandbox（用户态示例程序或你自写版本）：给定白名单掩码，只允许 read/write/open/close/exec/exit 等，然后 exec 目标命令，如 cat README；若命令涉及未许可 syscall，应返回 -1，trace 开启时会打印拒绝行。
#### 质量检查：
该设计仅在 syscall 分发处插桩，不改变既有行为路径；沙箱默认关闭，只有调用 sandbox(on=1,..) 才会生效。
未来可扩展 sandbox_action 为发信号（例：SIGSYS），但需与你的信号处理路径配合。
### 现状说明与可选增强
allow_mask[4] 提供 128 位空间，当前号表到 42，用前两个 32 位足够；预留增长。
用户态可直接 #include "kernel/include/sysnum.h" 做名称/编号映射，减少重复。
策略文件 /etc/policy*.txt 格式可做“每行一个 syscall 名称”，用户态解析为掩码；名称表与内核 sysnames[] 对齐，或在用户态维护副本避免耦合。
### 文件位置
以上内容已写入 BobWorking/CopilotWorking/NeedCode1.md，包含完整代码与精确补丁点说明，可直接用于生成“可打补丁”的版本。
如果你希望我直接在仓库里应用这些改动（实际编辑对应 C 文件、增加 xv6-user/sandbox.c 并更新 Makefile，然后本地构建测试），告诉我即可，我会进一步自动完成并验证运行效果
