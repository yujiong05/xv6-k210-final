#ifndef __SIGNAL_H
#define __SIGNAL_H

#include "types.h"

// Signal definitions
#define SIGHUP    1   // Hangup detected on controlling terminal
#define SIGINT    2   // Interrupt from keyboard
#define SIGQUIT   3   // Quit from keyboard
#define SIGILL    4   // Illegal Instruction
#define SIGTRAP   5   // Trace/breakpoint trap
#define SIGABRT   6   // Abort signal from abort(3)
#define SIGFPE    7   // Floating point exception
#define SIGKILL   9   // Kill signal
#define SIGUSR1   10  // User-defined signal 1
#define SIGSEGV   11  // Invalid memory reference
#define SIGUSR2   12  // User-defined signal 2
#define SIGPIPE   13  // Broken pipe
#define SIGALRM   14  // Timer signal from alarm(2)
#define SIGTERM   15  // Termination signal
#define SIGSTOP   19  // Stop process
#define SIGTSTP   20  // Stop typed at terminal
#define SIGCONT   18  // Continue if stopped
#define SIGCHLD   17  // Child stopped or terminated
#define SIGWINCH  28  // Window resize signal

#define SIG_DFL  ((void (*)(int))0)   // Default signal handling
#define SIG_IGN  ((void (*)(int))1)   // Ignore signal
#define SIG_ERR  ((void (*)(int))(-1))// Signal error return value

// Number of signals supported (signal numbers 1-31, typically)
#define NSIG 32

#endif
