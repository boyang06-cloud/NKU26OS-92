#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
    pmm_manager = &best_fit_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
}

//lab2 function1 2411210
static void page_init(void) {
    extern char end[];

    uint64_t mem_base = get_memory_base();
    uint64_t mem_size = get_memory_size();
    uint64_t mem_end;
    uintptr_t mem_end_aligned;
    uintptr_t pages_end_va;
    uintptr_t pages_end_pa;
    uintptr_t freemem;
    size_t page_count;
    size_t free_page_count;
    size_t i;

    // 当前内核使用固定的虚实地址映射偏移
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;

    // 检查 DTB 提供的物理内存信息
    if (mem_base == 0 || mem_size == 0) {
        panic("page_init: invalid DTB memory information");
    }

    if (mem_size > (~(uint64_t)0 - mem_base)) {
        panic("page_init: physical memory range overflow");
    }

    mem_end = mem_base + mem_size;

    // nbase 固定对应 DRAM_BASE，因此当前框架要求内存从这里开始
    if (mem_base != DRAM_BASE) {
        panic("page_init: unexpected physical memory base");
    }

    mem_end_aligned = ROUNDDOWN(mem_end, PGSIZE);
    npage = PPN(mem_end);

    if (npage <= nbase || mem_end_aligned <= mem_base) {
        panic("page_init: invalid physical memory range");
    }

    // 在内核镜像结束位置之后按页对齐放置 Page 数组
    pages = (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE);

    page_count = npage - nbase;

    // 所有页面先初始化为保留状态
    for (i = 0; i < page_count; i++) {
        pages[i].ref = 0;
        pages[i].flags = 0;
        pages[i].property = 0;
        SetPageReserved(&pages[i]);
    }

    // 计算 Page 描述符数组末尾对应的物理地址
    pages_end_va =
        (uintptr_t)pages + page_count * sizeof(struct Page);

    pages_end_pa = PADDR(pages_end_va);

    // 描述符数组之后的第一个页对齐地址作为空闲内存起点
    freemem = ROUNDUP(pages_end_pa, PGSIZE);

    if (freemem < mem_base || freemem >= mem_end_aligned) {
        panic("page_init: no available physical memory");
    }

    free_page_count = (mem_end_aligned - freemem) / PGSIZE;

    if (free_page_count == 0) {
        panic("page_init: no free pages");
    }

    // 将剩余可用页面交给当前的物理内存管理器
    init_memmap(pa2page(freemem), free_page_count);
}


/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
