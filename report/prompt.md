# 模块一：物理内存初始化功能
## 任务一：实现物理内存探测与页描述符初始化功能

```
我正在完成 riscv64-ucore 操作系统实验 Lab2，负责模块一：物理内存探测与页描述符初始化。

请先阅读以下源码：
- kern/driver/dtb.c
- kern/driver/dtb.h
- kern/init/entry.S
- kern/init/init.c
- kern/mm/pmm.c
- kern/mm/pmm.h
- kern/mm/memlayout.h
- kern/mm/mmu.h

当前 pmm.c 中存在 `//lab2 function1 YOUR CODE` 标记，pmm_init() 调用了 page_init()，但源码中尚未实现该函数。

请结合当前源码实现 page_init()，而不是直接套用其他版本 uCore 的代码。

具体要求：
1. 设置 va_pa_offset = PHYSICAL_MEMORY_OFFSET。
2. 通过 get_memory_base() 和 get_memory_size() 获取 DTB 解析得到的物理内存范围，并检查其有效性。
3. 根据物理内存结束地址设置 npage，满足当前框架中 PPN 的定义和 nbase 的约定。
4. 在链接器符号 end 之后按 PGSIZE 对齐设置 pages。
5. 初始化 npage - nbase 个 struct Page 描述符，合理初始化 ref、flags 和 property，并将页面初始标记为保留。
6. 计算描述符数组结束后的物理地址，将其按页向上对齐作为空闲内存起点。
7. 确保内核镜像、描述符数组和其他前置占用内存不会被加入空闲页区。
8. 使用现有的 pa2page()、PADDR() 和 init_memmap() 接口建立可用内存区间。
9. 不修改其他组员负责的页面分配算法，不改变现有接口。
10. 解释每一步的设计原因、边界条件及潜在错误。

请同时给出：
- 推荐的代码补丁；
- 每个关键变量的含义；
- 对照 tools/test_lab2.c 中 test_page_init() 的逐项验证方法；
- 可能出现的编译错误、运行错误和对应排查步骤。

如果发现当前代码框架存在限制或假设，请明确说明，不要默默替换成其他版本的实现。
```

# 模块二：Best-Fit 物理页分配
## 任务一：实现最佳适配页面分配器

```
我正在完成 riscv64-ucore 操作系统实验 Lab2，负责模块二：Best-Fit 物理页分配。

请先阅读以下源码：
- kern/mm/best_fit_pmm.c
- kern/mm/best_fit_pmm.h
- kern/mm/default_pmm.c
- kern/mm/pmm.c
- kern/mm/pmm.h
- kern/mm/memlayout.h
- libs/list.h

当前 best_fit_pmm.c 中存在 `//lab2 function2-2 YOUR CODE` 标记，需要实现 best_fit_pmm_manager。
可以参照 default_pmm.c 的结构，但分配策略必须从 first-fit 改成 best-fit。

具体要求：
1. best_fit_init：初始化 free_list，nr_free = 0。
2. best_fit_init_memmap：把 [base, base+n) 每页清 ref、清 flags 并标记 Reserved；
   首页 property = n 并置 Property；按地址升序插入 free_list；nr_free += n。
3. best_fit_alloc_pages(n)：n > nr_free 时返回 NULL；遍历整个空闲链表，
   在所有 property >= n 的块中选择 property 最小的一个；找不到返回 NULL；
   选中块若大于 n，把 page+n 开始的剩余部分插回链表；nr_free -= n；返回块首。
4. best_fit_free_pages(base, n)：检查每页非 Reserved 且非 Property；清 flags 和 ref；
   设置 base->property = n 并置 Property；nr_free += n；按地址升序插回链表；
   分别与前一空闲块、后一空闲块合并。
5. 定义并导出 best_fit_pmm_manager，把各函数指针填好。
6. 在 pmm.c 的 init_pmm_manager() 中选择 &best_fit_pmm_manager。
7. 不修改 basic_check、best_fit_check，也不修改模块一的 page_init。

请同时给出：
- 推荐代码；
- 最佳适配与首次适配在分配路径上的具体差别；
- 释放时为什么必须合并前后空闲块；
- 对照 tools/test_lab2.c 中 test_best_fit() 以及 best_fit_pmm.c 中 best_fit_check() 的验证方法。
```

# 模块三：Challenge——任意大小内存单元的简化 SLUB 分配器

以下两份提示词按 `prompt_template.md` 的四段式模板编写，依次使用。第一份生成分配模块，第二份读取生成的模块并生成测试；本节仅描述实现与测试要求，不代表代码已经实现或测试已经通过。路径均相对于仓库根目录。

## 提示词一：从零实现 SLUB 模块并接入内核

```text
[PROMPT]
**任务**：我正在完成 riscv64-ucore 操作系统 Lab2 的 Challenge。实验没有提供 SLUB 的代码框架，请从零新建 code/kern/mm/slub.h、code/kern/mm/slub.c，实现简化 SLUB 内存对象分配器，并修改 code/kern/init/init.c 接入内核。采用两层架构：底层复用现有 Best-Fit 物理页分配器，上层将页面划分成多个可复用的小对象槽；大对象使用连续多页。要求能体现对象缓存、空闲对象链表和页面回收的主体思想，不要求实现 Linux SLUB 的全部功能，也不要求特定时空复杂度。

**操作要求**：必须修改项目中的实际文件，而不是仅展示代码。先阅读 code/kern/mm/pmm.h、pmm.c、memlayout.h、mmu.h、best_fit_pmm.c、code/kern/init/init.c、code/libs/defs.h、list.h、string.h 及 code/Makefile。保留已有物理页管理、检查函数和启动流程；不要修改 struct Page 布局或复用其 flags、property、page_link 保存 SLUB 元数据。仅允许新增上述 slub 文件、在 init.c 添加初始化调用，以及必要时补充 Makefile 的编译配置。现有 kern/mm 已属于源码扫描目录，应先核实现有规则是否已能编译新文件。本提示词只生成分配模块，测试由下一份提示词完成。

**输出要求**：完成实际文件修改后，说明数据结构、大小类别、元数据布局、对象地址与所属 slab 的关联方式、分配释放流程及内核接入点。列出修改文件和实际执行的构建命令及结果；若工具链缺失或构建失败，说明真实原因，不能声称已经通过。以工程真实代码为准，不引入 libc、Linux 内核头文件或工程没有提供的接口。

[RELY]
1. 当前物理页分配接口位于 code/kern/mm/pmm.h：
   struct Page *alloc_pages(size_t n);
   void free_pages(struct Page *base, size_t n);
   size_t nr_free_pages(void);
   #define alloc_page() alloc_pages(1)
   #define free_page(page) free_pages(page, 1)
   static inline uintptr_t page2pa(struct Page *page);
   static inline struct Page *pa2page(uintptr_t pa);
   extern uint64_t va_pa_offset;
   extern struct Page *pages;
   extern size_t npage;
   extern const size_t nbase;
   pmm_init() 已选择 best_fit_pmm_manager，完成页初始化及原有检查。alloc_pages(n) 分配连续 n 页，失败返回 NULL；free_pages 必须接收正确的块首页和原页数，不能传入 0 页。

2. code/kern/mm/mmu.h 中定义：
   #define PGSIZE 4096
   #define PGSHIFT 12
   #define PPN(la) (((uintptr_t)(la)) >> PGSHIFT)

3. code/kern/mm/memlayout.h 中的页描述符为：
   struct Page {
       int ref;
       uint64_t flags;
       unsigned int property;
       list_entry_t page_link;
   };
   页描述符由 PMM 管理，SLUB 元数据应放在已分配页内部或模块静态存储中。释放页之后，不能再访问该页中的元数据。

4. 当前采用固定虚实地址偏移。把 page2pa(page) + va_pa_offset 转换为指针可访问已分配页；PADDR(kva) 可将有效内核虚拟地址转为物理地址。pmm.h 中的 KADDR 宏目前被注释，且没有 page2kva/kva2page，不能直接调用这些接口。

5. code/libs/defs.h 提供 size_t、uintptr_t、uint8_t、NULL、ROUNDUP、ROUNDDOWN；size_t 和 uintptr_t 均为 64 位无符号类型。ROUNDUP 内部包含加法，必须在调用前处理溢出。不要假定存在 SIZE_MAX，可以使用 (size_t)-1 表示最大 size_t。
   code/libs/string.h 提供：
   void *memset(void *s, char c, size_t n);
   void *memcpy(void *dst, const void *src, size_t n);
   还可使用 list.h、assert.h 和 stdio.h 中工程已有的链表、断言及 cprintf。

6. kern_init() 目前在 pmm_init() 后进入永久循环。SLUB 初始化应放在 pmm_init() 返回后。编译环境为 RISC-V 64 位、GNU99、-nostdinc、-nostdlib，并启用 -Wall、-Werror；不能使用宿主 malloc/free 或未提供的运行库。

[GUARANTEE]
在新建的 code/kern/mm/slub.h 中声明，并在 slub.c 中实现：
    void slub_init(void);
    void *slub_alloc(size_t size);
    void slub_free(void *ptr);

在 slub.h 中新增公开常量，供调用者和后续测试使用：
    #define SLUB_ALIGN 8
    #define SLUB_SMALL_MAX 2048

修改已有接口：
    int kern_init(void);
保留其 noreturn 属性及已有逻辑，在 pmm_init() 返回后调用一次 slub_init()。

允许新增模块内部的 static 辅助函数、静态缓存描述符及宏。不要求对外提供 Linux kmalloc/kmem_cache 接口。初始化和元数据管理不得递归调用 slub_alloc 来引导自身。

[SPECIFICATION]
## slub_init
**Pre-Condition**：pmm_init() 已完成，物理页分配器可用；当前实验按单核、无并发调用处理。
**Post-Condition**：所有大小类别和模块内部状态已初始化，后续分配释放可用；采用静态缓存描述符和按需分配 slab 页的方式，初始化不消耗物理页。
  **Case 1**：首次调用时，建立空的缓存管理状态，不预先申请对象页。
  **Case 2**：重复调用时直接返回，不清空已建立的状态、不丢失仍在使用的对象。
**Requirements**：不得改动 PMM 的空闲链表、页计数或已分配页面的所有权。

## slub_alloc
**Pre-Condition**：slub_init() 已执行；size 表示调用者需要的有效负载字节数。
**Post-Condition**：成功返回至少 SLUB_ALIGN 字节对齐的内核虚拟地址，其后至少 size 字节可读写；所有存活对象的有效负载互不重叠，且不覆盖分配器元数据。失败返回 NULL，既有对象和管理状态保持有效，不泄漏已申请的页。不要求将新分配的有效负载清零。
  **Case 1**：size == 0 时返回 NULL，不消耗对象槽或页面。
  **Case 2**：1 <= size <= SLUB_SMALL_MAX 时，使用 8、16、32、64、128、256、512、1024、2048 字节有效负载类别，向上选择可容纳请求的最小类别。同一 slab 页只管理一个类别；优先从已有可分配 slab 的空闲对象链表取得槽位，必要时通过 alloc_pages(1) 新建 slab 页。页头、对象头、链表指针和对齐填充都必须计入布局，不能假定整页均为用户负载。空闲链表可复用空闲对象区域，但对象分配后不得覆盖其有效负载。
  **Case 3**：size > SLUB_SMALL_MAX 时，通过 alloc_pages 分配足够的连续页，并保存块首页和实际页数等释放所需信息。计算必须覆盖元数据、对齐填充和全部有效负载；申请 PGSIZE 字节不保证仅占一页。返回的仍是有效负载地址。
  **Case 4**：大小计算发生加法、乘法或向上取整溢出，或请求明显超过系统可管理内存时，在访问对象内存前返回 NULL。无法获得所需页时也返回 NULL，回滚中途取得的资源。
**Requirements**：区分“类别的有效负载容量”和“对象槽总跨度”。保证每个小对象类别在一页内确实存在可用槽位，可靠管理空闲、部分使用、全满状态；可使用单一 slab 链表加使用计数简化实现。大小类别仅是内部取整策略，调用者可以请求非类别大小。请求能否成功受可用内存和连续页条件限制，不能通过仅支持固定几个入参或设置过小的请求上限代替任意大小分配。

## slub_free
**Pre-Condition**：模块已初始化；ptr 为 NULL，或是本模块返回且尚未释放的对象起始地址。不要求支持非法地址、内部偏移地址或重复释放，须明确这些情况不在接口保证范围内。
**Post-Condition**：释放目标对象，对其他存活对象的数据和可用性无影响；其槽位或底层页可再次分配。不要求保留已释放对象的数据。
  **Case 1**：ptr == NULL 时直接返回，不改变任何资源计数。
  **Case 2**：小对象释放后放回所属 slab 的空闲对象链表，更新使用计数和可分配状态；原来全满的 slab 必须重新能够分配。当该 slab 最后一个存活对象释放后，从管理结构中移除该 slab，并立即把其页归还 PMM，不保留空 slab 页。
  **Case 3**：大对象释放时，根据元数据恢复最初分配的块首页和页数，完整归还连续页。不要仅按 ptr 所在页释放，也不要向调用者要求额外传入大小。
**Requirements**：在归还页之前保存后续所需的信息并移除页内链表节点，归还后不再解引用页内元数据。能够在其他对象仍存活时释放任一对象，不能要求严格按逆序释放。所有对象释放后，nr_free_pages() 应恢复到本轮分配前的值。

## 内核接入与验证
**Pre-Condition**：工程已有模块及其检查逻辑完整保留。
**Post-Condition**：内核启动时在 PMM 可用之后完成 SLUB 初始化，现有物理页检查仍能通过。
**Requirements**：在 code/ 下执行 make 构建，必要时先 make clean；可以运行 make qemu 检查启动，但此阶段不能据此声称 SLUB 功能测试已通过。说明单核简化、大小分类和大对象路径等设计取舍，确保下一份提示词能基于公开接口编写测试。
```

## 提示词二：为生成的 SLUB 模块设计并接入测试

```text
[PROMPT]
**任务**：针对上一份提示词已经生成的 SLUB 分配模块，独立设计并实现内核内测试函数，验证不同大小对象的分配、写入、释放、重用和页面回收。实验没有提供 SLUB 测试框架，请新建 code/kern/mm/slub_test.h、code/kern/mm/slub_test.c，并修改 code/kern/init/init.c，在 slub_init() 后调用测试入口。

**操作要求**：必须修改实际文件。先阅读 code/kern/mm/slub.h、slub.c、pmm.h、pmm.c、mmu.h、code/kern/init/init.c 和 code/Makefile，再按公开接口及本提示词的行为规格编写测试。测试应独立计算预期结果，以实际读写、对象区间和 PMM 页数验证正确性，不得把分配器内部计算原样复制为测试判据。允许读取源码以了解类别布局和构造状态转换，但不要直接操作模块内部链表、PMM 空闲链表或管理计数。

发现第一份实现不符合约定时，应通过测试暴露问题并说明原因；不得删除测试、放宽断言或改变约定使其通过。本任务仅新增测试文件、添加启动调用及必要的构建配置，不修改现有评分测试和分配算法；若被测实现存在缺陷，应如实报告失败位置和修复建议。

**输出要求**：给出实际修改文件、每组测试的目的与判据，以及“测试行为—可能检出的缺陷—有效性理由”的简要说明，便于写入实验报告。执行实际构建和 QEMU 测试，报告真实日志；不能把预期输出冒充运行结果。若环境阻止执行，明确尚未验证的部分。

[RELY]
1. 第一份提示词要求 slub.h 导出：
   #define SLUB_ALIGN 8
   #define SLUB_SMALL_MAX 2048
   void slub_init(void);
   void *slub_alloc(size_t size);
   void slub_free(void *ptr);
   小对象有效负载类别为 8、16、32、64、128、256、512、1024、2048；超过 2048 字节走连续多页路径。以上是约定接口，执行前必须核对生成的实际代码，发现缺失应明确报告。

2. 约定行为：零大小返回 NULL；成功指针至少 8 字节对齐，负载至少为请求大小；不要求初始内容为零；不同存活对象负载不重叠；释放 NULL 无副作用；不测试非法指针或重复释放。空 slab 立即归还物理页，大对象完整归还其页；slub_init 不申请页且重复调用不丢失对象。

3. 工程提供：
   size_t nr_free_pages(void);
   struct Page *alloc_pages(size_t n);
   void free_pages(struct Page *base, size_t n);
   #define PGSIZE 4096
   size_t、uintptr_t、uint8_t、NULL，以及 assert、cprintf、memset。
   size_t 为 64 位无符号类型，最大值可写为 (size_t)-1。
   返回地址是内核虚拟地址，测试应直接读写该地址，不能把它作为物理地址使用。

4. 当前 Makefile 扫描 kern/mm，新建测试文件放在这里可以复用编译规则。code/tools/test_lab2.c 仅包含已有的页面初始化和 Best-Fit 评分测试，不能假定其中已有 SLUB 测试，也不能假定 make test 会自动调用新增函数。kern_init() 的启动调用才是本次测试的实际入口。
   默认内核栈只有 2 * PGSIZE。较大的指针表、长度表等测试数据使用固定上界的 static 数组，不能放在栈上，也不能使用被测 slub_alloc 分配测试管理结构。环境为 GNU99，无宿主 libc。

[GUARANTEE]
在 code/kern/mm/slub_test.h 中声明，并在 slub_test.c 中实现：
    void slub_test(void);

修改已有接口：
    int kern_init(void);
在已有 slub_init() 后、永久循环前调用一次 slub_test()，保留其他启动逻辑。

各测试组、模式填充、数据核验、区间重叠检查等辅助函数设为 static，自行命名。允许新增测试用静态数组，不增加必须依赖的分配器公开接口。成功组打印明确的 [SLUB Test] ... PASS，全部通过后打印 [SLUB Test] ALL PASS；任何断言失败必须中止成功路径，不得继续打印总通过。

[SPECIFICATION]
## slub_test
**Pre-Condition**：pmm_init 和 slub_init 已完成，单核顺序执行，测试开始时没有其他模块持有 SLUB 对象。运行期间没有其他消费者改变 PMM 空闲页数量。
**Post-Condition**：完成下面各组可重复测试，释放本组申请的全部对象；每组结束及全部结束时，nr_free_pages() 均等于对应的起始快照。测试成功有明确日志，失败时能够定位测试组、请求大小或对象编号。

  **Case 1：初始化与空操作**。
  记录初始页数，检查 slub_alloc(0) == NULL，调用 slub_free(NULL)，验证页数不变。持有一个已写入模式的小对象后再调用 slub_init()，检查其内容仍正确，且后续仍能分配、释放，最终回到初始页数。该测试检验零长度约定、空释放和重复初始化不会破坏状态。

  **Case 2：类别边界、页边界和任意大小**。
  至少覆盖 1、7、8、9、15、16、17、31、32、33、63、64、65、127、128、129、255、256、257、511、512、513、1023、1024、1025、2047、2048、2049、PGSIZE-1、PGSIZE、PGSIZE+1、2*PGSIZE、2*PGSIZE+1，以及非整齐大小 3、37、100、777、3000、6000。每次成功后验证非 NULL 和对齐，对全部请求字节写入并读回，包括首字节、尾字节和跨页部分，再释放。分组记录并核对空闲页恢复，不能硬编码“大对象一定只占 ceil(size / PGSIZE) 页”，因为元数据和对齐也会占空间。该测试检验向上选择类别、临界值分支和实际容量是否正确。

  **Case 3：多个存活对象互不干扰**。
  同时保留多个同类和不同类对象，包含小对象与多页大对象。先检查每对有效负载区间 [ptr, ptr+size) 不相交，再以“对象编号和字节偏移”确定不同的非零数据模式，写满所有对象后逐一核验。以非分配顺序释放一部分对象，每次释放或后续重新分配后再次核验所有仍存活对象；新对象也写满并参与重叠检查。最后释放全部对象。不能只检查指针互不相等，也不能给所有对象填相同模式。该测试用于检出槽跨度错误、元数据侵入负载、对象别名和释放误伤其他对象。

  **Case 4：跨 slab 扩容、空槽复用与整页回收**。
  使用 size=8 的同类小对象，分配足够数量确保该类别需要至少两张 slab 页，可使用 PGSIZE / SLUB_ALIGN + 1 个对象及固定大小静态指针表；检查实际页面消耗，确保扩容路径确实执行。对对象写入不同模式后，释放其中一部分，保证相关 slab 仍有存活对象；重新申请同类对象并检查未释放对象内容及新旧存活区间。
  构造至少一个“全满 slab 释放一个槽后重新分配”的状态，可结合实际 slab 布局或按页对齐地址分组确认该状态。只要仍有适用空槽，重新分配不得消耗新的物理页；不要求分配器返回某个固定历史地址。逐步清空所有 slab，最终空闲页应精确恢复。该测试检验 full/partial 状态转换、空闲链表复用、扩容和最后一个对象释放时的整页归还。

  **Case 5：大对象分配和乱序释放**。
  同时申请如 2049、4096、4097、8193、16391 字节对象，逐字节检查不同数据模式；将新建的相应静态记录保留至释放结束。乱序释放后核验其余对象，最终检查页数恢复。允许记录每次分配的页数差并在释放时核对相应归还量，不依赖具体元数据头大小。该测试检验大对象容量、跨页写入、原始块首页及页数恢复。

  **Case 6：整数溢出与资源不足失败**。
  同时保留已写入模式的小对象和大对象。测试 (size_t)-1、(size_t)-1-7、(size_t)-1-PGSIZE 等无法满足的请求，必须返回 NULL，且每次调用前后的页数及存活对象内容均不改变。
  另外构造不会触发整数溢出、但显然超出当前空闲页总量的大对象请求，例如在验证乘加运算安全后请求 (nr_free_pages()+1)*PGSIZE 字节；它必须返回 NULL。该普通资源不足测试与极大整数测试分开执行，不能只靠溢出分支替代内存不足验证。失败后再申请和释放正常大小对象，确认分配器仍可用；最终清理全部存活对象并检查基线恢复。该测试检验大小运算、防止小块误分配、失败无泄漏及失败后的状态完整性；不要求耗尽机器全部物理内存。

  **Case 7：重复混合分配与释放**。
  以固定种子或固定序列执行至少数百次有界操作，在固定上限的静态槽表中交错分配、检查、释放不同大小对象。空槽才可分配，存活槽才可释放；每次释放前及周期检查时验证数据模式，新增对象后检查区间不重叠。结束时清空全部槽，并核对页数恢复。完整测试入口连续运行两轮，保证每轮均通过且无累计泄漏。该测试检验多次复用后链表、计数及不同类别的状态是否持续正确。

**Requirements**：
- 断言应针对明确的接口约定，不能假定释放后数据仍保留、分配结果自动清零或固定的地址返回顺序。不得读写超过请求大小的区域，不对已释放对象解引用；释放前若需要页地址等信息必须先保存。
- 分配器不保证非法指针和重复释放的处理，因此不得用会使整套测试提前 panic 的未定义调用充当正常边界测试。
- 正常测试使用有限内存，必要的分配若失败应给出明确诊断，不能静默跳过该组并打印 PASS。以 nr_free_pages() 检查回收时，应确保该组不留下任何存活对象且没有其他分配干扰。
- 在 code/ 下执行 make clean、make，再运行 make qemu 观察串口输出。启动末尾本来就会永久循环，采用合理超时结束 QEMU 时，应结合 ALL PASS 和有无 panic 判断结果，不能仅因超时就判失败或判通过。
- 另外运行现有 make grade，确认原有页面初始化和 Best-Fit 测试不受影响；把原有测试结果与新增 SLUB 测试结果分别说明，不能相互替代。
- 简要解释覆盖的核心行为和边界情况及其检错理由，并说明这些测试可有效发现所列缺陷，但不能形式化证明所有执行路径都正确。
```
