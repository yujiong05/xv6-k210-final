// ps command - list all processes
#include "kernel/include/types.h"
#include "kernel/include/param.h"
#include "kernel/include/procinfo.h"
#include "user.h"

// Process state names
static char *state_names[] = {
  "UNUSED  ",
  "SLEEPING",
  "RUNNABLE",
  "RUNNING ",
  "ZOMBIE  "
};

// Use static array to avoid stack overflow
static struct procinfo info[NPROC];

int main(int argc, char *argv[])
{
  int count;

  // Get all process information
  count = getprocs(info, NPROC);
  if(count < 0) {
    printf("ps: getprocs failed\n");
    exit(1);
  }

  // Print header
  printf("PID   STATE     PRIO QLV TIME_SLICE TICKS UTIME STIME START_T  SZ  NAME\n");
  printf("----  --------  ---- --- --------- ----- ----- ----- ------ ----- ----\n");

  // Print process information
  for(int i = 0; i < count; i++) {
    const char *state_str;
    if(info[i].state >= 0 && info[i].state <= 4) {
      state_str = state_names[info[i].state];
    } else {
      state_str = "UNKNOWN ";
    }

    printf("%d %s %d %d %d %d %d %d %d %d %s\n",
           info[i].pid,
           state_str,
           info[i].priority,
           info[i].queue_level,
           info[i].time_slice,
           (int)info[i].ticks_used,
           (int)info[i].utime,
           (int)info[i].stime,
           (int)info[i].start_time,
           (int)info[i].sz,
           info[i].name);
  }

  exit(0);
}
