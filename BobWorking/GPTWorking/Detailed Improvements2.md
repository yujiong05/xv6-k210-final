# 改进详解.md（对齐你的工程：sysnum.h + QEMU + 已有 trace/strace）

> 改进主题：**进程系统调用沙箱（seccomp-lite）**  
> 目标：在 xv6-k210 上做一个“眼前一亮但不难”的突破性新功能：**把系统调用变成安全边界**。  
> 运行平台：**QEMU（默认 platform := qemu）**；编译环境：**Windows + WSL**（你已确认）。  
> 你已有基础：系统已有 `trace(mask)` 与 `xv6-user/strace.c`，可复用“按 syscall 号做 bitmask 过滤”的成熟套路（MIT 6.S081 官方实验也采用 mask 方式）。  

---

## 0. 你将交付什么？

1) 新系统调用：`sandbox(int on, int action, const uint64 *allow_mask)`  
2) 进程级沙箱：进程安装 **syscall allowlist（白名单 bitmask）**  
3) 用户态工具：`sandbox` 命令，读取策略文件（每行一个 syscall 名称），安装沙箱后 `exec` 运行目标程序  
4) 可演示的“故事线”：
   - 正常运行：`cat secret.txt` 成功
   - 沙箱运行：禁止 `open` 后 `sandbox cat secret.txt` 报 `Permission denied`（返回 -1），程序仍活着但“啥也干不了”
   - 或升级为 kill 模式：触发禁止 syscall 直接干掉进程（更像 seccomp 的 KILL）

**为什么这算“突破”**：Linux 的 seccomp 就是通过过滤系统调用来缩小攻击面（attack surface），降低内核暴露面。你实现的是教学版 allowlist，但理念和价值一致。  
参考：Linux 内核文档对 seccomp 过滤的目标与动作（KILL/ERRNO/LOG 等）说明。  

---

## 1. 设计规格（明确边界，避免把项目做炸）

### 1.1 核心行为
- 每个进程维护：
  - `sandbox_on`：是否启用
  - `sandbox_action`：违规处理策略（默认建议 ERRNO，更好演示）
  - `sandbox_allow[]`：白名单 bitmask（按 syscall 号置位）
- 每次系统调用分发前检查：
  - 在 allowlist：正常执行
  - 不在 allowlist：
    - `ERRNO`：直接返回 `-1`（用户态可打印 Permission denied）
    - `KILL`：打印一次日志并标记 `killed=1`

### 1.2 继承语义（与 trace 保持一致，减少心智成本）
- `fork()`：子进程继承父进程的沙箱设置（与 MIT trace 实验要求一致：mask 应被继承）  
- `exec()`：默认保留沙箱设置（更像真实世界“进程身份不变、执行映像变”）

> 这两条能让你讲“最小权限运行命令”的故事：父进程装沙箱 → exec 运行目标程序 → 目标程序被限制。

### 1.3 syscall 编号
你当前 syscall 连续：`SYS_fork=1 ... SYS_munmap=41`  
因此新增：
- `#define SYS_sandbox 42`
- `SYS_MAX` 取 42（你可以加一个宏，后面计算 bitmask 长度会更稳）

---

## 2. 数据结构（proc 里加 3 个字段就够了）

### 2.1 选择 bitmask 的理由
- O(1) 检查（常数极小）
- 空间小（42 个 syscall 只需要 1 个 u64；为了对齐，做成数组更通用）
- 与你已有 `trace(mask)` 机制统一

### 2.2 建议的定义（kernel/proc.h）
```c
// kernel/proc.h

// 你新增一个统一最大 syscall 号，避免到处写 42
#define SYS_MAX SYS_sandbox

#define SYSCALL_MASK_WORDS ((SYS_MAX + 64) / 64)

#define SANDBOX_ACT_KILL   0
#define SANDBOX_ACT_ERRNO  1

struct proc {
  ...
  int sandbox_on;
  int sandbox_action;
  uint64 sandbox_allow[SYSCALL_MASK_WORDS];
  ...
};
```

---

## 3. 内核改动（按文件逐一落地）

> 你工程里 syscall 号定义文件：`kernel/include/sysnum.h`  
> syscall 分发器：`kernel/syscall.c`  
> 你已有 trace/strace：建议沿用其写法与风格。

### 3.1 `kernel/include/sysnum.h`：增加 syscall 号
```c
// kernel/include/sysnum.h
...
#define SYS_munmap 41
#define SYS_sandbox 42
```

> 如果你有 `sysnames[]`（用于 strace 输出），也要加 `"sandbox"`（见 3.4）。

### 3.2 `kernel/proc.h`：新增字段（见上节）
做完后：确保 proc 初始化时这些字段为 0（通常 `allocproc()` 会清零或显式初始化）。

### 3.3 `kernel/proc.c`：fork 继承沙箱设置
你已经有 trace_mask 继承逻辑的话，照着抄一份（MIT trace 实验也强调 fork 要继承 mask）。  
在 `fork()` 成功创建子进程后，加入：

```c
// kernel/proc.c fork() 中，copy 父进程到子进程的位置附近
np->sandbox_on = p->sandbox_on;
np->sandbox_action = p->sandbox_action;
memmove(np->sandbox_allow, p->sandbox_allow, sizeof(p->sandbox_allow));
```

### 3.4 `kernel/syscall.c`：分发前拦截（核心“安全边界”）
#### 3.4.1 增加检查函数
```c
static inline int
sandbox_allowed(struct proc *p, int num)
{
  if(!p->sandbox_on) return 1;
  if(num <= 0 || num > SYS_MAX) return 0;
  int idx = num / 64;
  int off = num % 64;
  return (p->sandbox_allow[idx] >> off) & 1ULL;
}
```

#### 3.4.2 在 `syscall()` 里插桩
找到 `num = p->trapframe->a7;` 之后、真正调用 handler 之前，插入：

```c
if(!sandbox_allowed(p, num)){
  if(p->sandbox_action == SANDBOX_ACT_ERRNO){
    p->trapframe->a0 = -1;
    return;
  } else {
    printf("[sandbox] pid=%d (%s) blocked syscall=%d\n", p->pid, p->name, num);
    p->killed = 1;
    p->trapframe->a0 = -1;
    return;
  }
}
```

#### 3.4.3 挂接 sys_sandbox 到 syscall 表
在 `syscall.c` 中：
- `extern uint64 sys_sandbox(void);`
- `syscalls[SYS_sandbox] = sys_sandbox;`（或你项目的数组初始化方式）

#### 3.4.4（如果有）补全 sysnames
你说你只看到 `sysnames`，说明你工程可能像 MIT trace 一样维护了 syscall 名称数组。  
在相应位置加上：
```c
[SYS_sandbox] "sandbox",
```

### 3.5 `kernel/sysproc.c`（或你放进程相关 syscall 的文件）：实现 `sys_sandbox()`
#### 3.5.1 内核侧接口
- `on=0`：关闭沙箱
- `on=1`：安装 allowlist
- `action`：`SANDBOX_ACT_ERRNO` 或 `SANDBOX_ACT_KILL`
- `allow_mask`：指向用户态数组（长度 = `sizeof(p->sandbox_allow)`）

#### 3.5.2 参考实现
```c
// kernel/sysproc.c (或等价文件)

#include "types.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

uint64
sys_sandbox(void)
{
  int on, action;
  uint64 uptr;
  struct proc *p = myproc();

  if(argint(0, &on) < 0) return -1;
  if(argint(1, &action) < 0) return -1;
  if(argaddr(2, &uptr) < 0) return -1;

  if(on == 0){
    p->sandbox_on = 0;
    return 0;
  }

  if(action != SANDBOX_ACT_KILL && action != SANDBOX_ACT_ERRNO)
    return -1;

  if(uptr == 0){
    // 允许全部 syscall：便于调试
    memset(p->sandbox_allow, 0xFF, sizeof(p->sandbox_allow));
  } else {
    if(copyin(p->pagetable, (char*)p->sandbox_allow, uptr, sizeof(p->sandbox_allow)) < 0)
      return -1;
  }

  p->sandbox_action = action;
  p->sandbox_on = 1;
  return 0;
}
```

---

## 4. 用户态改动（新 syscall + 新命令）

你项目的添加 syscall 步骤可参考 xv6-k210 官方文档（user.h/usys.pl/编号/内核表）。  

### 4.1 用户态声明：`xv6-user/user.h`
```c
int sandbox(int on, int action, const uint64 *mask);
```

### 4.2 syscall stub：`xv6-user/usys.pl`
加入：
```perl
entry("sandbox");
```

### 4.3 syscall 号同步
你 syscall 号在 `kernel/include/sysnum.h`。用户态需要能拿到 `SYS_sandbox`。两种做法：

**推荐做法（少拷贝，最稳）**：让用户态编译时 include 到 `kernel/include/`  
- 如果你的 user 编译命令已经 `-Ikernel/include`，则直接在 `sandbox.c` 里：
  ```c
  #include "sysnum.h"
  ```
- 若没有 include 路径，则新增一个 `xv6-user/sysnum_user.h`，只放 syscall 号宏（复制一份即可）。

### 4.4 新增用户程序：`xv6-user/sandbox.c`
功能：读取策略文件 → 解析 syscall 名 → 生成 bitmask → 安装沙箱 → exec 目标程序。

**策略文件格式**（建议放 `/etc/`，更像现代系统）：
- 每行一个 syscall 名称
- `#` 开头注释
示例 `/etc/policy_cat.txt`：
```text
# minimal policy for cat
open
read
write
close
fstat
exit
```

**实现要点**：
- 维护一个 `name -> sysnum` 的映射表（41 项，拷一次就完事；之后基本不动）
- 支持 `-e`（ERRNO）与 `-k`（KILL）两种模式
- 让 `sandbox` 的参数形如：
  ```sh
  sandbox -e /etc/policy_cat.txt cat README
  ```

---

## 5. Makefile 与文件系统镜像（让它“编译即带上”）

### 5.1 把 sandbox 程序加入 UPROGS
在 `Makefile` 的 `UPROGS` 中加入：
```make
$U/_sandbox \
```

### 5.2 把策略文件打进 fs 镜像
不同 xv6-k210 变体做法略有差异：你按项目里“README / init / 现有配置文件”那套复制规则加一条即可。

推荐路径：`/etc/policy_*.txt`  
- 报告里可以强调：**引入 /etc 管理配置**，可观测性与工程化更强。

---

## 6. 运行与验收（QEMU）

### 6.1 编译与启动（WSL）
典型流程（按你项目的 make 目标为准）：
```sh
make clean
make fs
make run
```
若你需要显式指定：
```sh
make run platform=qemu
```

### 6.2 演示脚本（你答辩时直接照念）
1) 证明正常：
```sh
cat /secret.txt
```
2) 开沙箱（ERRNO）：
```sh
sandbox -e /etc/policy_noopen.txt cat /secret.txt
# 预期：open 失败，cat 输出 “Permission denied / open failed”
```
3) 开沙箱（KILL）：
```sh
sandbox -k /etc/policy_noopen.txt cat /secret.txt
# 预期：内核打印 [sandbox] ... blocked syscall=...
```

---

## 7. 测试点与评分点（写进报告就很像“工程化”）

### 7.1 功能正确性
- allowlist 覆盖的 syscall 能正常运行
- 未覆盖 syscall：
  - ERRNO：返回 -1
  - KILL：进程退出（被标记 killed）
- fork/exec 继承性验证：
  - `sandbox sh` 后在 shell 里跑 `cat`/`ls`，行为符合策略

### 7.2 性能影响（轻量但能量化）
- syscall 分发增加一次 bitmask 检查（常数级）
- 你可以用一个循环调用 `getpid()` 的小程序对比开/关沙箱耗时（粗测即可）

### 7.3 安全叙事（加分点）
- “减少暴露面”：进程只暴露必要 syscall  
- “最小权限原则”：策略文件可审计，可复用  
- 对标 seccomp 行为：ERRNO/KILL/LOG（你实现 ERRNO/KILL，LOG 作为可选扩展）

---

## 8. 可选扩展（做完核心再做，仍然不复杂）

### 8.1 学习模式（LOG）：自动生成最小白名单
新增 action=LOG：
- 不拦截，只记录进程实际用到的 syscall（置位 `used_mask`）
- 进程退出时打印出建议策略（或提供 `sandbox_dump()` syscall 读取 used_mask）
对标 Linux seccomp 的 `SECCOMP_RET_LOG` 行为（内核文档有说明）。  

---

## 9. 常见坑位（提前排雷）
1) **mask 长度不一致**：内核 `sizeof(p->sandbox_allow)` 与 user 侧数组长度必须一致  
2) **把 exit 禁了**：程序可能“挂住”；策略里建议默认包含 `exit`/`wait`（视程序而定）  
3) **编号更新遗漏**：`sysnum.h`、`syscalls[]`、`sysnames[]`、user stub、UPROGS 缺一不可  
4) **fork 继承忘了拷贝**：会出现“父进程限制了，子进程没限制”的违和行为（trace 实验同类坑）  

---

## 10. 参考（你报告里可引用的“权威出处”）
- MIT 6.S081 Lab: system calls（trace 使用 mask 的官方说明，强调继承）。  
- xv6-k210 官方文档：如何在该工程中添加新 syscall（user.h/usys.pl/编号/内核表）。  
- Linux Kernel 文档：seccomp 过滤的动机（减少暴露面）与动作（KILL/ERRNO/LOG 等）。  

