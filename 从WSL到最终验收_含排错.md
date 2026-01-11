# xv6-k210 seccomp-lite 沙箱：从 WSL 启动到最终验收（含常见报错处理）

本文给出一套**可复现、可验收**的完整流程：从在 Windows 打开 WSL 开始，到在 QEMU 里跑完沙箱验证；并整理你遇到过的典型报错与修复办法。

> 关键原理支撑  
> - xv6 RISC-V：系统调用号来自 trapframe 保存的 `a7`，返回值写回 `a0`。citeturn0search1turn0search13  
> - 6.S081 trace：`mask` 的第 N 位控制跟踪 syscall N，例如 `trace(1 << SYS_fork)`。citeturn0search4turn0search0  
> - `dos2unix`：用于把 Windows 文本（CRLF/BOM）转换成 Unix 格式，避免解析乱码。citeturn0search14turn0search6  
> - `mkfs.fat/mkfs.vfat`：用于在设备或镜像文件中创建 FAT 文件系统。citeturn0search3turn0search15  

---

## 0. 两套环境（先把“在哪敲命令”搞清楚）

你实际上在切换两台“机器”：

### A) WSL（宿主机 / 编译机）
用途：编译、打包 `fs.img`、修改 `etc/policy_*.txt`、查 `kernel/include/sysnum.h`。  
提示符通常像：`platinum@...:/mnt/d/...$`

### B) xv6（客体机 / 运行机）
用途：运行 `/bin/sandbox`、`cat/ls/mkdir/rm/strace` 做验收。  
提示符像：`-> / $`

> 不要在 xv6 里运行 `riscv64-unknown-elf-readelf`、`grep kernel/include/sysnum.h` 这类“源码/主机工具”；它们只存在于 WSL 的工程目录里，不在 xv6 的文件系统镜像里。

---

## 1. 一次性准备（WSL 端）

### 1.1 打开 WSL 并进入仓库根目录
在 PowerShell：
```powershell
wsl
```

在 WSL：
```bash
cd /mnt/d/Coding/ClionCode/Xv6_K210_Finally
```

### 1.2 安装依赖（只需做一次）
```bash
sudo apt update
sudo apt install -y build-essential make perl git \
  qemu-system-misc gdb-multiarch \
  dosfstools dos2unix \
  gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf
```

验证工具链是否可用：
```bash
which riscv64-unknown-elf-gcc
riscv64-unknown-elf-gcc --version
```

---

## 2. 打包并启动 xv6（WSL 端）

### 2.1 统一策略文件格式（强烈建议每次验收前做）
```bash
dos2unix etc/policy_*.txt
```
用途：避免 UTF-8 BOM / CRLF 导致 `sandbox` 解析出奇怪字符（你之前的 `��#` 就是这类问题）。citeturn0search14turn0search6

### 2.2 重建文件系统镜像并运行
```bash
rm -f fs.img
make fs
make run
```

`make fs` 本质：创建并格式化 FAT 镜像（`mkfs.fat/mkfs.vfat`），再把用户程序复制到镜像的 `/` 和 `/bin`，把策略文件复制到镜像 `/etc`。citeturn0search3turn0search15

**建议截图（图 1）：**WSL 端 `make fs` 输出（能看到 “making fs image...”/mkfs 输出）。

---

## 3. 进入 xv6 后：环境检查（xv6 端）

进入 `-> / $` 后依次执行：

```sh
ls /bin
ls /etc
```

预期：
- `/bin` 里存在 `sandbox`、`strace` 等
- `/etc` 里存在你的策略文件（如 `policy_cat.txt / policy_nofile.txt / policy_sh.txt ...`）

**建议截图（图 2）：**`ls /etc` 输出（能证明策略文件已打包进镜像）。

---

## 4. 完整验收脚本（xv6 端）

> 建议统一用绝对路径（`/bin/sandbox`），避免 PATH 差异引起误判。

### 4.1 用例 A：允许读文件（放行 open/read）
```sh
/bin/sandbox -e /etc/policy_cat.txt cat README
```
预期：能打印 README 内容。

**建议截图（图 3）：**命令 + README 输出开头几行。

### 4.2 用例 B：禁止 open（ERRNO 与 KILL）

ERRNO（可视化最强）：
```sh
/bin/sandbox -e /etc/policy_nofile.txt cat README
```
预期：`cat` 无法打开 README（表现为报错或直接无内容输出，取决于 `cat` 实现）。

KILL（更强制）：
```sh
/bin/sandbox -k /etc/policy_nofile.txt cat README
```
预期：进程被快速终止，常见表现是“无输出直接回提示符”。

**建议截图（图 4）：**`-e` 模式下的失败现象。

### 4.3 用例 C：禁止 remove + 用 strace 给“铁证”

#### C1) 先在 WSL 算出 mask（只需算一次）
在 WSL：
```bash
grep -n "SYS_remove" kernel/include/sysnum.h
```
假设输出 `#define SYS_remove 17`，那么：
- mask = `1 << 17` = `131072`  
这与 6.S081 trace 的 mask 语义一致。citeturn0search4turn0search0

#### C2) xv6 里执行“禁止 remove”的可观测策略
确保你镜像里存在：
- `/etc/policy_sh_noremove_trace.txt`（特点：**不含 remove，但包含 trace**）

在 xv6：
```sh
/bin/sandbox -e /etc/policy_sh_noremove_trace.txt sh
mkdir t
/bin/strace 131072 rm t
ls
```

预期（验收关键点）：
- `strace` 输出类似：`remove -> -1`
- `rm: t failed to delete`
- `ls` 里仍然能看到 `t`

为什么可信：xv6 的 `syscall()` 从 trapframe 的 `a7` 取 syscall 号，并把返回值写回 `a0`，所以 `-1` 会稳定反映到用户态；`strace/trace` 按 mask 位控制输出。citeturn0search1turn0search13turn0search4

**建议截图（图 5，最重要）：**同屏包含 `remove -> -1`、`rm` 失败提示、`ls` 仍显示 `t`。

---

## 5. 创建缺失策略文件（WSL 端）——用于修复 “open policy failed”

如果 xv6 里 `ls /etc` 看不到某个策略文件，就在 WSL 创建它，重建 `fs.img`。

例如创建 `/etc/policy_sh_noremove_trace.txt`：

```bash
cat > etc/policy_sh_noremove_trace.txt <<'EOF'
# sh policy: no remove, but allow trace
fork
exec
wait
exit
pipe
read
write
close
open
fstat
dup
chdir
getpid
sbrk
sleep
uptime
getcwd
readdir
mkdir
rename
trace
EOF

dos2unix etc/policy_sh_noremove_trace.txt
rm -f fs.img
make fs
make run
```

---

## 6. 常用命令速查（实验中会反复用）

### WSL / Linux 端（主机）
- `ls`：列目录内容  
- `cd PATH`：切换目录  
- `cat FILE`：打印文件内容  
- `grep -n "PATTERN" FILE`：搜索字符串并显示行号（查 `SYS_remove` 用）  
- `rm -f FILE`：删除文件（`-f` 强制）  
- `dos2unix FILE`：把 CRLF/BOM 等转换为 Unix 文本格式 citeturn0search14turn0search6  
- `make fs / make run`：重建并运行（将策略/程序打入镜像）

### xv6 端（客体）
- `ls /etc`：确认策略文件已进入镜像  
- `/bin/sandbox ...`：用策略运行程序（你的改进点）  
- `/bin/strace MASK cmd`：观察某类 syscall 的返回值（证据链）  
- `mkdir t`：创建目录（为删除测试准备目标）  
- `rm t`：触发 `remove`（用于验证被拒绝/放行）

---

## 7. 常见报错与处理（对照你遇到过的现象）

### 7.1 `riscv64-unknown-elf-gcc: No such file or directory`
**原因**：你在 PowerShell/Windows 环境编译，或者 WSL 没装交叉编译器。  
**处理**：在 WSL 安装 `gcc-riscv64-unknown-elf`，并在 WSL 中执行 `make ...`。

---

### 7.2 xv6：`exec sandbox failed`
**原因 1**：shell 不做 PATH 搜索，`sandbox` 在 `/bin`。  
**处理**：用绝对路径：`/bin/sandbox ...`

**原因 2**：镜像没刷新，`sandbox` 没打进 fs.img。  
**处理**：WSL 执行 `rm -f fs.img && make fs && make run`，再在 xv6 `ls /bin` 确认。

---

### 7.3 `sandbox: unknown syscall '��#' (ignored)`
**原因**：策略文件带 Windows CRLF 或 UTF-8 BOM，导致行首出现乱码。  
**处理**：在 WSL 对策略文件执行 `dos2unix`，或用 `cat > file <<'EOF' ... EOF` 重写后再 `make fs`。citeturn0search14turn0search6

---

### 7.4 `sandbox: open /etc/xxx.txt failed`
**原因 1（最常见）**：文件不在镜像 `/etc` 里。  
**处理**：xv6 里 `ls /etc` 确认；若没有则在 WSL 创建 `etc/policy_*.txt` 并 `make fs` 重建。

**原因 2**：你在“已禁 open 的沙箱 shell”里再次运行 `sandbox`，导致它连策略文件都打不开。  
**处理**：先 `exit` 回到外层 sh，再运行 `/bin/sandbox ...`。

---

### 7.5 xv6：`exec # failed` 或 `rm: # failed to delete`
**原因**：你把带 `#` 的说明/注释粘贴进 xv6 shell；该 sh 不一定支持注释，会把 `#` 当命令或参数。  
**处理**：在 xv6 里只输入“纯命令”，不要带注释行或在命令行尾加 `# ...`。

---

### 7.6 `/bin/strace: strace failed`
**原因**：当前沙箱策略没有允许 `trace` syscall；`strace` 程序先调用 `trace(mask)`，失败就会报错。6.S081 trace 的语义本质就是通过 `trace(mask)` 开启跟踪。 cite turn0search4 turn0search0 
**处理**：在该策略里加入一行 `trace`，重建镜像后再测。

---

### 7.7 `pid X: remove -> 0` 但你以为“应该被拒绝”
**原因**：沙箱没有成功启动（例如策略文件打开失败），所以 `rm` 在“普通环境”执行，`remove` 返回 0 表示成功。  
**处理**：先解决 `sandbox: open ... failed`，并用 `ls /etc` 确认策略文件存在；成功进入沙箱后再运行 `strace rm t`，应看到 `remove -> -1`。

---

## 8. 建议你最终提交/答辩要放的截图清单

1) WSL：`make fs` 输出（证明重建并打包）  
2) xv6：`ls /etc`（证明策略文件在镜像中）  
3) 用例 A：`/bin/sandbox -e ... cat README` 成功输出  
4) 用例 B：`/bin/sandbox -e ... cat README` 失败现象  
5) 用例 C：`/bin/strace 131072 rm t` 输出 `remove -> -1` + `ls` 仍有 `t`（最关键）

---

## 参考资料
- xv6-riscv book（系统调用：`a7` 取号，返回写入 `a0`）：citeturn0search1turn0search13  
- MIT 6.S081 / trace(mask) 语义：citeturn0search4turn0search0  
- dos2unix（CRLF/BOM 处理）：citeturn0search14turn0search6  
- mkfs.fat / mkfs.vfat（创建 FAT 文件系统镜像）：citeturn0search3turn0search15  
