# 功能模块一
````
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

# 功能模块二

````
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
- `proc_list` 与 `hash_list` 已初始化；`idleproc` 的 pid 为 0、状态为 `PROC_RUNNABLE`、`kstack` 指向 `bootstack`、`need_resched` 置 1；`current == idleproc`；`initproc` 存在且 pid 为 1、父进程为 `idleproc`。

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

# 功能模块三

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
- 编写此提示词时，`proc_init` 将 `idleproc->need_resched` 初始化为 0；现有时钟中断处理不会设置该标志。进入空闲循环时需要主动触发首次调度，确保 `initproc` 能够运行。
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

