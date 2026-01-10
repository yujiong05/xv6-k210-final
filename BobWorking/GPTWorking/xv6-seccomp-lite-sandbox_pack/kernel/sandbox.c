#include "include/types.h"
#include "include/riscv.h"
#include "include/param.h"
#include "include/proc.h"
#include "include/syscall.h"
#include "include/string.h"

// sys_sandbox(on, action, allow_mask, words)
//
// on=0: disable sandbox (and clear policy)
// on=1: enable sandbox with allow-mask
//
// action=0: deny by returning -1
// action=1: deny and kill the process (p->killed=1)
//
// allow_mask: user pointer to uint32 array
// words: number of uint32 words to copy (0..4)
//
// Note: syscall dispatch checks p->allow_mask[].
// This is a "seccomp-lite" design: simple allowlist bitmap.
uint64
sys_sandbox(void)
{
  int on, action, words;
  uint64 umask_addr;

  if (argint(0, &on) < 0 ||
      argint(1, &action) < 0 ||
      argaddr(2, &umask_addr) < 0 ||
      argint(3, &words) < 0) {
    return -1;
  }

  struct proc *p = myproc();

  if (on == 0) {
    p->sandbox_on = 0;
    p->sandbox_action = 0;
    memset(p->allow_mask, 0, sizeof(p->allow_mask));
    return 0;
  }

  if (on != 1) {
    return -1;
  }
  if (action != 0 && action != 1) {
    return -1;
  }
  if (words < 0 || words > 4) {
    return -1;
  }

  uint32 tmp[4] = {0};

  if (words > 0) {
    uint64 nbytes = (uint64)words * sizeof(uint32);
    if (either_copyin(tmp, 1, umask_addr, nbytes) < 0) {
      return -1;
    }
  }

  p->sandbox_on = 1;
  p->sandbox_action = action;
  for (int i = 0; i < 4; i++) {
    p->allow_mask[i] = tmp[i];
  }
  return 0;
}
