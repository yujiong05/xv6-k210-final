# 改进详解：为 Xv6-K210 增加「进程系统调用沙箱（seccomp-lite）」——小成本做出“安全感爆棚”的突破

> 适用对象：刚学完操作系统的大三同学  
> 目标：功能“眼前一亮”、实现难度可控、改动点集中、可在 QEMU 或 K210 上跑通  
> 核心卖点：**把“系统调用”从“功能入口”升级为“安全边界”**——像 Linux 的 seccomp 一样，给进程装一个轻量沙箱，让它只能调用被允许的系统调用。

---

## 1. 你要做的突破是什么？

### 1.1 一句话描述
新增一个系统调用 `sandbox()`，让进程能够安装一份 **系统调用白名单**（allowlist）。之后该进程（以及可选地它 fork/exec 出来的后代）只能执行白名单里的系统调用；否则要么返回错误，要么直接 kill。

这相当于 xv6 里“微缩版 seccomp”：Linux 的 seccomp 用 BPF 过滤器做复杂判断；我们用 bitmask 白名单做“可实现 + 效果显著”的教学版。

### 1.2 为什么它吸引人（而且不复杂）
- **吸引人**：大部分同学做的都是性能/功能（调度、COW、mmap）。你做的是“安全边界”——看起来更“现代 OS”。
- **可控**：改动集中在 `proc` 结构体 + `syscall.c` 分发器 + 一个新 syscall + 一个用户态命令。
- **效果显著**：能演示真实场景：限制不可信程序、最小权限运行命令、减少内核暴露面（attack surface）。

---

## 2. 设计目标与范围（先立规矩，避免失控）

### 2.1 明确的范围（本期必做）
1. **内核支持**
   - 每个进程保存一份 `syscall_allow_mask`（bitmask）。
   - syscall 分发前做检查：不允许就拒绝（返回 -1）或 kill。
2. **用户态支持**
   - 新增 `sandbox` 命令：读取策略文件（syscall 名称列表）→ 生成 bitmask → 调用 `sandbox()` 安装 → `exec()` 运行目标程序。

### 2.2 可选加分项（做完必做再加）
- 违规 syscall 打印更友好：`pid/name/syscall`。
- 继承策略：默认 `fork` 继承；`exec` 保留（更像真正沙箱）。
- “学习模式”：先放行并记录用到的 syscall，自动生成最小白名单（这块稍微复杂，建议后做）。

---

## 3. 数据结构与关键逻辑

### 3.1 进程结构体新增字段（proc）
新增三个字段即可：

```c
// 以 SYS_MAX 为上限，按 64bit 分组
#define SYSCALL_MASK_WORDS ((SYS_MAX + 64) / 64)

int sandbox_on;              // 0=关闭, 1=开启
int sandbox_action;          // 0=KILL, 1=ERRNO(-1)
uint64 syscall_allow[SYSCALL_MASK_WORDS];  // 白名单 bitmask
```

> 说明：`SYS_MAX` 代表你系统里最大的 syscall 编号（下面教你怎么取）。  
> bitmask 的好处：检查 O(1)，空间也小（几十个 syscall → 几个 uint64）。

### 3.2 syscall 检查函数（内核内联）
```c
static inline int
syscall_allowed(struct proc *p, int num)
{
  if(!p->sandbox_on) return 1;
  if(num < 0 || num > SYS_MAX) return 0;
  int idx = num / 64;
  int off = num % 64;
  return (p->syscall_allow[idx] >> off) & 1;
}
```

### 3.3 syscall 分发器插桩（“安全边界”就这里）
在 `kernel/syscall.c` 的 `syscall()` 里，拿到 `num` 后，调用真正 handler 前做检查：

- 允许：正常执行
- 不允许：
  - action=ERRNO：直接返回 -1
  - action=KILL：打印并把进程标记 `killed=1`

---

## 4. 代码改动清单（按文件走，你照着改就能跑）

> 注意：不同 xv6-k210 分支目录名可能略有不同。以下以常见结构为准：`kernel/` 与 `xv6-user/`。

### 4.1 内核：新增 syscall 编号（SYS_sandbox）
文件：`kernel/include/syscall.h`（或类似位置）
```c
#define SYS_sandbox  23   // 示例编号：找一个没用的编号填进去
```

同时在用户态也要同步（见 4.4）。

### 4.2 内核：声明与实现 sys_sandbox
1) 在 `kernel/include/defs.h` 增加声明：
```c
uint64 sys_sandbox(void);
```

2) 在 `kernel/sysproc.c`（或你的 syscalls 实现在 `syscall`/`sysproc` 的文件）添加实现：

```c
#include "types.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"
#include "param.h"

#define SANDBOX_OFF   0
#define SANDBOX_ON    1

#define SANDBOX_ACT_KILL  0
#define SANDBOX_ACT_ERRNO 1

uint64
sys_sandbox(void)
{
  int on, action;
  uint64 uptr;

  if(argint(0, &on) < 0) return -1;
  if(argint(1, &action) < 0) return -1;
  if(argaddr(2, &uptr) < 0) return -1;

  struct proc *p = myproc();

  if(on == SANDBOX_OFF){
    p->sandbox_on = 0;
    return 0;
  }

  if(action != SANDBOX_ACT_KILL && action != SANDBOX_ACT_ERRNO)
    return -1;

  // 如果 uptr==0：表示“全部允许”，方便调试
  if(uptr == 0){
    memset(p->syscall_allow, 0xFF, sizeof(p->syscall_allow));
  } else {
    if(copyin(p->pagetable, (char*)p->syscall_allow, uptr, sizeof(p->syscall_allow)) < 0)
      return -1;
  }

  p->sandbox_action = action;
  p->sandbox_on = 1;
  return 0;
}
```

### 4.3 内核：在 syscall 分发器里做拦截
文件：`kernel/syscall.c`

1) 添加 `syscall_allowed()`（放在文件顶部合适位置）
2) 在 `syscall()` 内部加入：

```c
void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;

  // ====== 这里是你新加的沙箱拦截 ======
  if(!syscall_allowed(p, num)){
    if(p->sandbox_action == SANDBOX_ACT_ERRNO){
      p->trapframe->a0 = -1;
      return;
    } else {
      printf("[sandbox] pid=%d blocked syscall=%d\n", p->pid, num);
      p->killed = 1;
      p->trapframe->a0 = -1;
      return;
    }
  }
  // ===================================

  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    p->trapframe->a0 = syscalls[num]();
  } else {
    printf("%d %s: unknown sys call %d\n",
           p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

### 4.4 内核：把 sys_sandbox 挂到 syscalls 表
仍在 `kernel/syscall.c`：

```c
extern uint64 sys_sandbox(void);

static uint64 (*syscalls[])(void) = {
  ...
  [SYS_sandbox]  sys_sandbox,
};
```

---

## 5. 用户态：把新 syscall 暴露出来（关键三步）

### 5.1 用户头文件声明
文件：`xv6-user/user.h`
```c
int sandbox(int on, int action, const uint64 *mask);
```

### 5.2 用户 syscall 编号同步
文件：`xv6-user/include/syscall.h`（或 `user/syscall.h`）
```c
#define SYS_sandbox  23
```

### 5.3 生成 syscall 桩代码
文件：`xv6-user/usys.pl`
在列表里加：
```perl
entry("sandbox");
```

然后 `make` 会自动生成 `usys.S`。

---

## 6. 用户态：新增 sandbox 命令（策略文件 + exec）

### 6.1 策略文件格式（简单到离谱）
每行一个 syscall 名称，可用 `#` 注释：

```text
# policy_min.txt
read
write
open
close
fstat
exit
```

### 6.2 sandbox.c（核心逻辑）
文件：`xv6-user/sandbox.c`

要点：
1) 读 policy 文件
2) 把 syscall 名称映射成编号（用一个表）
3) 置位 bitmask
4) 调用 `sandbox(SANDBOX_ON, action, mask)`
5) `exec(argv[...])`

> syscall 名称到编号的映射表：你可以在 `kernel/syscall.h` 里把编号写成宏，然后在 user 侧复制一份（教学系统常用做法）。

---

## 7. 编译、打包、运行（QEMU 和 K210 都给你）

### 7.1 工具链与 QEMU（宿主机准备）
- 安装 RISC-V GCC 交叉工具链（riscv64-unknown-elf 或 riscv64-unknown-linux-gnu）
- 安装 `qemu-system-riscv64`
- 进入项目目录执行：`make build`

> 如果你用的是官方 xv6-k210 工作流：`make fs` 生成 `fs.img`，再 `make run platform=qemu` 启动。  
> 如果上板：`make sdcard dst=<挂载点>` 写入 SD，再 `make run`。

### 7.2 把 sandbox 程序加入 UPROGS
文件：`Makefile`
在 `UPROGS = \` 里加入：
```make
  $U/_sandbox \
```

### 7.3 在系统里放入 policy 文件
- QEMU：把 policy 文件放进 `fs.img` 对应的 `/bin/`（项目通常有拷贝规则，你可以照着其它文件加）
- K210：`make sdcard dst=...` 会把 `/bin` 目录的用户程序拷进 SD 卡（按你项目规则来）

---

## 8. 验收演示（你报告里最亮眼的部分）

### 8.1 演示 1：最小权限运行 cat
```sh
$ sandbox -e /bin/policy_cat.txt cat README
# 正常输出
```

### 8.2 演示 2：禁止 open，直接“掐死”程序
```sh
$ sandbox -k /bin/policy_noopen.txt cat README
[sandbox] pid=... blocked syscall=...
# 进程被 kill
```

### 8.3 你写报告时怎么讲（建议话术）
- “我们把系统调用当成安全边界，对进程安装 syscall allowlist，缩小内核暴露面。”
- “这与 Linux seccomp 的目标一致，但实现更轻量，适合教学系统。”
- “成本：几处改动；收益：可控执行、可审计、可扩展。”

---

## 9. 风险点与排障清单（别踩坑）

1. **SYS 编号冲突**：确保 `SYS_sandbox` 没占用。
2. **mask 大小不一致**：内核 `sizeof(p->syscall_allow)` 与 user 侧数组长度必须一致。
3. **exec 是否保留策略**：默认 `proc` 结构体不被 exec 重置 → 策略能保留；如果你项目 exec 做了清理，需要你特意保留这几个字段。
4. **不小心把 exit 禁了**：程序会变成“僵尸”；建议默认策略一定包含 `exit` / `wait` 等。

---

## 10. 下一步怎么把它做得更“像 Linux”（可选扩展）

- **学习模式**：记录实际用过的 syscall → 自动生成最小 allowlist（像 seccomp 的 `LOG` 行为）。
- **按 syscall 参数过滤**：例如允许 open 但只允许只读（这会复杂很多，别一上来就做）。
- **沙箱可继承 + 不可解除**：增强安全属性（Linux seccomp 安装后通常不可撤销）。

---

## 11. 你交付物可以长这样（建议目录）
```text
docs/
  sandbox.md            # 你的设计文档（可直接用本文改写）
xv6-user/
  sandbox.c
  policy_cat.txt
kernel/
  syscall.c
  sysproc.c
  include/syscall.h
```

---

## 12. 最后：你需要我补全哪些信息才能“百分百对齐你的工程”？

把下面问题的答案发我，我就能把上面的代码片段“对齐到你的真实文件名/编号/接口”，给你做一份**精确到行号的 patch 指南**：

1) 你这个分支里 syscall 编号定义文件路径是哪个？（例如 `kernel/include/syscall.h`）  
2) 你的 `syscall()` 分发器在哪个文件？（通常 `kernel/syscall.c`）  
3) 你的 `struct proc` 定义在哪？（`kernel/proc.h` 或 `kernel/include/proc.h`）  
4) 你目前总共有多少 syscall（`syscalls[]` 表有多大）？  
5) 你是在 QEMU 还是 K210 上验收？（两者流程略不同）  
6) 你希望违规处理是 “kill” 还是 “返回 -1” 默认？  
7) 你们课程报告更看重：安全性、性能、还是可观测性？

答完这些，我可以把方案改成“完全贴合你的工程”，你照着一步步做即可。
