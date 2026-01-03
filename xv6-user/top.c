// top command - display processes in real-time
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

// Clear screen (simple implementation)
void clear_screen() {
  printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
}

int main(int argc, char *argv[])
{
  int count;
  int iterations = 0;
  int max_iterations = 10;  // Run for 10 iterations by default

  // Parse optional iteration count
  if(argc > 1) {
    max_iterations = atoi(argv[1]);
    if(max_iterations <= 0)
      max_iterations = 10;
  }

  while(iterations < max_iterations) {
    clear_screen();

    // Get system uptime
    uint64 uptime_sec = uptime() / 10;  // ticks / 10 = seconds (assuming 100Hz)

    printf("top - %d iterations remaining - up %d seconds\n", max_iterations - iterations, (int)uptime_sec);
    printf("\n");

    // Get all process information
    count = getprocs(info, NPROC);
    if(count < 0) {
      printf("top: getprocs failed\n");
      exit(1);
    }

    // Print header
    printf("PID   STATE     PRIO QLV TICKS UTIME STIME CPU%%  NAME\n");
    printf("----  --------  ---- --- ----- ----- ----- ----- ----\n");

    // Calculate total CPU time for percentage calculation
    uint64 total_cpu = 0;
    for(int i = 0; i < count; i++) {
      total_cpu += info[i].utime + info[i].stime;
    }

    // Print process information
    for(int i = 0; i < count; i++) {
      const char *state_str;
      if(info[i].state >= 0 && info[i].state <= 4) {
        state_str = state_names[info[i].state];
      } else {
        state_str = "UNKNOWN ";
      }

      // Calculate CPU percentage
      int cpu_percent = 0;
      if(total_cpu > 0) {
        cpu_percent = (int)((info[i].utime + info[i].stime) * 100 / total_cpu);
      }

      printf("%d %s %d %d %d %d %d %d %s\n",
             info[i].pid,
             state_str,
             info[i].priority,
             info[i].queue_level,
             (int)info[i].ticks_used,
             (int)info[i].utime,
             (int)info[i].stime,
             cpu_percent,
             info[i].name);
    }

    printf("\nTotal processes: %d\n", count);

    iterations++;
    if(iterations < max_iterations) {
      sleep(10);  // Sleep for 1 second (10 ticks)
    }
  }

  exit(0);
}
