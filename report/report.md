# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|------|------|
| **实验名称** | Lab 4: 进程管理 |
| **小组成员** | 2410550杨博 2411210王禹博 2413124梁昊 |
| **完成日期** | 2026-11-19 |

### 小组分工

| 成员 | 已记录的分工 |
|------|--------------|
| 2411210-王禹博 | 功能模块一：虚拟内存页操作 |
| 2413124-梁昊 | 功能模块二：进程创建及初始化；进程创建流程分析与测试 |
| 2410550-杨博 | 功能模块三：进程调度；实验报告整理 |

---

## 一、实验目的

1. 理解 Sv39 多级页表和物理页引用计数，实现页映射的建立、更新与撤销。
2. 理解进程控制块、内核栈和内核线程的创建过程，完成 `idleproc` 与 `initproc` 的初始化。
3. 实现进程选择、页表切换和寄存器上下文切换，使多个内核线程能够轮流执行。
4. 理解中断状态保存与恢复，通过明确的提示词、代码检查及 QEMU 测试验证实现。

---

## 二、实验环境

### 基本运行环境

| 项目 | 内容 |
|------|------|
| 操作系统 | WSL 2，Ubuntu 22.04.5 LTS |
| 目标架构 | RISC-V 64 位，Sv39 分页 |
| 交叉编译器 | `riscv64-unknown-elf-gcc 15.1.0` |
| 模拟器 | QEMU 4.1.1，`virt` 机器 |
| 固件 | OpenSBI 0.4 |
| 调试工具 | `riscv64-unknown-elf-gdb` |
| 模拟物理内存 | 128 MiB |

### AI 工具

| 成员 | AI 编程工具 | 底层模型 |
|------|-------------|----------|
| 2411210-王禹博 | Codex | GPT-6 |
| 2413124-梁昊 | Claude Code（VS Code 扩展） | Claude |
| 2410550-杨博 | Codex | GPT-6 |

---

## 三、实验整体逻辑分析

### 3.1 本章节的逻辑主线

本实验围绕内核线程的创建与调度展开。首先建立和撤销虚拟地址到物理页的映射，为地址空间管理提供基础；随后创建进程控制块、内核栈和初始执行现场；最后由调度器选择可运行线程，切换页表和寄存器，使线程在 CPU 上执行。

实验启动后，`proc_init` 将当前执行现场登记为 `idleproc`，并创建 `initproc`。`cpu_idle` 发起首次调度，`schedule` 选择 `initproc`，`proc_run` 更新当前线程并切换地址空间，`switch_to` 恢复目标上下文。新线程沿 `forkret → forkrets → __trapret → sret` 恢复 trapframe，进入 `kernel_thread_entry` 并执行 `init_main`。

### 3.2 功能的逐步实现

1. **虚拟内存页操作**：实现 `page_insert` 和 `page_remove`，处理引用计数、映射替换和 TLB 刷新，为地址空间管理提供基础。
2. **进程创建与初始化**：分配并初始化 PCB 和内核栈，设置 PID、链表和初始上下文，建立 `idleproc` 和 `initproc`。
3. **进程调度与切换**：通过循环链表选择可运行线程，在关中断的临界区内更新 `current`、切换 Sv39 页表并保存和恢复上下文。
4. **集成测试**：在临时副本中注入 PMM、进程创建和调度测试，结合 QEMU 运行结果验证线程创建与执行路径。

---

## 四、实验内容与实现

### 功能模块一：虚拟内存页操作

**负责人：** 2411210-王禹博

#### 模块功能描述

**需要实现的函数：**

```c
int page_insert(pde_t *pgdir, struct Page *page,
                uintptr_t la, uint32_t perm);

void page_remove(pde_t *pgdir, uintptr_t la);
```

**功能说明：**

本模块在已有多级页表机制的基础上，实现物理页与线性地址之间映射的建立、更新和撤销。

- `page_insert` 建立或更新映射，并维护物理页的引用计数。
- `page_remove` 撤销已有映射；如果映射不存在，则直接返回。
- 页表项发生变化后，使对应的 TLB 项失效。
- 两个接口为后续地址空间管理提供基础。

实现中还使用了模块内部的 `page_remove_pte` 辅助函数，集中处理页表项清除、TLB 失效、引用计数减少及物理页释放。

#### 最终提示词

````markdown
[PROMPT]

**任务**：修改 `kern/mm/pmm.c` 中模块一对应的位置，实现虚拟内存页的映射建立、更新和撤销。

**操作要求**：

1. 必须修改项目中的实际文件，保留现有框架和其他实验模块。
2. 根据已有接口实现 `page_insert` 和 `page_remove`；可增加模块内部使用的 `static` 辅助函数。
3. 不改变公开函数签名，不实现进程创建和调度模块。
4. 可以添加简短注释，说明引用计数、失败处理和 TLB 更新等关键逻辑。

**输出要求**：以项目中的真实定义为准，完成代码修改并进行适当验证；说明实际测试结果和未完成的测试。

[RELY]

可使用项目中已有的定义和接口：

- 页表及物理页类型：`pde_t`、`pte_t`、`struct Page`。
- 页表项定位：`get_pte(pgdir, la, create)`。
- 页表项和物理页转换：`pte_create()`、`pte2page()`、`pde2page()`、`page2ppn()`。
- 引用计数：`page_ref_inc()`、`page_ref_dec()`、`set_page_ref()`。
- 物理页释放：`free_page()`。
- TLB 失效：`tlb_invalidate()`。
- 页表权限和状态位：`PTE_V`、`PTE_R`、`PTE_W`、`PTE_X`、`PTE_U`。
- 内存不足错误码：`E_NO_MEM`。

已有 `get_pte()` 在创建多级页表时可能先分配第一级中间页表，再因后续分配失败而返回空指针。实现映射失败路径时需要考虑这一点。

[GUARANTEE]

必须实现以下接口：

```c
int page_insert(pde_t *pgdir, struct Page *page,
                uintptr_t la, uint32_t perm);

void page_remove(pde_t *pgdir, uintptr_t la);
```

可以添加只在 `pmm.c` 内使用的 `static` 辅助函数，不改变公开接口。

[SPECIFICATION]

## page_insert

**Pre-Condition**：

- `pgdir` 指向有效的页表根。
- `page` 指向待映射的物理页。
- `la` 为目标线性地址，`perm` 为调用者指定的权限位。

**Post-Condition**：

- 成功时返回 `0`，目标页表项映射到 `page`，权限、引用计数和 TLB 状态保持一致。
- 无法分配所需页表时返回 `-E_NO_MEM`，不留下半完成映射；如果本次操作新建了中间页表但后续分配失败，应回收该页表并恢复原状态。

**Case 1：目标地址尚无映射**

建立映射，目标物理页引用计数增加一次。

**Case 2：目标地址已映射到同一物理页**

更新页表项权限，保持物理页引用计数不变，不能释放该页。

**Case 3：目标地址已映射到其他物理页**

撤销旧映射；旧页引用计数减为零时释放旧页。随后建立新映射，目标物理页引用计数增加一次。

**Requirements**：

- 页表项更新后使对应 TLB 项失效。
- 符合 Sv39 叶页表项的权限要求：叶项应具有读或执行权限，可写页同时具有读权限。
- 避免因重复映射同一页而错误释放物理页。

## page_remove

**Pre-Condition**：

- `pgdir` 指向有效的页表根。
- `la` 为需要撤销映射的线性地址。

**Post-Condition**：

- 有有效映射时，清除页表项并使对应 TLB 项失效；物理页引用计数减少一次，减为零时释放物理页。
- 页表项不存在或映射无效时，直接返回，不修改物理页引用计数。
- 重复删除同一映射应安全。
````

#### 实现迭代过程

##### 第一次迭代

完成映射建立与撤销的实现，并增加内部辅助函数处理旧映射。实现时区分了以下情况：

1. 原地址没有映射：增加目标页引用计数并建立映射。
2. 原地址映射到同一物理页：更新权限，引用计数保持不变。
3. 原地址映射到其他物理页：撤销旧映射，再建立新映射。
4. 创建页表失败：返回 `-E_NO_MEM`，并回收本次已创建的第一级中间页表。
5. 删除不存在的映射：直接返回。

**关键处理：**

- 删除映射时先清除页表项并使 TLB 项失效，再减少引用计数；计数为零时释放物理页。
- 对同一物理页重复映射时，只更新页表项，不改变引用计数。
- 按 Sv39 的叶页表项要求补充必要的读权限，使无读写执行权限的叶项以及只写组合不会形成无效映射。

### 功能模块二：进程创建及初始化功能

**负责人：** 2413124-梁昊

#### 模块功能描述

**需要实现/修改的函数：**

```c
static struct proc_struct *alloc_proc(void);                          // kern/process/proc.c
void proc_init(void);                                                 // kern/process/proc.c
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags); // kern/process/proc.c
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf); // kern/process/proc.c
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf); // kern/process/proc.c
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);   // kern/process/proc.c
```

**功能说明：**

本模块把内核从"只能运行一段初始化代码"推进到"能够创建、登记多个内核线程"。

- `alloc_proc` 从堆上分配一个进程控制块，初始化进程状态、上下文和两个链表节点。PCB 里的指针和状态如果没有显式初始化，后面的代码就可能读到随机值。
- `proc_init` 建立全局进程链表和 PID 哈希表，然后把当前这段执行现场直接登记成 `idleproc`，再通过 `kernel_thread` 创建第一个真正的内核线程 `initproc`。
- `kernel_thread` 只负责构造一个临时的 trapframe：把线程入口函数和参数分别放进 `s0`、`s1`，把 `epc` 指向 `kernel_thread_entry`，然后交给 `do_fork`。
- `do_fork` 负责把 PCB、内核栈、PID、链表挂接、就绪状态组合成一个可运行的线程，并处理失败时的资源回收。
- `copy_thread` 把 trapframe 和 `context` 放到新线程自己的内核栈上，使它在第一次被调度时能经 `forkret -> forkrets -> __trapret` 正确进入入口函数。
- `copy_mm` 在本实验中只是占位：当前创建的是内核线程，`current->mm` 本来就是 `NULL`，不需要复制用户地址空间。

#### 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

````markdown
[PROMPT]

**任务**：修改 `kern/process/proc.c` 中"功能模块二"对应的位置，实现进程创建及初始化功能，包括 `alloc_proc`、`proc_init`、`kernel_thread`、`do_fork`、`copy_thread` 和 `copy_mm`。

**操作要求**：

1. 必须在项目中修改实际文件，保留现有框架代码、`forkret`、`setup_kstack`、`put_kstack` 等已有函数，以及模块一和模块三的标记位置。
2. 只在 `//LAB4 function2` 标记处补全代码，不修改 `proc.h`、`trap.c`、`pmm.c` 等共享文件。
3. 不实现进程调度（`schedule`、`proc_run`、`cpu_idle`、`switch_to`）。
4. 允许添加模块内部使用的 `static` 函数和局部辅助逻辑。

**输出要求**：直接进行文件操作，以工程中的真实定义为准；完成后说明首次运行链路和失败清理路径。

[RELY]

// 数据结构与全局变量
struct proc_struct { state, pid, runs, kstack, need_resched, parent, mm,
                     context, tf, pgdir, flags, name, list_link, hash_link };
struct context { ra, sp, s0..s11 };
struct trapframe { gpr.s0, gpr.s1, gpr.a0, gpr.sp, status, epc, ... };
list_entry_t proc_list;  static list_entry_t hash_list[HASH_LIST_SIZE];
struct proc_struct *idleproc, *initproc, *current;  static int nr_process;
#define HASH_SHIFT 10   #define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define MAX_PROCESS 4096  #define PROC_NAME_LEN 15
#define KSTACKPAGE 2      #define KSTACKSIZE (KSTACKPAGE * PGSIZE)

// 可用函数与符号
kmalloc()/kfree()/memset()/memcpy()
alloc_pages()/free_pages()/page2kva()/kva2page()
setup_kstack()/put_kstack()/get_pid()/hash_proc()/find_proc()
set_proc_name()/wakeup_proc()/list_init()/list_add()
boot_pgdir_pa、bootstack
read_csr(sstatus)、SSTATUS_SPP、SSTATUS_SPIE、SSTATUS_SIE
kernel_thread_entry、forkret、forkrets
CLONE_VM、E_NO_MEM、E_NO_FREE_PROC

[GUARANTEE]

必须实现或修改的接口：

```c
static struct proc_struct *alloc_proc(void);
void proc_init(void);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
```

[SPECIFICATION]

## alloc_proc

**Pre-Condition**：
- `kmalloc` 可用，系统中还有空闲堆空间。

**Post-Condition**：
- 成功时返回一个所有字段都已显式初始化的 PCB；失败时返回 `NULL`。

**Case 1**：分配成功时，`state=PROC_UNINIT`、`pid=-1`、`runs=0`、`kstack=0`、`need_resched=0`、`parent=NULL`、`mm=NULL`、`tf=NULL`、`flags=0`、`context` 全零、`name` 清零、`pgdir=boot_pgdir_pa`，PCB 尚未挂入任何链表。

**Case 2**：`kmalloc` 返回 `NULL` 时直接返回 `NULL`，不修改任何全局状态。

**Requirements**：
- `context` 必须清零，这样 idleproc 的 `context.ra` 才会是 0，后续调度也不会误用未初始化的返回地址。

## proc_init

**Pre-Condition**：
- `pmm_init`、`vmm_init` 已完成，`bootstack`、`boot_pgdir_pa` 有效。

**Post-Condition**：
- `proc_list` 与 `hash_list` 已初始化；`idleproc` 的 pid 为 0、状态为 `PROC_RUNNABLE`、`kstack` 指向 `bootstack`；`current == idleproc`；`initproc` 存在且 pid 为 1、父进程为 `idleproc`。

**Case 1**：`idleproc` 分配失败时调用 `panic`，因为后续流程没有可运行的执行现场。

**Case 2**：`kernel_thread(init_main, "Hello world!!", 0)` 返回负值或 0 时调用 `panic`；否则用 `find_proc` 找回 PCB，命名为 `init`。

**Requirements**：
- `idleproc` 不是 fork 出来的，不挂入 `proc_list`/`hash_list`，调度器在链表为空时会显式回退到它。

## kernel_thread

**Pre-Condition**：
- `fn` 为合法的内核函数入口，`arg` 为传给它的参数。

**Post-Condition**：
- 新建线程首次运行时执行 `fn(arg)`；函数返回后经 `kernel_thread_entry` 调用 `do_exit`。
- 返回值是新线程的 pid，创建失败时为负错误码。

**Case 1**：正常创建时，`trapframe.gpr.s0` 保存函数地址、`gpr.s1` 保存参数、`epc` 指向 `kernel_thread_entry`、`status` 设置 `SPP/SPIE` 并清除 `SIE`。

**Case 2**：`do_fork` 失败时直接把错误码返回给调用者。

**Requirements**：
- `status` 必须保留 S 态（`SPP=1`）并让 `sret` 之后重新开中断（`SPIE=1`），同时避免返回前被中断打断（`SIE=0`）。

## do_fork

**Pre-Condition**：
- `current` 指向当前线程，`tf` 指向调用者提供的 trapframe 模板。

**Post-Condition**：
- 成功时返回新线程 pid，新 PCB 已挂入 `proc_list` 和 `hash_list`，状态为 `PROC_RUNNABLE`。
- 失败时返回负错误码，且不残留已分配的内核栈或 PCB。

**Case 1**：`nr_process >= MAX_PROCESS` 时返回 `-E_NO_FREE_PROC`。

**Case 2**：`alloc_proc` 或 `setup_kstack` 失败时分别返回 `-E_NO_MEM`；已分配的 PCB 用 `kfree` 回收，已分配的内核栈用 `put_kstack` 回收，顺序与分配顺序相反。

**Case 3**：`copy_mm` 失败时回收内核栈和 PCB；成功时依次执行 `copy_thread`、分配 pid、挂链表、`nr_process++`、`wakeup_proc`。

**Requirements**：
- `get_pid + hash_proc + list_add + nr_process++` 必须在关中断的临界区内完成，避免时钟中断看到只挂了一半的链表。

## copy_thread

**Pre-Condition**：
- `proc->kstack` 已指向刚分配的两个连续物理页，`tf` 是模板。

**Post-Condition**：
- `proc->tf` 指向内核栈顶部的 trapframe 副本，`gpr.a0=0`，`gpr.sp` 指向该 trapframe；`context.ra=forkret`，`context.sp=trapframe`。

**Case 1**：`esp == 0`（内核线程）时，新线程使用 trapframe 自身作为初始栈指针。

**Case 2**：`esp != 0`（以后用于用户进程）时，按调用者给出的用户栈地址设置 `gpr.sp`。

## copy_mm

**Pre-Condition**：
- `current` 是内核线程，`current->mm == NULL`。

**Post-Condition**：
- `proc->mm = NULL` 并返回 0；不复制任何用户地址空间。

**Requirements**：
- 当前阶段不做真正的 mm 复制，但函数形态要保留，方便后续实验扩展。
````

#### 实现迭代过程

##### 第一次迭代

**遇到的问题：**

- 生成的 `alloc_proc` 只初始化了状态、pid 和几个指针，没有清零 `context`，也没有设置 `pgdir`。由于 `kmalloc` 返回的内存不保证为零，`idleproc->context.ra` 可能是随机值，测试在 `assert(idleproc->context.ra == 0)` 处失败。
- `proc_init` 把 `idleproc` 当成普通线程挂进了 `proc_list`。调度器遍历链表时会把"当前执行现场"和"空闲回退线程"混在一起，链表为空时找不到 `idleproc`。

**问题解决策略：**

- 在 [SPECIFICATION] 中逐字段列出 `alloc_proc` 的初始化要求，特别写明"`context` 必须清零、`pgdir` 指向内核页表"，并要求把该函数当成"返回干净 PCB"来验收。
- 在 [RELY] 中补充说明 `idleproc` 与普通 fork 线程的区别：直接复用 `bootstack`、不入链表、调度器会显式回退到它。

##### 第二次迭代

**遇到的问题：**

- 第一版 `copy_thread` 把 `context.sp` 设成了内核栈顶，但 `context.ra` 仍然指向函数入口。新线程第一次被 `switch_to` 切进来后，直接返回到了 `copy_thread` 的调用点，而没有先恢复 trapframe，最终跑飞。
- `do_fork` 在挂链表时没有关中断，测试中偶尔出现 `hash_list` 与 `proc_list` 状态不一致。

**问题解决策略：**

- 在 [SPECIFICATION] 中把首次运行链路写清楚：`copy_thread` 只负责把 `context.ra` 设为 `forkret`、`context.sp` 设为 trapframe，剩下的 `forkret -> forkrets -> __trapret -> sret` 由已有代码完成。
- 明确要求 pid 分配、哈希挂接、链表挂接和计数更新放在 `local_intr_save/local_intr_restore` 之间。

**最终结果：**

- 编译通过，`alloc_proc`/`proc_init` 的字段全部符合测试预期。
- 注入测试输出 `[Test Process Create] PASS`；`idleproc` 的 pid、状态、内核栈、父进程、`mm`、`tf`、`context.ra` 均通过断言，`initproc` 的 pid、父进程、`context.ra/sp` 也通过断言。

#### 本模块涉及的知识点

- **进程控制块（PCB）**：`struct proc_struct` 是进程存在的唯一标志，包含状态、PID、内核栈、页表、父子关系和上下文。OS 原理中的 PCB 通常还包含文件描述符表、信号处理等字段，本实验只保留进程管理必需的部分。
- **内核栈**：每个线程独立分配 `KSTACKPAGE` 个物理页，trapframe 放在栈顶。这样可以保证线程切换后再次进入内核时，trapframe 不会互相覆盖。
- **上下文与中断帧的区别**：`context` 只保存被调用者保存寄存器和 `ra/sp`，用于主动切换；`trapframe` 保存完整的 32 个通用寄存器和 CSR，用于异常/中断返回。首次运行必须借 trapframe 才能进入 C 函数。
- **临界区**：进程链表和 PID 哈希表是全局共享结构，`do_fork` 中的挂接操作必须在关中断的情况下完成。

### 功能模块三：进程调度

**负责人：** 2410550-杨博

#### 模块功能描述

本模块负责让已经创建的内核线程获得 CPU。需要实现的接口为：

```c
void cpu_idle(void);                                      // kern/process/proc.c
void schedule(void);                                      // kern/schedule/sched.c
void proc_run(struct proc_struct *proc);                  // kern/process/proc.c
void switch_to(struct context *from, struct context *to); // kern/process/switch.S
```

`cpu_idle` 是空闲线程的执行循环；`schedule` 负责选择线程；`proc_run` 切换当前进程和页表；`switch_to` 在汇编层保存与恢复寄存器。四个函数共同完成从调度决策到目标线程继续执行的过程。

#### 实现过程与关键处理

1. **首次调度**：`proc_init` 将 `idleproc->need_resched` 设为 0，现有时钟中断处理不会设置该标志。`cpu_idle` 首次进入时主动置位，再进入检查循环，从而让已经创建的 `initproc` 获得 CPU。
2. **轮转选择**：`schedule` 先保存中断状态并关中断，清除当前线程的调度请求，再沿 `proc_list` 从当前线程之后查找 `PROC_RUNNABLE` 线程。`idleproc` 不在普通进程链表中，当前线程为空闲线程时从链表头开始。遍历一轮后仍没有可运行的普通线程，就选择 `idleproc`。选中后增加目标线程的 `runs`。
3. **切换进程与地址空间**：当目标与当前进程不同时，`proc_run` 在关中断状态下保存旧进程指针、更新 `current`，将 Sv39 的 MODE 和目标页表根物理页号写入 `satp`，执行 `sfence.vma`，再调用 `switch_to`。原线程被再次调度后，从切换点继续执行并恢复先前的中断状态。
4. **保存和恢复上下文**：`switch_to` 使用 RV64 的 `sd` 和 `ld`，按 `struct context` 中的字段顺序保存和恢复 `ra`、`sp` 与 `s0` 至 `s11`。第一次切换到新线程时，恢复的 `ra` 指向 `forkret`；线程再次运行时则回到之前调用 `switch_to` 的位置。
5. **构建与测试调整**：GCC 15 将 `kmalloc.c` 中传给 `kva2page` 的整数地址判为类型错误，修改为显式指针转换；调度测试线程的返回类型改为与 `kernel_thread` 参数一致的 `int`。在进程创建测试中加入无普通可运行进程时回退 `idleproc` 的断言。

#### 最终提示词

````markdown
[PROMPT]

**任务**：实现进程调度功能，使已经创建的内核线程能够获得 CPU 并运行。修改以下实际文件：

- `kern/process/proc.c`：实现空闲循环 `cpu_idle` 和进程切换入口 `proc_run`。
- `kern/schedule/sched.c`：实现调度选择函数 `schedule`。
- `kern/process/switch.S`：实现保存和恢复寄存器上下文的 `switch_to`。

**操作要求**：

1. 必须修改项目中的实际文件，在模块三标记位置补全实现，保留已有进程创建、内存管理和中断处理逻辑。
2. 以项目中的真实声明和数据结构为准，不改变四个公开接口的签名；可增加仅供模块内部使用的 `static` 辅助函数。
3. 调度选择与进程切换的关键步骤必须受关中断临界区保护，恢复中断时保留进入临界区之前的状态。
4. 本次只实现内核线程调度，不扩展用户进程、进程退出或抢占式时间片机制。

**输出要求**：直接修改文件并进行实际构建和测试，说明首次调度链路、调度策略、实际测试结果，以及遇到的构建或测试限制。测试应覆盖空闲线程首次调度、多个可运行线程轮转、无普通可运行线程时回退空闲线程和上下文切换。不要把未执行的测试写成通过。

[RELY]

- `kern/process/proc.h` 定义 `struct proc_struct`；本模块使用 `state`、`need_resched`、`runs`、`pgdir`、`context` 和 `list_link`，以及 `PROC_RUNNABLE`。`proc->pgdir` 保存页表根的物理地址。
- `struct context` 的字段按顺序为 `uintptr_t ra, sp, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11`。本项目为 RV64，字段均为 8 字节；汇编中的偏移和访问宽度须与实际布局一致。
- `proc_list` 是带头结点的双向循环链表；`list_next` 和 `le2proc(le, list_link)` 可用于遍历。`current` 指向当前线程，`idleproc` 是空闲线程，初始化时不挂入 `proc_list`。
- `local_intr_save(intr_flag)` 保存原中断状态并关中断，`local_intr_restore(intr_flag)` 按保存的状态恢复；可以嵌套使用。
- `write_csr(satp, value)`、`SATP_MODE_SV39`、`RISCV_PGSHIFT`、`flush_tlb()` 和 `boot_pgdir_pa` 可用于页表切换。Sv39 的 MODE 位于 RV64 `satp` 的高 4 位，参考 `kern/mm/pmm.c` 中 `enable_paging()`；`libs/riscv.h` 的 `lsatp()` 使用 Sv32 格式，本实验不可调用。
- 模块二的 `copy_thread` 已准备首次运行所需的 `context` 和 trapframe：`context.ra` 指向 `forkret`，首次切换后经 `forkret -> forkrets -> __trapret` 恢复执行现场并进入线程入口。
- 当前 `proc_init` 将 `idleproc->need_resched` 初始化为 0；现有时钟中断处理不会设置该标志。进入空闲循环时需要主动触发首次调度，确保 `initproc` 能够运行。
- `tools/test_lab4.c` 和 `tools/inject.py` 提供 PMM、进程创建和调度测试；`tools/grade.sh` 检查测试标记与 `initproc` 输出。

[GUARANTEE]

必须实现以下接口：

```c
void cpu_idle(void);                                      // kern/process/proc.c
void schedule(void);                                      // kern/schedule/sched.c
void proc_run(struct proc_struct *proc);                  // kern/process/proc.c
void switch_to(struct context *from, struct context *to); // kern/process/switch.S
```

保留 `cpu_idle` 已声明的 `noreturn` 属性；汇编符号 `switch_to` 须能由 C 代码链接调用。

[SPECIFICATION]

## cpu_idle

**Pre-Condition**：进程表和空闲线程已初始化，`current == idleproc`；内核已完成时钟和中断初始化。

**Post-Condition**：函数持续运行且不返回；当空闲线程需要调度时调用 `schedule`，让其他可运行线程获得 CPU。

**Case 1：首次进入空闲循环**：主动产生一次调度请求，使已经创建的 `initproc` 可以运行，即使当前没有任何时钟中断设置 `need_resched`。

**Case 2：空闲线程重新获得 CPU**：继续检查调度标志；有请求时调度，无请求时继续等待。

**Requirements**：不要依赖现有时钟中断自动请求重调度，也不要无条件关闭中断后忙等。

## schedule

**Pre-Condition**：`current`、`idleproc` 和 `proc_list` 已初始化，调用线程允许进入调度过程。

**Post-Condition**：清除发起调度的当前线程的 `need_resched`；按循环顺序选择一个 `PROC_RUNNABLE` 线程，增加被选线程的 `runs`，并通过 `proc_run` 完成必要的切换。

**Case 1：当前线程为普通线程**：从其链表节点的下一个节点开始循环检查，跳过头结点和不可运行的线程，最多遍历一轮；若其他线程均不可运行但当前线程仍可运行，可以再次选择当前线程。

**Case 2：当前线程为 `idleproc`**：从 `proc_list` 的第一个普通进程开始检查，不能把未加入链表的 `idleproc->list_link` 当作遍历起点。

**Case 3：没有可运行的普通线程**：选择 `idleproc` 作为回退线程；链表为空时也必须安全处理。

**Requirements**：选择过程、标志与计数更新以及调用 `proc_run` 的关键路径处于关中断保护下；当被切走的线程以后恢复执行时，正确恢复其进入本次调度前的中断状态。不能将链表头解释为进程节点，也不能产生无限遍历。

## proc_run

**Pre-Condition**：`proc` 指向有效的可运行线程，`current` 指向当前线程，目标页表根物理地址有效。

**Post-Condition**：若目标线程与当前线程不同，保存旧线程的执行上下文并切换到目标线程；目标运行期间 `current`、活动页表和实际执行现场相互对应。被切走的线程以后恢复时，从其原来的切换点继续执行，并恢复其原有中断状态。

**Case 1：`proc == current`**：直接返回，不重复切换页表或上下文。

**Case 2：`proc != current`**：在关中断的临界区保存旧线程指针、更新 `current`，按目标 `pgdir` 设置 Sv39 `satp`、刷新 TLB，并调用 `switch_to` 切换上下文。

**Requirements**：Sv39 MODE 和页表物理页号应使用足够宽的无符号类型组合；保持切换步骤的原子性，按保存的状态恢复中断。内核线程使用 `boot_pgdir_pa` 对应的内核页表；不得使用 Sv32 的 `lsatp()`。

## switch_to

**Pre-Condition**：`from` 和 `to` 分别指向有效的旧、目标 `struct context`，两个上下文在切换期间保持有效。

**Post-Condition**：保存旧线程的 `ra`、`sp` 和 `s0` 至 `s11`，恢复目标线程的对应寄存器，再从恢复的 `ra` 继续执行；旧线程以后恢复时从调用后的下一条指令继续。

**Case 1：目标线程首次运行**：恢复模块二准备的 `ra` 和 `sp`，进入既有的 `forkret -> forkrets -> __trapret` 链路。

**Case 2：目标线程曾被切走**：恢复其保存的现场，继续之前暂停的执行。

**Requirements**：按 RV64 ABI 使用正确的寄存器宽度与结构体偏移；只负责上下文保存和恢复，不选择线程、不修改 `current`、不切换页表。
````

#### 验证结果

完整评分中的 `check PMM`、`check Process Create` 和 `check Schedule` 均为 `OK`，总分 30/30。调度测试创建两个内核线程并多次主动切换，确认两个线程均得到执行；额外断言验证了空闲线程回退。普通 QEMU 启动输出了 `initproc` 的 PID、名称及问候信息。

---

### 练习：进程创建流程分析

**负责人：** 2413124-梁昊

结合本模块的实现，`kernel_thread` 创建一个内核线程的完整流程如下：

1. `kernel_thread` 在栈上准备一个全零的 `trapframe`，把入口函数和参数放进 `s0`、`s1`，把 `epc` 指向 `kernel_thread_entry`，并把 `sstatus` 的 `SPP/SPIE` 置位、`SIE` 清零。
2. `do_fork` 调用 `alloc_proc` 得到 PCB，把 `parent` 设为当前线程，再调用 `setup_kstack` 分配两个连续物理页作为内核栈。
3. `copy_mm` 在当前阶段直接返回 0，因为内核线程不拥有用户地址空间。
4. `copy_thread` 把 trapframe 复制到新内核栈的栈顶，设置 `a0=0` 表示子线程返回 0，并把 `context.ra` 设为 `forkret`、`context.sp` 设为 trapframe 地址。
5. 在关中断的临界区内分配 PID、挂入 `proc_list` 和 `hash_list`、增加 `nr_process`。
6. `wakeup_proc` 把状态改为 `PROC_RUNNABLE`，`do_fork` 返回新线程的 PID。
7. 第一次被调度时，`switch_to` 恢复 `context.ra/sp`，跳到 `forkret`；`forkret` 调用 `forkrets(current->tf)`，最终经 `__trapret` 执行 `sret`，从 `epc` 即 `kernel_thread_entry` 开始运行，装入 `s0/s1` 后跳转到入口函数。

---

### 扩展练习（Challenge）

**负责人：** 2413124-梁昊

#### 1. `local_intr_save` / `local_intr_restore` 如何实现开关中断

`local_intr_save(x)` 展开为 `x = __intr_save()`。`__intr_save` 先读取 `sstatus` 的 `SIE` 位：如果中断本来是开的，就调用 `intr_disable()`（用 `csrrc` 清零 `SIE`）并返回 1；如果本来是关的，就什么都不做并返回 0。`local_intr_restore(x)` 只有在 `x` 为 1 时才重新调用 `intr_enable()`。

这样做的好处是保存了"进入临界区之前中断处于什么状态"，而不是无条件开中断。如果一段代码在已经关中断的上下文中被调用（例如 trap 处理过程中），`restore` 不会错误地把中断打开。这也是它比直接 `intr_disable(); ...; intr_enable();` 更安全的原因。

#### 2. `get_pte` 中两段相似代码与 sv32/sv39/sv48 的关系

RISC-V 的分页是把虚拟地址按位段切开、逐级索引页表：sv32 是两级、每级 10 位；sv39 是三级、每级 9 位；sv48 是四级、每级 9 位。除了层数和页表项宽度，每一级的动作完全一样：用当前级的索引取出页表项，如果无效就分配下一级页表页并把页表项填好，然后以页表项指向的物理页作为下一级的基址继续往下走。

所以 `get_pte` 里出现两段形式相同的代码，本质上是同一套"查表—缺页时分配—下降一级"的逻辑重复了一次。把它们写成循环，循环次数就是页表级数（sv39 下是 2 次中间级遍历加最后一级叶项）。之所以看起来像复制粘贴，是因为这个内核只支持一种分页模式，直接把级数展开写死了，省掉循环变量和模式判断。

以 4 KiB 页为例，页内偏移均占 12 位。Sv32 的虚拟地址分为两段各 10 位的页号索引和 12 位偏移，每个页表项占 4 字节；Sv39 和 Sv48 的页表项占 8 字节，每级索引占 9 位，分别使用三级和四级页表。处理器从 `satp` 指定的根页表开始逐级查找，遇到叶页表项后结合页内偏移形成物理地址，并检查有效位及访问权限；较高层的叶项还可映射大页，物理地址和虚拟地址须满足相应的对齐要求。Sv39 的 64 位虚拟地址高位必须是第 38 位的符号扩展，Sv48 则要求高位是第 47 位的符号扩展。

本实验在 RV64 下使用 Sv39：`satp` 的 MODE 字段为 8，页表根物理地址右移 12 位得到物理页号。`proc_run` 写入目标线程的 `satp` 后执行 `sfence.vma`，避免继续使用旧地址空间的 TLB 项。

#### 3. 是否有必要把页表项查找和页表页分配合并

现阶段不建议拆开。`get_pte(pgdir, la, create)` 的调用者要么在建映射时需要分配，要么在查映射时不需要分配，`create` 参数已经能区分这两种语义，合并实现的代码量也小。当前 `get_pte` 在第二级页表分配失败时不会自行回收先前新建的中间页表；`page_insert` 在调用失败后负责回滚这部分资源。

但合并确实带来两个问题：一是"查找"这个只读操作被迫携带分配语义，调用者必须小心传 0；二是分配策略（用哪个分配器、失败如何回滚）被固定在查找函数里，后续如果要支持按需分页或不同的页表分配策略，就得改这个函数本身。如果这个内核继续扩展，拆成 `get_pte`（只查找）和 `alloc_pte`（按需创建）会更清晰；在本实验的规模下没有必要。

---

## 五、测试与验证

### 5.1 构建和运行

在 `code` 目录执行 `make -j4`，RISC-V 内核和 `bin/ucore.img` 构建成功。通过 QEMU 的 `virt` 机器加载镜像后，内核完成物理内存、SLUB、虚拟内存和进程初始化，随后进入 `initproc`。实际输出包括：

```text
check_alloc_page() succeeded!
check_pgdir() succeeded!
check_boot_pgdir() succeeded!
check_vma_struct() succeeded!
check_vmm() succeeded.
++ setup timer interrupts
this initproc, pid = 1, name = "init"
To U: "Hello world!!".
To U: "en.., Bye, Bye. :)"
```

`init_main` 返回后通过 `kernel_thread_entry` 进入现有的 `do_exit`，当前框架在该函数中输出 `process exit` panic。上述输出验证了从 `idleproc` 到 `initproc` 的首次调度及线程入口执行。

### 5.2 评分测试

评分脚本在临时副本中通过 `tools/inject.py` 注入测试，再构建内核并使用 QEMU 和 GDB 检查运行输出。各项检查覆盖的内容如下：

| 测试项 | 验证内容 | 结果 |
|--------|----------|------|
| PMM | 页映射建立、替换、重复映射、引用计数、删除，以及页表分配失败时的回滚 | OK |
| Process Create | `idleproc` 和 `initproc` 的状态、父进程、内核栈及上下文，空闲线程链表节点初始化；无普通可运行线程时回退空闲线程 | OK |
| Schedule | 两个内核线程在多次主动调度中均获得执行，跨切换恢复中断状态，`initproc` 输出预期内容 | OK |

完整评分的实际结果：

```text
  -check PMM:                                OK
  -check Process Create:                     OK
  -check Schedule:                           OK
Total Score: 30/30
```

### 5.3 已保存的测试截图

下图记录模块二阶段的 QEMU 输出，包含 PMM 和进程创建测试：

![模块二阶段 QEMU 输出](./images/lab4_qemu.png)

下图记录模块二阶段的测试输出：

![模块二阶段测试输出](./images/lab4_grade.png)

---

## 六、实验总结与收获

### 对操作系统的理解

1. **页表与物理页管理**：页表负责把虚拟地址映射到物理页，引用计数记录物理页被映射的次数。替换或删除映射时，需要同步处理页表项、引用计数和 TLB，避免保留失效的地址转换。
2. **PCB 和内核栈**：PCB 保存线程状态、PID、页表根和切换上下文；每个内核线程拥有独立内核栈，供 trapframe 和内核调用使用。`idleproc` 直接使用启动栈，普通线程通过 `do_fork` 分配内核栈。
3. **进程调度与上下文切换**：调度器按状态选择执行对象，`proc_run` 维护当前线程和地址空间，`switch_to` 保存并恢复调用约定要求保留的寄存器。首次运行通过 trapframe 恢复入口，之后的切换则回到上次暂停的位置。
4. **中断与临界区**：进程链表、`current` 和页表切换需要保持一致。使用 `local_intr_save` 和 `local_intr_restore` 能保护关键步骤，并在嵌套调用时保留进入临界区前的中断状态。
5. **与 OS 原理的对应**：实验实现了最基本的线程创建和轮转调度；教材中更完整的进程系统还包括用户态进程加载、独立用户地址空间复制、抢占式时间片、进程退出及回收、文件描述符和进程间通信。本实验的 `copy_mm` 仅处理内核线程，`do_exit` 仍是触发 panic 的框架实现。

### AI 协作开发的经验

明确项目中的结构体、函数接口和调用顺序，可以让提示词直接约束生成代码。例如，调度模块需要写清 `idleproc` 不在普通链表中、`satp` 使用 Sv39 格式，以及首次切换必须恢复模块二准备的 trapframe。这些条件决定了代码能否与已有框架衔接。

编译和运行结果用于检查提示词与实现：GCC 的类型错误暴露了接口参数不匹配；首次调度的检查揭示空闲线程需要主动设置调度标志；QEMU 和注入测试进一步验证页映射、进程初始化以及多个线程的切换。将这些观察写回提示词和报告，能够让后续实现和复查更准确。
