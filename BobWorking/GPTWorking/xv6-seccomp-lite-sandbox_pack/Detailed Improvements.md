# 改进详解：为 Xv6-K210 增加「进程系统调用沙箱（seccomp-lite）」——低成本做出“安全感爆棚”的突破

> 一句话：让每个进程都能声明“我只允许这些系统调用”，其余全部拦截。  
> 效果：演示时一眼就懂、立刻震撼；实现时不需要大改内核结构，适合大三 OS 入门阶段落地。

这个改进借鉴了 Linux 的 **seccomp** 思路（过滤/限制系统调用；fork/exec 继承过滤器；可选择 kill 或返回错误）。citeturn0search1  
在 xv6-k210 体系里，我们做一个“微缩版 seccomp”（所以叫 *seccomp-lite*）：用 **位图 allowlist** 表达允许的系统调用集合。

---

## 1. 你实现了什么“新功能”

### 1.1 新增系统调用：`sandbox(on, action, allow_mask, words)`
- `on = 1`：开启沙箱
- `on = 0`：关闭沙箱并清空策略
- `action = 0`：拒绝时返回 `-1`（演示更直观：程序报 “Permission denied”）
- `action = 1`：拒绝时 kill 进程（更“硬核”）
- `allow_mask`：用户态传入的白名单位图（uint32 数组）
- `words`：位图长度（0..4，对应最多 128 个系统调用位）

### 1.2 新增用户态命令：`sandbox`
用法：
```sh
sandbox [-e|-k] <policy_file> <cmd> [args...]
```

策略文件格式（极简、容易讲清楚）：
- 每行一个 syscall 名称（如 `open` / `read` / `write`）
- `#` 开头为注释
- 空行忽略

为了避免“自断后路”，wrapper 会自动放行：`exec / exit / sandbox`（否则 sandbox 自己 exec 目标程序会被拦住）。

---

## 2. 设计选择：为什么它“亮眼”但不复杂

### 2.1 为什么这是“突破性”
在课程项目里，大家常做：
- COW、MLFQ、mmap、缓存优化（偏“性能/功能”）
而 **“进程级安全边界（syscall gate）”** 是另一个叙事维度：
- 你能讲“最小权限原则”
- 你能演示“同一程序在不同策略下行为完全不同”
- 你能对标工业界（Linux seccomp / 容器沙箱）

### 2.2 为什么实现成本低
拦截系统调用最合适的位置就是 `kernel/syscall.c` 的 **统一分发器**：这里已经拿到了 syscall number（a7），也能直接改返回值（a0）。  
xv6-k210 自己的文档也明确了“系统调用号 + syscalls 表 + user.h/usys.pl”这一套新增流程。citeturn0search2

---

## 3. 内核实现：三块拼图

### 3.1 数据结构：给 `struct proc` 加三项
文件：`kernel/include/proc.h`

新增字段：
- `int sandbox_on;`
- `int sandbox_action;`
- `uint32 allow_mask[4];`

含义：进程“随身携带”自己的白名单策略。

### 3.2 拦截点：在 `syscall()` 里做 deny/kill
文件：`kernel/syscall.c`

核心逻辑（口头讲解版）：
1) 如果没开沙箱：正常走  
2) 如果开了：  
   - 计算 syscall 对应的位（word/bit）
   - 如果白名单里没有：
     - action=0：返回 -1
     - action=1：标记 killed，然后返回 -1（trap 返回后会 exit）

> 这与经典 xv6 “trace(mask)” 的设计哲学是同一条线：都是对 syscall 分发阶段做轻量改造，只是 trace 是“观察”，sandbox 是“拦截”。citeturn0search7

### 3.3 新系统调用：`sys_sandbox()`（放到独立文件，降低耦合）
新增文件：`kernel/sandbox.c`

做三件事：
1) 从 a0-a3 取参数（`argint/argaddr`）
2) 从用户态拷贝 allow_mask（`either_copyin`）
3) 写入当前进程 `p->sandbox_*` 字段

---

## 4. 用户态实现：sandbox 命令 + 三份策略文件

### 4.1 `xv6-user/sandbox.c`
- 读策略文件
- 把每行 syscall 名字映射到编号
- 置位到 allow_mask
- 调用 `sandbox(1, action, mask, 4)`
- `exec(cmd, argv)`

### 4.2 策略文件（放到 `/etc`，结构更“现代”）
- `etc/policy_cat.txt`：允许 `cat` 类程序
- `etc/policy_nofile.txt`：故意不允许 `open`（用于演示“被拒绝”）
- `etc/policy_sh.txt`：更宽松，能跑 shell/常见命令

Makefile 已包含把 `etc/policy*.txt` 拷贝进镜像的逻辑（/etc 目录你需要自己建）。

---

## 5. 你需要做哪些改动（可直接照做）

你将拿到一个压缩包（本次回复会提供下载链接），里面包含：
- 完整替换文件：`sysnum.h / syscall.c / proc.h / user.h / usys.pl / Makefile`
- 新增文件：`kernel/sandbox.c`、`xv6-user/sandbox.c`、`etc/policy_*.txt`
- 关键补丁：`kernel/proc.c.sandbox.patch`
- 快速说明：`README_SANDBOX.md`

### 5.1 `kernel/proc.c` 的 3 处插入（必须做）
1) `allocproc()`：初始化 sandbox 字段  
2) `freeproc()`：清空 sandbox 字段（防止进程复用泄露策略）  
3) `fork()`：拷贝 sandbox 字段（保证子进程也被限制，符合 seccomp 行为）citeturn0search1

---

## 6. 运行与验收演示（建议剧本）

编译运行（QEMU）：
```sh
make clean
make fs
make run
```

进 xv6 shell 后演示：
```sh
# 1) 正常：能读
cat README

# 2) 开沙箱：仍然能读（policy_cat 放行 open/read/write）
sandbox /etc/policy_cat.txt cat README

# 3) 开沙箱：打不开文件（policy_nofile 不放行 open）
sandbox /etc/policy_nofile.txt cat README

# 4) 更硬核：直接 kill（-k）
sandbox -k /etc/policy_nofile.txt cat README
```

讲解要点（答辩加分）：
- “最小权限原则”：策略文件就是权限声明
- “可观测性”：配合 trace 可打印被拦的 syscall
- “继承语义”：fork/exec 继承限制（与 Linux 一致）citeturn0search1

---

## 7. 你还能加一个小而美的加分点（可选）

**拒绝日志**：当 action=1 kill 时，在 kernel 里额外打印一行：
- pid、syscall 名、拒绝原因
这样现场演示“更像真实系统”。

这一步只涉及 `kernel/syscall.c` 的一行 `printf`，但观感升级很明显。

---

## 8. 风险与边界（实话实说）
- 这是 allowlist 位图，不支持复杂条件（如“只允许 open 某个路径”），但足够课程展示
- 若未在 `fork()` 复制 sandbox 字段，子进程会绕过限制——所以 proc.c 的补丁是关键
- wrapper 自动放行 exec/exit/sandbox：是“工程上更不坑自己”的选择（否则你会卡在 exec 这一步）

---

## 9. 文件清单（你最终应看到）
- kernel/include/sysnum.h
- kernel/include/proc.h
- kernel/syscall.c
- kernel/sandbox.c  ✅新增
- xv6-user/user.h
- xv6-user/usys.pl
- xv6-user/sandbox.c ✅新增
- etc/policy_cat.txt ✅新增
- etc/policy_nofile.txt ✅新增
- etc/policy_sh.txt ✅新增
- kernel/proc.c（手工插入 3 处）

