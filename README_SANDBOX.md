# xv6-sandbox

This patch adds a "seccomp-lite" sandbox mechanism to xv6-k210.

## Features

1.  **New Syscall**: `int sandbox(int on, int action, uint32 *allow_mask, int words)`
    *   `on`: 1 to enable, 0 to disable (needs refining for security, but allowed for demo).
    *   `action`: 0 to deny by returning -1, 1 to deny by killing the process.
    *   `allow_mask`: A bitmap of allowed system calls.
2.  **Kernel Enforcement**:
    *   `syscall()` dispatch checks the mask if sandbox is enabled.
    *   `proc` struct augmented with `sandbox_on`, `sandbox_action`, and `allow_mask`.
    *   `fork()` inherits sandbox state.
3.  **User Tool**: `_sandbox`
    *   Usage: `sandbox [-e|-k] <policy_file> <cmd> [args...]`
    *   Parses policy file (syscall names) and sets up the mask.
4.  **Policy Files**:
    *   `/etc/policy_cat.txt`: Minimal read-only.
    *   `/etc/policy_nofile.txt`: No file open allowed.
    *   `/etc/policy_sh.txt`: For running a shell.

## Usage Example

```bash
# Run cat with read-only policy
$ sandbox /etc/policy_cat.txt cat README

# Try to run cat with no-open policy (will fail)
$ sandbox /etc/policy_nofile.txt cat README
cat: cannot open README

# Run a sandboxed shell
$ sandbox /etc/policy_sh.txt sh
$ ls
.              1 1 1024
..             1 1 1024
README         2 2 2227
...
$ open README
(might fail if 'open' is not in policy, but sh needs open for redirection etc. see policy_sh.txt)
```

## Build

`make fs` will compile `_sandbox` and copy `etc/policy_*.txt` into `fs.img`.

