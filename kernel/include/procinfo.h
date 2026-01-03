#ifndef __PROCINFO_H
#define __PROCINFO_H

#include "types.h"

// Process information structure for sys_getprocs system call
struct procinfo {
  int pid;              // Process ID
  int state;            // Process state (UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE)
  int priority;         // Process priority (0-100)
  int queue_level;      // MLFQ queue level (0=highest, 2=lowest)
  int time_slice;       // Remaining time slices
  uint64 ticks_used;    // Total ticks used
  uint64 utime;         // User mode ticks
  uint64 stime;         // Kernel mode ticks
  uint64 start_time;    // Process start time (ticks since boot)
  uint64 sz;            // Process memory size (bytes)
  char name[16];        // Process name
};

#endif
