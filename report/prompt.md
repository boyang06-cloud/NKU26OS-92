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