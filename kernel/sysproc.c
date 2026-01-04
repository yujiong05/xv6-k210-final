
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
