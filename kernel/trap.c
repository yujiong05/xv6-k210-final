#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/sbi.h"
#include "include/plic.h"
#include "include/trap.h"
#include "include/syscall.h"
#include "include/signal.h"
#include "include/printf.h"
#include "include/console.h"
#include "include/timer.h"
#include "include/disk.h"
#include "include/vm.h"
#include "include/vma.h"

extern char trampoline[], uservec[], userret[];

// COW分配的前向声明
int cow_alloc(pagetable_t pagetable, uint64 va);

// in kernelvec.S, calls kerneltrap().
extern void kernelvec();

int devintr();

// void
// trapinit(void)
// {
//   initlock(&tickslock, "time");
//   #ifdef DEBUG
//   printf("trapinit\n");
//   #endif
// }

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
  w_sstatus(r_sstatus() | SSTATUS_SIE);
  // enable supervisor-mode timer interrupts.
  w_sie(r_sie() | SIE_SEIE | SIE_SSIE | SIE_STIE);
  set_next_timeout();
  #ifdef DEBUG
  printf("trapinithart\n");
  #endif
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  // printf("run in usertrap\n");
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();

  // save user program counter.
  p->trapframe->epc = r_sepc();

  // Check if we're returning from a signal handler
  if (p->trapframe->signal_ret_pc != 0) {
    // Restore the original PC
    p->trapframe->epc = p->trapframe->signal_ret_pc;
    p->trapframe->signal_ret_pc = 0;
  }

  if(r_scause() == 8){
    // system call
    if(p->killed)
      exit(-1);
    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;
    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();
    syscall();
  }
   else if((which_dev = devintr()) != 0){
    // ok
  } else if(r_scause() == 13 || r_scause() == 15){
    uint64 va = r_stval();
    uint64 a  = PGROUNDDOWN(va);

    // 1) COW 优先：通常只有写 fault（15）需要 COW 修复
    if(r_scause() == 15 && is_cow_page(p->pagetable, a)){
      if(cow_alloc(p->pagetable, a) < 0)
        p->killed = 1;
      goto done_pf;
    }

    // 2) VMA (mmap) 检查：检查是否在 mmap 区域
    struct vma *vma = vma_lookup(&p->vma_manager, a);
    if(vma != 0) {
      // 在 VMA 区域，分配页面
      // 检查权限
      int perm = PTE_U;
      if(vma->prot & PROT_READ)
        perm |= PTE_R;
      if(vma->prot & PROT_WRITE)
        perm |= PTE_W;
      if(vma->prot & PROT_EXEC)
        perm |= PTE_X;

      // MAP_PRIVATE 使用 COW
      if(vma->flags & MAP_PRIVATE)
        perm = (perm | PTE_COW) & ~PTE_W;

      // 懒惰分配页面
      if(lazy_alloc(p->pagetable, p->kpagetable, a) < 0) {
        p->killed = 1;
        goto done_pf;
      }

      // 更新页面权限
      pte_t *pte = walk(p->pagetable, a, 0);
      if(pte != 0) {
        *pte = (*pte & ~(PTE_R | PTE_W | PTE_X)) | perm;
      }
      goto done_pf;
    }

    // 3) lazy allocation：只允许补"已通过 sbrk 扩过的范围"
    if(a >= p->sz || a >= MAXUVA){
      p->killed = 1;
      goto done_pf;
    }

    if(lazy_alloc(p->pagetable, p->kpagetable, a) < 0){
      p->killed = 1; // OOM 或映射失败
      goto done_pf;
    }

  done_pf:
  ;
    // 正常返回，让用户态重试该指令
  }
  else {
    printf("\nusertrap(): unexpected scause %p pid=%d %s\n", r_scause(), p->pid, p->name);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    // trapframedump(p->trapframe);
    p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  // Process pending signals before returning to user space
  // Handle at most one signal per trap to avoid starvation
  process_signals();

  // give up the CPU if this is a timer interrupt.
  // handle_time_slice() will decrement time slice and demote if needed.
  if(which_dev == 2) {
    // Update user mode time statistics
    if(p != 0) {
      acquire(&p->lock);
      p->utime++;
      release(&p->lock);
    }
    if(handle_time_slice() || higher_priority_ready())
      yield();
  }

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  // printf("[usertrapret]p->pagetable: %p\n", p->pagetable);
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap() {
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("\nscause %p\n", scause);
    printf("sepc=%p stval=%p hart=%d\n", r_sepc(), r_stval(), r_tp());
    struct proc *p = myproc();
    if (p != 0) {
      printf("pid: %d, name: %s\n", p->pid, p->name);
    }
    panic("kerneltrap");
  }
  // printf("which_dev: %d\n", which_dev);

  // give up the CPU if this is a timer interrupt.
  // handle_time_slice() will decrement time slice and demote if needed.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING) {
    // Update kernel mode time statistics
    struct proc *p = myproc();
    acquire(&p->lock);
    p->stime++;
    release(&p->lock);
    if(handle_time_slice() || higher_priority_ready()) {
      yield();
    }
  }
  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

// Process pending signals for a user process
// Returns 1 if a signal was handled, 0 otherwise
int
process_signals(void)
{
  struct proc *p = myproc();
  if (p == 0)
    return 0;

  acquire(&p->lock);

  // Check if there are any pending signals (not blocked by mask)
  uint64 pending = p->sig_pending & ~p->sig_mask;
  if (pending == 0) {
    release(&p->lock);
    return 0;
  }

  // Find the first pending signal (lowest bit set)
  int sig = 0;
  for (int i = 0; i < NSIG; i++) {
    if (pending & (1UL << i)) {
      sig = i + 1;  // Signal numbers start at 1
      break;
    }
  }

  if (sig == 0) {
    release(&p->lock);
    return 0;
  }

  // Clear the pending bit
  p->sig_pending &= ~(1UL << (sig - 1));
  release(&p->lock);

  // Handle special signals first
  if (sig == SIGKILL) {
    p->killed = 1;
    return 1;
  }

  if (sig == SIGSTOP) {
    // Stop the process - put it to sleep
    acquire(&p->lock);
    if (p->state == RUNNING) {
      p->state = SLEEPING;
      p->chan = (void*)1;  // Use a non-zero chan to indicate signal stop
    }
    release(&p->lock);
    return 1;
  }

  if (sig == SIGCONT) {
    // Continue a stopped process
    acquire(&p->lock);
    if (p->state == SLEEPING && p->chan == (void*)1) {
      p->state = RUNNABLE;
      p->chan = 0;
    }
    release(&p->lock);
    return 1;
  }

  // Get the handler for this signal
  uint64 handler = p->sig_handlers[sig];

  if (handler == (uint64)SIG_IGN) {
    // Signal is ignored
    return 1;
  }

  if (handler == (uint64)SIG_DFL) {
    // Default handling
    // For most signals, default is to terminate the process
    // Some signals should be ignored by default
    if (sig == SIGCHLD || sig == SIGCONT || sig == SIGWINCH) {
      return 1;
    }
    // Default action: terminate process
    p->killed = 1;
    return 1;
  }

  // User-defined handler
  // Set up trapframe to call the handler
  // When the handler returns, execution will continue from signal_ret_pc
  if (p->trapframe->signal_ret_pc == 0) {
    // Save current PC and set up to call handler
    p->trapframe->signal_ret_pc = p->trapframe->epc;
    p->trapframe->epc = handler;
    p->trapframe->a0 = sig;  // Pass signal number as argument
  }

  return 1;
}

// Check if it's an external/software interrupt, 
// and handle it. 
// returns  2 if timer interrupt, 
//          1 if other device, 
//          0 if not recognized. 
int devintr(void) {
	uint64 scause = r_scause();

	#ifdef QEMU 
	// handle external interrupt 
	if ((0x8000000000000000L & scause) && 9 == (scause & 0xff)) 
	#else 
	// on k210, supervisor software interrupt is used 
	// in alternative to supervisor external interrupt, 
	// which is not available on k210. 
	if (0x8000000000000001L == scause && 9 == r_stval()) 
	#endif 
	{
		int irq = plic_claim();
		if (UART_IRQ == irq) {
			// keyboard input 
			int c = sbi_console_getchar();
			if (-1 != c) {
				consoleintr(c);
			}
		}
		else if (DISK_IRQ == irq) {
			disk_intr();
		}
		else if (irq) {
			printf("unexpected interrupt irq = %d\n", irq);
		}

		if (irq) { plic_complete(irq);}

		#ifndef QEMU 
		w_sip(r_sip() & ~2);    // clear pending bit
		sbi_set_mie();
		#endif 

		return 1;
	}
	else if (0x8000000000000005L == scause) {
		timer_tick();
		return 2;
	}
	else { return 0;}
}

void trapframedump(struct trapframe *tf)
{
  printf("a0: %p\t", tf->a0);
  printf("a1: %p\t", tf->a1);
  printf("a2: %p\t", tf->a2);
  printf("a3: %p\n", tf->a3);
  printf("a4: %p\t", tf->a4);
  printf("a5: %p\t", tf->a5);
  printf("a6: %p\t", tf->a6);
  printf("a7: %p\n", tf->a7);
  printf("t0: %p\t", tf->t0);
  printf("t1: %p\t", tf->t1);
  printf("t2: %p\t", tf->t2);
  printf("t3: %p\n", tf->t3);
  printf("t4: %p\t", tf->t4);
  printf("t5: %p\t", tf->t5);
  printf("t6: %p\t", tf->t6);
  printf("s0: %p\n", tf->s0);
  printf("s1: %p\t", tf->s1);
  printf("s2: %p\t", tf->s2);
  printf("s3: %p\t", tf->s3);
  printf("s4: %p\n", tf->s4);
  printf("s5: %p\t", tf->s5);
  printf("s6: %p\t", tf->s6);
  printf("s7: %p\t", tf->s7);
  printf("s8: %p\n", tf->s8);
  printf("s9: %p\t", tf->s9);
  printf("s10: %p\t", tf->s10);
  printf("s11: %p\t", tf->s11);
  printf("ra: %p\n", tf->ra);
  printf("sp: %p\t", tf->sp);
  printf("gp: %p\t", tf->gp);
  printf("tp: %p\t", tf->tp);
  printf("epc: %p\n", tf->epc);
}
