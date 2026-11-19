#include <defs.h>
#include <assert.h>
#include <error.h>
#include <pmm.h>
#include <proc.h>
#include <sched.h>
#include <stdio.h>
#include <sync.h>

static void lab4_pass(const char *name)
{
    cprintf("[Test %s] PASS\n", name);
}

void test_lab4_pmm_basic(void)
{
    struct Page *p1 = alloc_page();
    struct Page *p2 = alloc_page();
    struct Page *p3 = alloc_page();
    pte_t *ptep;
    size_t free_before;

    assert(p1 != NULL && p2 != NULL && p3 != NULL);

    assert(page_insert(boot_pgdir_va, p1, 0, 0) == 0);
    assert((ptep = get_pte(boot_pgdir_va, 0, 0)) != NULL);
    assert((*ptep & PTE_V) && pte2page(*ptep) == p1 && page_ref(p1) == 1);

    assert(page_insert(boot_pgdir_va, p2, PGSIZE, PTE_U | PTE_W) == 0);
    assert(page_insert(boot_pgdir_va, p1, PGSIZE, 0) == 0);
    assert(page_ref(p1) == 2 && page_ref(p2) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(pte2page(*ptep) == p1 && (*ptep & (PTE_U | PTE_W)) == 0);

    assert(page_insert(boot_pgdir_va, p3, 2 * PGSIZE, 0) == 0);
    free_before = nr_free_pages();
    assert(page_insert(boot_pgdir_va, p3, 2 * PGSIZE, PTE_W) == 0);
    assert(page_ref(p3) == 1 && nr_free_pages() == free_before);

    page_remove(boot_pgdir_va, 0);
    page_remove(boot_pgdir_va, PGSIZE);
    page_remove(boot_pgdir_va, 2 * PGSIZE);
    page_remove(boot_pgdir_va, 2 * PGSIZE);
    assert(page_ref(p1) == 0 && page_ref(p2) == 0 && page_ref(p3) == 0);

    // Force both page-table allocation failures, including partial rollback.
    free_before = nr_free_pages();
    struct Page *target = alloc_page(), *page;
    list_entry_t held;
    list_init(&held);
    assert(target != NULL && !(boot_pgdir_va[1] & PTE_V));
    while ((page = alloc_page()) != NULL)
    {
        list_add(&held, &page->page_link);
    }
    assert(!list_empty(&held));
    assert(page_insert(boot_pgdir_va, target, 1UL << 30, PTE_R) == -E_NO_MEM);
    assert(!(boot_pgdir_va[1] & PTE_V) && page_ref(target) == 0);

    list_entry_t *le = list_next(&held);
    list_del(le);
    free_page(le2page(le, page_link));
    assert(nr_free_pages() == 1);
    assert(page_insert(boot_pgdir_va, target, 1UL << 30, PTE_R) == -E_NO_MEM);
    assert(!(boot_pgdir_va[1] & PTE_V));
    assert(page_ref(target) == 0 && nr_free_pages() == 1);

    while (!list_empty(&held))
    {
        le = list_next(&held);
        list_del(le);
        free_page(le2page(le, page_link));
    }
    free_page(target);
    assert(nr_free_pages() == free_before);

    lab4_pass("PMM");
}

void test_lab4_process_create(void)
{
    assert(current == idleproc);
    assert(idleproc != NULL && idleproc->pid == 0);
    assert(idleproc->state == PROC_RUNNABLE);
    assert(idleproc->kstack == (uintptr_t)bootstack);
    assert(idleproc->parent == NULL && idleproc->mm == NULL);
    assert(idleproc->tf == NULL && idleproc->runs == 0);
    assert(idleproc->flags == 0 && idleproc->context.ra == 0);
    assert(list_empty(&idleproc->list_link));
    assert(list_empty(&idleproc->hash_link));

    assert(initproc != NULL && initproc->pid == 1);
    assert(initproc->state == PROC_RUNNABLE);
    assert(initproc->parent == idleproc && initproc->mm == NULL);
    assert(initproc->context.ra != 0 && initproc->context.sp != 0);

    // With no runnable ordinary process, scheduling must fall back to idle.
    int idle_runs = idleproc->runs;
    initproc->state = PROC_SLEEPING;
    idleproc->need_resched = 1;
    schedule();
    assert(current == idleproc && idleproc->runs == idle_runs + 1);
    assert(!idleproc->need_resched);
    initproc->state = PROC_RUNNABLE;

    lab4_pass("Process Create");
}

static volatile int test_thread1_ran;
static volatile int test_thread2_ran;

static int test_lab4_thread(void *arg)
{
    int id = (int)(intptr_t)arg;
    int i;
    for (i = 0; i < 3; i++)
    {
        if (id == 1) test_thread1_ran = 1;
        else test_thread2_ran = 1;
        current->need_resched = 1;
        schedule();
    }
    while (1)
    {
        current->need_resched = 1;
        schedule();
    }
    return 0;
}

void test_lab4_schedule(void)
{
    int i;
    test_thread1_ran = test_thread2_ran = 0;
    assert(kernel_thread(test_lab4_thread, (void *)(intptr_t)1, 0) > 0);
    assert(kernel_thread(test_lab4_thread, (void *)(intptr_t)2, 0) > 0);

    // A switch inside an existing critical section must keep interrupts off.
    bool intr_flag;
    local_intr_save(intr_flag);
    current->need_resched = 1;
    schedule();
    assert((read_csr(sstatus) & SSTATUS_SIE) == 0);
    local_intr_restore(intr_flag);
    assert(((read_csr(sstatus) & SSTATUS_SIE) != 0) == intr_flag);

    for (i = 0; i < 8; i++)
    {
        current->need_resched = 1;
        schedule();
    }
    assert(test_thread1_ran && test_thread2_ran);
    lab4_pass("Schedule");
}
