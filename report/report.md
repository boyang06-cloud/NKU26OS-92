# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|------|------|
| **实验名称** | Lab 2: 物理内存和页表 |
| **小组成员** | 2410550-杨博 2411210-王禹博 2413124-梁昊 |
| **完成日期** | 2026.10.23 |

### 小组分工


| 成员 | 负责的练习/模块 |
|------|----------------|
| 2411210-王禹博 | 模块一：物理内存初始化功能 |
| 2410550-杨博   | [练习或模块名称] |
| 2413124-梁昊   | 模块二：Best-Fit 物理页分配；任务三：Buddy System 提示词设计 |

实验报告如何分工？

---

## 一、实验目的

<!-- 说明本次实验的主要目标和预期成果 -->

本实验的主要目的是：

1. [目的1：例如，理解用户态进程如何通过系统调用进入内核]
2. [目的2：例如，掌握进程创建、执行、退出的完整生命周期]
3. [目的3：例如，学习使用 AI 辅助开发操作系统核心功能]

---

## 二、实验环境

你们使用的 AI 工具

| 成员 | AI 编程工具 | 底层模型 | 备注 |
|------|------------|---------|------|
| 2411210-王禹博 | Codex | GPT-6.1 sol|
| 2410550-杨博 | | |
| 2413124-梁昊 | Claude Code（VS Code 扩展） | Claude | |


## 三、实验整体逻辑分析

### 2.1 本章节的逻辑主线

[请用自己的话描述本章实验的核心主线，例如：
- 本章围绕哪个核心主题展开？（例如：用户进程管理、虚拟内存、文件系统等）
- 整个实验解决什么问题？（例如：如何让用户程序运行在操作系统之上）]

### 2.2 功能的逐步实现

[说明本章实验是按照什么顺序逐步实现各个功能的，以及为什么要按这个顺序，例如：

1. 首先实现**功能A** → 为什么首先实现这个？（例如：建立系统调用接口是后续所有用户态功能的基础）
2. 接着再完善**功能B** → 为什么接着实现这个？（例如：有了系统调用后，需要能够加载和启动第一个用户进程）
3. 然后完成**功能C** → 为什么然后实现这个？（例如：有了第一个进程后，需要实现进程复制功能）
4. 最后补充了**功能D** → 为什么最后实现这个？（例如：最后完善进程退出和清理机制，形成完整闭环）]

---

## 四、实验内容与实现


### 功能模块一：物理内存探测与页描述符初始化

**负责人：** 2411210-王禹博

#### 模块功能描述

本模块负责获取内核可管理的物理内存范围，并初始化物理页描述符。启动时，OpenSBI/启动环境通过启动参数传递设备树（DTB）的地址；入口代码保存该地址，`dtb_init()` 解析 DTB 中的内存信息，供后续内存管理初始化使用。`page_init()` 根据物理内存基址和大小、内核结束位置以及 `struct Page` 描述符数组的占用范围，计算物理页号范围，初始化页描述符，并将未被内核及描述符数组占用的完整物理页交给现有 PMM 接口管理。

**涉及的函数/接口：**

```c
void dtb_init(void);
uint64_t get_memory_base(void);
uint64_t get_memory_size(void);
static void page_init(void);
void pmm_init(void);
```

其中 `dtb_init()`、`get_memory_base()`、`get_memory_size()` 和 `pmm_init()` 由实验框架提供；本模块主要实现 `kern/mm/pmm.c` 中的 `page_init()`，并结合 DTB 接口完成物理内存信息的获取。

#### 最终提示词

以下提示词用于根据当前实验源码实现并审查 `page_init()`。整理后的提示词应在提交前与实际使用记录核对。

```text
我正在完成 riscv64-ucore 操作系统实验 Lab2，负责模块一：物理内存探测与页描述符初始化。请以当前项目源码为准，不要套用其他版本 uCore 的实现。

请阅读 kern/driver/dtb.c、kern/driver/dtb.h、kern/init/entry.S、kern/init/init.c、kern/mm/pmm.c、kern/mm/pmm.h、kern/mm/memlayout.h、kern/mm/mmu.h，以及相关测试代码。

当前 pmm.c 中的 page_init() 需要实现。请遵循项目已有接口和地址布局约定，完成以下工作：
1. 按当前框架设置 va_pa_offset = PHYSICAL_MEMORY_OFFSET。
2. 通过 get_memory_base() 和 get_memory_size() 获取 DTB 解析出的物理内存基址和大小，并检查无效值及地址加法溢出。
3. 遵循 DRAM_BASE、nbase、PPN、PGSIZE 的定义，计算 npage 和 Page 描述符数量。
4. 将 pages 数组放在链接器符号 end 之后，并按页对齐。
5. 初始化相关 struct Page 的 ref、flags、property，并在可用性确认前将页面标记为保留。
6. 计算 Page 描述符数组结束位置对应的物理地址，按页向上对齐，确定可用完整物理页的起点。
7. 不得将内核镜像、Page 描述符数组或其他已占用区域加入空闲页区。
8. 使用现有 PADDR()、pa2page() 和 init_memmap() 接口，不要重写其他组员负责的分配算法。
9. 解释每个关键变量、地址单位、边界条件和当前实现依赖的内存布局假设。
10. 提供代码检查、单文件编译、完整构建和运行验证的步骤，并明确区分这些验证的结论。

如果源码和上述假设不一致，请先指出差异，不要修改框架测试或其他模块。
```

#### 实现迭代过程

##### 第一次迭代：实现 `page_init()`

**实现内容：**

- 在 `kern/mm/pmm.c` 中对应的 `//lab2 function1` 位置编写 `page_init()`。
- 获取 DTB 解析得到的物理内存基址与大小，并加入参数有效性和地址溢出检查。
- 设置地址转换偏移，计算 `npage`，在内核结束符号 `end` 之后按页对齐放置 `pages` 描述符数组。
- 初始化描述符并标记为保留，计算描述符数组之后的空闲页范围，调用 `init_memmap()`。

**验证情况：**

- 已执行 `make obj/kern/mm/pmm.o`，终端显示 `+ cc kern/mm/pmm.c` 且没有出现编译错误；这说明 `pmm.c` 对应的目标文件单独编译通过。
- 执行 `make qemu` 时，完整构建在 `kern/mm/best_fit_pmm.c` 处因 `free_list` 和 `nr_free` 未声明而停止。错误发生在 QEMU 启动之前，目前尚不能据此判断 `page_init()` 的运行时正确性。
- 完整 QEMU 启动、模块一运行测试和 `make grade` 的结果均待补充。

**问题与处理策略：**

1. **完整构建被其他模块阻塞：** 当前 `pmm.c` 选择 `default_pmm_manager`，但构建过程仍会编译 `best_fit_pmm.c`。应由负责 Best-Fit 的组员检查该模块，不能为了绕过错误而修改框架中的 `basic_check()` 或 `best_fit_check()`。
2. **运行时边界尚待验证：** 单文件编译不能验证 DTB 数据、实际内存布局、描述符数组范围以及 `init_memmap()` 的运行行为。完整构建恢复后，需要运行 QEMU 并检查实际日志和测试结果。

**关键注意事项：**

- 区分虚拟地址、物理地址、页号和字节数，地址转换遵循当前框架的 `va_pa_offset` 约定。
- 内核镜像和 `Page` 描述符数组占用的物理页不得被加入空闲页区。
- 当前实现依赖课程框架中的 DRAM 起始地址和映射约定；若运行环境不同，应重新核对这些假设。

---

### 扩展任务 Challenge：硬件的可用物理内存范围的获取方法

**负责人：** 2411210-王禹博

**题目：** 如果 OS 无法提前知道当前硬件的可用物理内存范围，请问你有什么办法让 OS 获取可用物理内存范围？

**回答：**

操作系统不应简单地假设所有机器都具有相同的物理内存起始地址和容量。比较通用的做法是让启动固件或引导程序在启动时提供物理内存布局信息，内核再解析这些信息，确定哪些物理地址范围属于 RAM、哪些区域被固件或设备保留。

常见方法包括：

1. **通过设备树（DTB）获取内存信息。** 在本实验的 RISC-V uCore 启动流程中，启动环境传入 DTB 地址，内核保存该地址并由 `dtb_init()` 解析设备树中的内存节点，进而通过 `get_memory_base()` 和 `get_memory_size()` 获取物理内存基址与大小。内核随后还需要排除内核镜像、页描述符数组及其他保留区域，才能得到真正可分配的物理页范围。
2. **通过固件提供的内存映射接口获取。** 例如，在支持 UEFI 的平台上，操作系统加载阶段可以从 UEFI Boot Services 获取内存映射；在传统 PC 启动环境中，引导程序也可以查询 BIOS 提供的内存映射信息，并将结果传给内核。操作系统根据映射中的类型和范围识别可用内存及保留区域。
3. **由引导程序探测后传递内存布局。** 如果内核本身不直接解析固件接口，也可以让 bootloader 负责查询平台内存信息，再通过约定的启动参数或数据结构把内存区域列表交给内核。
4. **读取平台相关的硬件寄存器或固件表。** 某些嵌入式平台可以通过 SoC 文档规定的寄存器或固件描述确定 DRAM 范围。但这种方法依赖具体硬件，移植性较差，不适合作为所有平台通用的方案。

不建议操作系统在没有平台支持的情况下，简单地向任意物理地址写入或读取数据来“试探”内存范围：该地址可能对应设备寄存器、固件保留区或不存在的物理空间，探测本身就可能触发异常或产生副作用。

因此，较稳妥的设计是：**由固件或引导程序提供可信的物理内存区域描述，内核解析并校验这些范围，再结合内核自身和其他保留区域的布局，计算最终可供物理内存分配器使用的页面集合。** 本实验采用 DTB 获取内存信息，就是这种设计方式的一个例子。

---

### 功能模块二：Best-Fit 物理页分配

**负责人：** 2413124-梁昊

#### 模块功能描述

**需要实现/修改的函数：**

```c
static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);

const struct pmm_manager best_fit_pmm_manager;
```

**功能说明：**

本模块实现物理内存管理器 `best_fit_pmm_manager`，向上提供与 `default_pmm_manager` 相同的接口。`pmm.c` 中的 `init_memmap()`、`alloc_pages()`、`free_pages()`、`nr_free_pages()` 不需要改动，只要把 `pmm_manager` 指向本模块即可。

分配采用最佳适配策略：遍历空闲链表，在所有 `property >= n` 的连续空闲块中选择 `property` 最小的那个，从而尽量保留大块内存。若选中的块比请求大，则把剩余部分留在链表里继续作为空闲块。释放时按地址顺序插回链表，并与前后相邻的空闲块合并，避免产生碎片。

模块一与本模块的接口是 `init_memmap(base, n)`：模块一算好可用物理内存区间后，把这段区间交给本模块建立空闲链表，之后内核的所有页分配都经过本模块。

#### 最终提示词

````markdown
[PROMPT]
在 riscv64-ucore 的 lab2 中实现 best_fit_pmm.c。当前文件只有 pmm_manager 所需函数的空位，
同一目录下的 default_pmm.c 已经提供 first-fit 的完整实现，可作为结构参考，但分配策略必须改成最佳适配。

[RELY]
- struct Page 的字段：ref、flags、property、page_link
- PageReserved / PageProperty / SetPageProperty / ClearPageProperty
- list_init / list_add / list_add_before / list_del / list_empty / list_next / list_prev
- le2page、alloc_page、free_page
- free_area_t、free_list、nr_free

[GUARANTEE]
实现并导出：
static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);
const struct pmm_manager best_fit_pmm_manager;
同时把 pmm.c 中 init_pmm_manager() 的选择改为 &best_fit_pmm_manager。

[SPECIFICATION]
1. init_memmap：把 [base, base+n) 的每页清 ref、清 flags、标记 Reserved；首页 property = n 并置 Property；
   按地址顺序插入 free_list；nr_free += n。
2. alloc_pages(n)：n > nr_free 返回 NULL；遍历整个链表，在所有 property >= n 的块中选择 property 最小者；
   找不到返回 NULL；选中后从链表删除，若 property > n 则把 page+n 开始的剩余块插回并令 property -= n；
   nr_free -= n；返回块首。
3. free_pages(base, n)：检查每页非 Reserved 且非 Property，清 flags、清 ref；base->property = n、
   置 Property、nr_free += n；按地址顺序插回；分别与前一空闲块和后一空闲块合并。
4. 不得修改 basic_check 和 best_fit_check。
````

#### 实现迭代过程

本模块的实现经历了 2 次迭代，过程如下：

---

##### 第一次迭代

先按 `default_pmm.c` 的结构写出五个函数，分配部分沿用了 first-fit 的写法：遍历链表，遇到第一个 `property >= n` 的块就 `break`。

**遇到的问题：**

运行注入测试后 `[Test 2] Best-Fit: FAIL`。`best_fit_check` 在释放 `p0+1` 的两页和 `p0+4` 的一页之后，期望分配 1 页时返回 `p0+4` 这个最小的空闲块，但 first-fit 返回了链表中靠前的大块，导致 `assert(p0 + 4 == p1)` 失败。

**问题解决策略：**

在提示词的 [SPECIFICATION] 中把分配规则明确成“遍历整个链表，在所有足够大的块中选 `property` 最小的一个”，并记录当前最小块大小 `min_size`，而不是遇到第一个合适的块就退出。修改后该断言通过。

---

##### 第二次迭代

**遇到的问题：**

分配已经符合最佳适配，但 `best_fit_check` 末尾的 `assert(total == 0)` 仍会失败。原因是释放时只把页插回链表，没有和相邻空闲块合并，空闲链表里出现本应合并的相邻块，最后按块统计的数量与原始链表对不上。

**问题解决策略：**

在提示词中补充释放流程的两个合并步骤：插回后先看前驱，若 `prev + prev->property == base` 则把当前块并入前驱；再看后继，若 `base + base->property == next` 则把后继并入当前块。同时插入时保持按地址升序，保证相邻块在链表中也相邻。

---

##### 最终结果

经过 2 次迭代，该模块最终：
- 编译通过
- `[Test 1] Page Init: PASS`
- `[Test 2] Best-Fit: PASS`
- `check_alloc_page() succeeded!`

#### 运行环境适配

本机使用 QEMU 11.1 + OpenSBI 1.8.1。Makefile 中的 `make qemu` 用 `-device loader` 启动时，OpenSBI 的 Domain0 Next Address 为 0，内核不会被执行；改用
`qemu-system-riscv64 -machine virt -display none -bios default -kernel bin/ucore.img -serial stdio`
后可以正常启动。

另外，`entry.S` 的启动页表原本只映射了 `0xFFFFFFFFC0000000` 以上的虚拟地址。内核入口实际从物理地址 `0x80200000` 开始执行，执行 `csrw satp` 后，下一条指令的取指仍发生在物理地址 `0x80200038`；该地址没有映射就会触发取指缺页（GDB 里 `scause = 12`）。因此在启动页表中补了一条 `0x80000000 - 0xBFFFFFFF` 的恒等映射，开分页瞬间和之后的高地址映射都能正常执行。该改动不改变页表大小，也不影响 `page_init` 对物理内存的统计。

---

### 任务三：Buddy System 分配算法（提示词设计）

**负责人：** 2413124-梁昊

本任务不提供代码框架和测试函数。下面两份提示词分别用于生成分配模块和对应的测试函数；生成的代码已经接入 `pmm_manager` 框架，并在内核中实际编译运行通过。

#### 实现概述

- 新增 `kern/mm/buddy_pmm.c` 和 `kern/mm/buddy_pmm.h`，导出 `buddy_pmm_manager`；
- 取 `MAX_ORDER = 10`，阶 k 的块大小为 2^k 页；
- 每个阶维护一条空闲链表；块首的 `Page.property` 记录阶号，`PageProperty()` 标记它是空闲块首；
- `init_memmap` 把一段连续内存按“尽量大、且相对 zone 起始地址对齐”的 2 的幂块分解后挂到对应阶；
- 分配时把页数向上取整到 2 的幂，从最小可用阶开始逐级对半拆分；
- 释放时整块归还，并循环与同阶 buddy 合并，直到不能合并；
- 新增 `kern/mm/buddy_test.c`，通过 `-DBUDDY_TEST` 编译选项在 `pmm_init()` 之后运行自测。

#### 提示词一：生成 Buddy System 分配模块

````markdown
[PROMPT]
在 riscv64-ucore 的 lab2 代码中，从零实现一个 Buddy System 物理页分配器，并接入现有的
pmm_manager 框架。现有框架中：kern/mm/pmm.h 定义 struct pmm_manager 的函数指针；
kern/mm/default_pmm.c 和 best_fit_pmm.c 是参考实现；kern/mm/memlayout.h 定义 struct Page、
free_area_t、PageReserved/PageProperty 等；kern/mm/pmm.c 的 init_pmm_manager() 负责选择管理器。
请以当前项目源码为准，不要套用其他版本 uCore 的实现。

[RELY]
- struct Page 的字段：ref、flags、property、page_link
- PageReserved / PageProperty / SetPageProperty / ClearPageProperty
- list_init / list_add / list_del / list_empty / list_next / list_prev / le2page
- struct pmm_manager 的六个函数指针签名
- cprintf、assert

[GUARANTEE]
新增 kern/mm/buddy_pmm.c 和 kern/mm/buddy_pmm.h，导出：
const struct pmm_manager buddy_pmm_manager;
实现 init、init_memmap、alloc_pages、free_pages、nr_free_pages、check 六个函数。
不修改 default_pmm.c、best_fit_pmm.c 的算法，也不修改模块一的 page_init()。

[SPECIFICATION]
1. 支持 MAX_ORDER（取 10）个阶，阶 k 的块大小为 2^k 页；每个阶维护一条空闲链表和一个空闲页计数。
2. init_memmap(base, n)：把 [base, base+n) 按“尽量大的、且相对 zone 起始地址对齐的 2 的幂块”
   分解后挂到对应阶的空闲链表；只有块首设置 Property，property 存阶号。
3. alloc_pages(n)：把 n 向上取整到 2^k；从 k 阶开始找第一个非空链表，若没有则向更高阶找；
   找到后逐级对半拆分，把另一半挂回低阶；返回块首；若超过 MAX_ORDER 仍无可用块返回 NULL。
4. free_pages(base, n)：把 n 向上取整到 2^k 后整块归还；插入对应阶链表后，循环检查 buddy
   是否同阶且空闲，可以合并则向上升阶，直到不能合并。
5. buddy 地址用“相对 zone 起始的页号异或 (1 << order)”计算；合并前必须检查 buddy 在 zone
   范围内，并且确实是同阶空闲块首，避免把更大空闲块内部的页误判成 buddy。
6. nr_free_pages 返回所有阶空闲页数之和；check 遍历各阶链表，验证每个块首都有 Property、
   property 等于所在阶，且按块统计的总页数与 nr_free 一致。
7. 代码需能在 -march=rv64gc -mabi=lp64d -Werror 下编译通过。

请说明：
- 每个函数的边界条件；
- 拆分和合并时 nr_free 分别如何变化；
- 如何保证合并后的块仍然对齐。
````

#### 提示词二：生成测试函数

````markdown
[PROMPT]
为刚生成的 buddy_pmm.c 设计一套可以在内核里直接运行的自测函数 buddy_test()。
不依赖 tools/inject.py 和 tools/grade.sh，直接编译进内核并打印每个用例的结果。

[RELY]
- buddy_pmm_manager 的六个函数指针
- struct Page 的 property、flags、page_link 字段
- cprintf、assert
- 一块静态的 struct Page 数组（例如 1024 页）作为测试用 zone，不占用真实物理内存

[GUARANTEE]
新增 kern/mm/buddy_test.c，导出 void buddy_test(void);
测试不修改 buddy 算法本身；每个用例开始前重新初始化测试 zone。

[SPECIFICATION]
至少覆盖以下用例：
1. 初始化：1024 页全部空闲，整块分配 1024 页应返回 zone 首页，释放后恢复到 1024 页。
2. 精确分配：alloc(1) 返回 zone 首页，nr_free 减少 1；释放后恢复到 1024。
3. 非 2 的幂：alloc(3) 按 4 页分配，nr_free 减少 4；释放后恢复。
4. 相邻分配：连续两次 alloc(1) 返回相邻的两页。
5. 合并：分配两个 4 页块，按相反顺序释放，应逐级合并；之后 alloc(8) 能拿到块首。
6. 边界：alloc(1025) 返回 NULL；alloc(1024) 成功；再 alloc(1) 返回 NULL。
7. 外部碎片：在 4 页 zone 中分配 4 个单页，释放第 1、3 个后 alloc(2) 应失败，
   因为两个空闲页不是 buddy；释放第 2、4 个后 alloc(2) 应成功。
8. 每个用例打印 PASS/FAIL，最后打印通过数/总数；全部通过时打印
   "Buddy System: ALL TESTS PASSED"。

请说明每个用例验证了 buddy 的哪条性质，以及哪些用例能区分 buddy 与 first-fit/best-fit。
````

#### 测试覆盖说明

| 用例 | 覆盖的核心行为或边界 |
|---|---|
| 1–3 | 初始化分解、整块分配、整块释放 |
| 4–6 | 单页分配的返回值与 `nr_free` 精确计数 |
| 7–9 | 请求页数不是 2 的幂时向上取整 |
| 10–11 | 连续分配得到相邻页，释放后能合回 |
| 12–14 | 两个 4 页块按相反顺序释放时逐级合并 |
| 15–18 | 超过 `MAX_ORDER`、占满整个 zone、耗尽后再分配 |
| 19–21 | 外部碎片：非 buddy 的空闲页不能合并，补齐 buddy 后可以合并 |

这些测试之所以有效，原因有几点：

1. 用例直接调用 `buddy_pmm_manager` 的函数指针，绕开 `pmm.c` 的分发层，能单独验证算法本身；
2. 同时检查返回值地址、`nr_free_pages()` 的增减和分配结果是否相邻，可以发现“能分配但计数错误”“合并方向错误”这类问题；
3. 第 20 条是 buddy 与 first-fit/best-fit 的关键区别：两个空闲页不是 buddy 时不能拼成 2 页块，best-fit 会直接分配，而 buddy 必须返回 NULL；
4. 第 15、17 条覆盖失败路径，避免实现只处理成功情况；
5. 每个用例前都重新初始化独立的 1024 页静态 zone，不依赖真实物理内存，也不会破坏 best_fit 的状态。

**测试截图：**

![Buddy System 编译结果](./images/buddy_build.png)

![Buddy System 测试结果](./images/buddy_test.png)

---

### 练习：[练习标题]

**负责人：** [学号-姓名]

[按照练习的具体要求进行解答]

---

### Challenge：[Challenge标题]

**负责人：** [学号-姓名]

[按照Challenge的具体要求进行解答]

---

## 五、测试与验证

<!-- 根据该实验的具体情况，提供完整的测试运行截图，应包含：
- 编译及运行成功的输出（make qemu），可能有多个
- 测试通过的结果（make grade），只有一个
-->

### 编译与运行

```bash
make GCCPREFIX=riscv-none-elf- DEFS="-march=rv64gc -mabi=lp64d"
qemu-system-riscv64 -machine virt -display none -bios default -kernel bin/ucore.img -serial stdio
```

内核串口输出：

```text
DTB Init
HartID: 0
DTB Address: 0x87e00000
Physical Memory from DTB:
  Base: 0x0000000080000000
  Size: 0x0000000008000000 (128 MB)
  End:  0x0000000087ffffff
DTB init completed
(THU.CST) os is loading ...
Special kernel symbols:
  entry  0xffffffffc02000d6 (virtual)
  etext  0xffffffffc02018a8 (virtual)
  edata  0xffffffffc0206018 (virtual)
  end    0xffffffffc0206078 (virtual)
Kernel executable memory footprint: 24KB
memory management: best_fit_pmm_manager
check_alloc_page() succeeded!
satp virtual address: 0xffffffffc0205000
satp physical address: 0x0000000080205000
```

按 `tools/inject.py` 注入评测代码后，两个检查项均通过：

```text
[Test 1] Page Init: PASS
[Test 2] Best-Fit: PASS
```

**测试截图：**

![测试结果截图](./images/test_result.png)

---

## 六、实验总结与收获

### 对操作系统的理解

1. 列出你们认为本实验中重要的知识点，以及与对应的 OS 原理中的知识点，并简要说明二者的含义、关系和差异
2. 列出你们认为 OS 原理中很重要、但在本实验中没有对应上的知识点

### AI 协作开发的经验

[写出你们在与 AI 协作过程中实际收获的经验或者感悟]

---

