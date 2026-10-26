#include <buddy_pmm.h>
#include <list.h>
#include <pmm.h>
#include <stdio.h>

/*
 * Buddy System 物理页分配器
 *
 * 每个阶 k 的块大小为 2^k 页。空闲块按阶挂在 free_lists[k] 上，
 * 块首的 Page.property 记录阶号，PageProperty() 标记它是空闲块首。
 * 分配时向上取整到 2 的幂，从最小可用阶开始逐级对半拆分；
 * 释放时把整块归还，并循环与同阶的 buddy 合并。
 */

#define MAX_ORDER 10

static list_entry_t free_lists[MAX_ORDER + 1];
static size_t nr_free_total;
static struct Page *buddy_zone;       /* 本分配器管理的第一页 */
static size_t buddy_zone_npages;      /* 管理的总页数 */

/* 返回能容纳 n 页的最小阶 */
static size_t order_for(size_t n) {
    size_t order = 0;
    while ((1UL << order) < n) {
        order++;
    }
    return order;
}

/* 返回与 p 同阶的 buddy；越界返回 NULL */
static struct Page *buddy_of(struct Page *p, size_t order) {
    size_t idx = (size_t)(p - buddy_zone);
    size_t buddy_idx = idx ^ (1UL << order);
    if (buddy_idx >= buddy_zone_npages) {
        return NULL;
    }
    return buddy_zone + buddy_idx;
}

static void add_block(struct Page *p, size_t order) {
    p->property = order;
    SetPageProperty(p);
    list_add(&free_lists[order], &(p->page_link));
    nr_free_total += (1UL << order);
}

static void buddy_init(void) {
    for (size_t order = 0; order <= MAX_ORDER; order++) {
        list_init(&free_lists[order]);
    }
    nr_free_total = 0;
    buddy_zone = NULL;
    buddy_zone_npages = 0;
}

/*
 * 把 [base, base + n) 分解成尽量大的、相对 zone 起始地址对齐的
 * 2 的幂块，逐个挂到对应阶的空闲链表上。
 */
static void buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);

    buddy_zone = base;
    buddy_zone_npages = n;
    for (size_t order = 0; order <= MAX_ORDER; order++) {
        list_init(&free_lists[order]);
    }
    nr_free_total = 0;

    for (size_t i = 0; i < n; i++) {
        base[i].ref = 0;
        base[i].flags = 0;
        base[i].property = 0;
        list_init(&base[i].page_link);
    }

    size_t remaining = n;
    struct Page *p = base;
    while (remaining > 0) {
        size_t order = 0;
        while (order < MAX_ORDER &&
               (1UL << (order + 1)) <= remaining &&
               (((size_t)(p - buddy_zone)) & ((1UL << (order + 1)) - 1)) == 0) {
            order++;
        }
        add_block(p, order);
        p += (1UL << order);
        remaining -= (1UL << order);
    }
}

static struct Page *buddy_alloc_pages(size_t n) {
    assert(n > 0);

    size_t order = order_for(n);
    if (order > MAX_ORDER) {
        return NULL;
    }

    /* 找到第一个有可用块的阶 */
    size_t cur = order;
    while (cur <= MAX_ORDER && list_empty(&free_lists[cur])) {
        cur++;
    }
    if (cur > MAX_ORDER) {
        return NULL;
    }

    /* 取该阶的第一个块 */
    list_entry_t *le = list_next(&free_lists[cur]);
    struct Page *p = le2page(le, page_link);
    list_del(&(p->page_link));
    ClearPageProperty(p);

    /* 逐级对半拆分，把另一半挂回低阶 */
    while (cur > order) {
        cur--;
        struct Page *buddy = p + (1UL << cur);
        buddy->property = cur;
        SetPageProperty(buddy);
        list_add(&free_lists[cur], &(buddy->page_link));
    }

    p->property = order;
    nr_free_total -= (1UL << order);
    return p;
}

static void buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0);

    size_t order = order_for(n);
    if (order > MAX_ORDER) {
        return;
    }

    /* 整块重置为未使用状态，只有块首挂到链表上 */
    for (size_t i = 0; i < (1UL << order); i++) {
        base[i].ref = 0;
        base[i].flags = 0;
        base[i].property = 0;
    }
    base->property = order;
    SetPageProperty(base);
    list_add(&free_lists[order], &(base->page_link));
    nr_free_total += (1UL << order);

    /* 与同阶 buddy 循环合并 */
    while (order < MAX_ORDER) {
        struct Page *buddy = buddy_of(base, order);
        if (buddy == NULL) {
            break;
        }
        if (!PageProperty(buddy) || buddy->property != order) {
            break;
        }

        list_del(&(buddy->page_link));
        list_del(&(base->page_link));
        ClearPageProperty(buddy);
        ClearPageProperty(base);

        if (buddy < base) {
            base = buddy;
        }
        order++;
        base->property = order;
        SetPageProperty(base);
        list_add(&free_lists[order], &(base->page_link));
    }
}

static size_t buddy_nr_free_pages(void) {
    return nr_free_total;
}

static void buddy_check(void) {
    size_t total = 0;
    for (size_t order = 0; order <= MAX_ORDER; order++) {
        list_entry_t *le = &free_lists[order];
        while ((le = list_next(le)) != &free_lists[order]) {
            struct Page *p = le2page(le, page_link);
            assert(PageProperty(p));
            assert(p->property == order);
            total += (1UL << order);
        }
    }
    assert(total == nr_free_total);
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
