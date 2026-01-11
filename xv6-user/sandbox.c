#include "kernel/include/types.h"
#include "kernel/include/param.h"
#include "kernel/include/fcntl.h"
#include "kernel/include/sysnum.h"
#include "user.h"

// sandbox [-e|-k] <policy_file> <cmd> [args...]
//
// policy_file format:
//   - one syscall name per line, e.g. "open", "read"
//   - empty lines allowed
//   - lines starting with '#' are comments
//
// action:
//   -e : deny by returning -1 (default)
//   -k : deny and kill the process
//
// Notes:
// - This wrapper will ALWAYS allow: exec, exit, sandbox (to avoid self-footgun).
// - The sandbox policy is inherited by fork() if you copied the fields in proc.c.

struct name_num {
  const char *name;
  int num;
};

static struct name_num table[] = {
  {"fork", SYS_fork},
  {"exit", SYS_exit},
  {"wait", SYS_wait},
  {"pipe", SYS_pipe},
  {"read", SYS_read},
  {"kill", SYS_kill},
  {"exec", SYS_exec},
  {"fstat", SYS_fstat},
  {"chdir", SYS_chdir},
  {"dup", SYS_dup},
  {"getpid", SYS_getpid},
  {"sbrk", SYS_sbrk},
  {"sleep", SYS_sleep},
  {"uptime", SYS_uptime},
  {"open", SYS_open},
  {"write", SYS_write},
  {"mkdir", SYS_mkdir},
  {"close", SYS_close},
  {"test_proc", SYS_test_proc},
  {"dev", SYS_dev},
  {"readdir", SYS_readdir},
  {"getcwd", SYS_getcwd},
  {"remove", SYS_remove},
  {"trace", SYS_trace},
  {"sysinfo", SYS_sysinfo},
  {"rename", SYS_rename},
  {"setpriority", SYS_setpriority},
  {"getpriority", SYS_getpriority},
  {"shmget", SYS_shmget},
  {"shmat", SYS_shmat},
  {"shmdt", SYS_shmdt},
  {"shmctl", SYS_shmctl},
  {"getqueuelevel", SYS_getqueuelevel},
  {"gettimeslice", SYS_gettimeslice},
  {"getprocs", SYS_getprocs},
  {"getrusage", SYS_getrusage},
  {"signal", SYS_signal},
  {"sigkill", SYS_sigkill},
  {"settime", SYS_settime},
  {"mmap", SYS_mmap},
  {"munmap", SYS_munmap},
  {"sandbox", SYS_sandbox},
};

static void
setbit(uint32 mask[4], int num)
{
  if(num < 0) return;
  int word = num / 32;
  int bit  = num % 32;
  if(word < 0 || word >= 4) return;
  mask[word] |= (1U << bit);
}

static int
lookup(const char *name)
{
  for(uint i = 0; i < sizeof(table)/sizeof(table[0]); i++){
    if(strcmp(table[i].name, name) == 0)
      return table[i].num;
  }
  return -1;
}

static char*
ltrim(char *s)
{
  while(*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
    s++;
  return s;
}

static void
rtrim(char *s)
{
  int n = strlen(s);
  while(n > 0){
    char c = s[n-1];
    if(c == ' ' || c == '\t' || c == '\r' || c == '\n')
      s[--n] = 0;
    else
      break;
  }
}

static void
strip_bom(char *s)
{
  if((unsigned char)s[0] == 0xEF &&
     (unsigned char)s[1] == 0xBB &&
     (unsigned char)s[2] == 0xBF){
    memmove(s, s+3, strlen(s+3)+1);
  }
}

static void
usage(void)
{
  fprintf(2, "usage: sandbox [-e|-k] <policy_file> <cmd> [args...]\n");
  fprintf(2, "  -e: deny by returning -1 (default)\n");
  fprintf(2, "  -k: deny and kill the process\n");
  exit(1);
}

int
main(int argc, char *argv[])
{
  int action = 0; // default: -e
  int i = 1;

  if(argc < 3)
    usage();

  if(strcmp(argv[i], "-e") == 0){
    action = 0;
    i++;
  } else if(strcmp(argv[i], "-k") == 0){
    action = 1;
    i++;
  }

  if(argc - i < 2)
    usage();

  char *policy = argv[i++];
  char **cmdv  = &argv[i];

  // Parse policy -> allow_mask bitmap
  uint32 mask[4] = {0};

  int fd = open(policy, O_RDONLY);
  if(fd < 0){
    fprintf(2, "sandbox: open %s failed\n", policy);
    exit(1);
  }

  char buf[512];
  char line[128];
  int linelen = 0;
  int first_line = 1;

  int n;
  while((n = read(fd, buf, sizeof(buf))) > 0){
    for(int k = 0; k < n; k++){
      char c = buf[k];
      if(c == '\n'){
        line[linelen] = 0;
        if(first_line){
          strip_bom(line);
          first_line = 0;
        }
        char *p = ltrim(line);
        rtrim(p);
        if(p[0] != 0 && p[0] != '#'){
          int num = lookup(p);
          if(num < 0){
            fprintf(2, "sandbox: unknown syscall '%s' (ignored)\n", p);
          } else {
            setbit(mask, num);
          }
        }
        linelen = 0;
      } else if(linelen < (int)sizeof(line) - 1){
        line[linelen++] = c;
      }
    }
  }
  close(fd);

  // Last line without newline
  if(linelen > 0){
    line[linelen] = 0;
    if(first_line){
      strip_bom(line);
      first_line = 0;
    }
    char *p = ltrim(line);
    rtrim(p);
    if(p[0] != 0 && p[0] != '#'){
      int num = lookup(p);
      if(num < 0){
        fprintf(2, "sandbox: unknown syscall '%s' (ignored)\n", p);
      } else {
        setbit(mask, num);
      }
    }
  }

  // Always allow these to keep the wrapper and target usable.
  setbit(mask, SYS_exec);
  setbit(mask, SYS_exit);
  setbit(mask, SYS_sandbox);

  if(sandbox(1, action, mask, 4) < 0){
    fprintf(2, "sandbox: sandbox() syscall failed\n");
    exit(1);
  }

  exec(cmdv[0], cmdv);
  fprintf(2, "sandbox: exec %s failed\n", cmdv[0]);
  exit(1);
}
