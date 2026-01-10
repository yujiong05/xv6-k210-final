# Sandbox Patch Pack (xv6-k210)

This folder contains the complete code you need to add a "seccomp-lite" syscall sandbox.

## What to copy/replace

Replace these files in your repo with the ones in this folder:

- kernel/include/sysnum.h
- kernel/syscall.c
- kernel/include/proc.h
- xv6-user/user.h
- xv6-user/usys.pl
- Makefile

Add these NEW files/directories:

- kernel/sandbox.c
- xv6-user/sandbox.c
- etc/policy_cat.txt
- etc/policy_nofile.txt
- etc/policy_sh.txt

## One required edit (kernel/proc.c)

Because proc.c is large and project-specific, this pack ships a patch snippet:

- kernel/proc.c.sandbox.patch

Apply it manually (recommended), or try `patch -p1 < kernel/proc.c.sandbox.patch` if your file matches.

You must do 3 insertions:
1) allocproc(): initialize sandbox_on/action/allow_mask
2) freeproc(): clear sandbox state
3) fork(): copy sandbox state to child

## Build & run (QEMU)

From your repo root:

```sh
make clean
make fs
make run
```

## Demo

Inside xv6 shell:

```sh
# allowed
sandbox /etc/policy_cat.txt cat README

# denied (open is not allowed)
sandbox /etc/policy_nofile.txt cat README

# kill mode demo
sandbox -k /etc/policy_nofile.txt cat README
```
