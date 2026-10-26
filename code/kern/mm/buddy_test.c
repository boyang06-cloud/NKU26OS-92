#include <buddy_pmm.h>
#include <defs.h>
#include <list.h>
#include <stdio.h>

/*
 * Buddy System 自测。
 *
 * 测试使用一块静态的 struct Page 数组作为独立的 zone，不占用真实物理内存，
 * 也不会改动 best_fit 分配器的状态。每个用例开始前重新初始化 zone。
 */

#define TEST_PAGES 1024

static struct Page test_pages[TEST_PAGES];
static int buddy_pass;
static int buddy_total;

static void check(int cond, int idx, const char *name) {
    buddy_total++;
    if (cond) {
        buddy_pass++;
        cprintf("[Buddy %d] %-46s PASS\n", idx, name);
    } else {
        cprintf("[Buddy %d] %-46s FAIL\n", idx, name);
    }
}

static void reset_zone(size_t n) {
    buddy_pmm_manager.init();
    buddy_pmm_manager.init_memmap(test_pages, n);
}

void buddy_test(void) {
    buddy_pass = 0;
    buddy_total = 0;
    cprintf("\n===== Buddy System Tests =====\n");

    /* 1. 初始化：1024 页应正好组成一个 2^10 的空闲块 */
    reset_zone(TEST_PAGES);
    check(buddy_pmm_manager.nr_free_pages() == 1024, 1,
          "init: 1024 pages free");
    struct Page *p = buddy_pmm_manager.alloc_pages(1024);
    check(p == test_pages, 2, "init: whole zone is one block");
    buddy_pmm_manager.free_pages(p, 1024);
    check(buddy_pmm_manager.nr_free_pages() == 1024, 3,
          "free(1024): merges back to one block");

    /* 2. 精确分配 1 页 */
    reset_zone(TEST_PAGES);
    p = buddy_pmm_manager.alloc_pages(1);
    check(p == test_pages, 4, "alloc(1): returns zone head");
    check(buddy_pmm_manager.nr_free_pages() == 1023, 5,
          "alloc(1): nr_free decreases by 1");
    buddy_pmm_manager.free_pages(p, 1);
    check(buddy_pmm_manager.nr_free_pages() == 1024, 6,
          "free(1): merges back");

    /* 3. 非 2 的幂：3 页应按 4 页分配 */
    reset_zone(TEST_PAGES);
    p = buddy_pmm_manager.alloc_pages(3);
    check(p == test_pages, 7, "alloc(3): rounds up to a 4-page block");
    check(buddy_pmm_manager.nr_free_pages() == 1020, 8,
          "alloc(3): nr_free decreases by 4");
    buddy_pmm_manager.free_pages(p, 3);
    check(buddy_pmm_manager.nr_free_pages() == 1024, 9,
          "free(3): returns the whole 4-page block");

    /* 4. 连续两次分配 1 页应得到相邻页 */
    reset_zone(TEST_PAGES);
    struct Page *p0 = buddy_pmm_manager.alloc_pages(1);
    struct Page *p1 = buddy_pmm_manager.alloc_pages(1);
    check(p0 == test_pages && p1 == test_pages + 1, 10,
          "alloc(1) twice: adjacent pages");
    buddy_pmm_manager.free_pages(p0, 1);
    buddy_pmm_manager.free_pages(p1, 1);
    check(buddy_pmm_manager.nr_free_pages() == 1024, 11,
          "free both: merges back");

    /* 5. 两个 4 页块按相反顺序释放，应逐级合并 */
    reset_zone(TEST_PAGES);
    struct Page *a = buddy_pmm_manager.alloc_pages(4);
    struct Page *b = buddy_pmm_manager.alloc_pages(4);
    check(a == test_pages && b == test_pages + 4, 12,
          "alloc(4) twice: two adjacent 4-page blocks");
    buddy_pmm_manager.free_pages(b, 4);
    buddy_pmm_manager.free_pages(a, 4);
    check(buddy_pmm_manager.nr_free_pages() == 1024, 13,
          "free in reverse order: cascades to order 10");
    p = buddy_pmm_manager.alloc_pages(8);
    check(p == test_pages, 14, "alloc(8) after merge: returns head");
    buddy_pmm_manager.free_pages(p, 8);

    /* 6. 边界：请求超过最大阶、占满整个 zone */
    reset_zone(TEST_PAGES);
    check(buddy_pmm_manager.alloc_pages(1025) == NULL, 15,
          "alloc(1025): exceeds MAX_ORDER, returns NULL");
    p = buddy_pmm_manager.alloc_pages(1024);
    check(p == test_pages, 16, "alloc(1024): takes the whole zone");
    check(buddy_pmm_manager.alloc_pages(1) == NULL, 17,
          "alloc after zone exhausted: returns NULL");
    buddy_pmm_manager.free_pages(p, 1024);
    check(buddy_pmm_manager.nr_free_pages() == 1024, 18,
          "free whole zone: restores 1024 pages");

    /* 7. 外部碎片：非 buddy 的两个空闲页不能拼成 2 页块 */
    reset_zone(4);
    struct Page *q0 = buddy_pmm_manager.alloc_pages(1);
    struct Page *q1 = buddy_pmm_manager.alloc_pages(1);
    struct Page *q2 = buddy_pmm_manager.alloc_pages(1);
    struct Page *q3 = buddy_pmm_manager.alloc_pages(1);
    check(q0 == test_pages + 0 && q1 == test_pages + 1 &&
          q2 == test_pages + 2 && q3 == test_pages + 3, 19,
          "4-page zone: four single pages");

    buddy_pmm_manager.free_pages(q0, 1);
    buddy_pmm_manager.free_pages(q2, 1);
    check(buddy_pmm_manager.alloc_pages(2) == NULL, 20,
          "fragmentation: non-buddy free pages cannot merge");

    buddy_pmm_manager.free_pages(q1, 1);
    buddy_pmm_manager.free_pages(q3, 1);
    p = buddy_pmm_manager.alloc_pages(2);
    check(p == test_pages, 21,
          "after buddies freed: 2-page alloc succeeds");

    cprintf("Buddy System: %d/%d tests passed\n", buddy_pass, buddy_total);
    if (buddy_pass == buddy_total) {
        cprintf("Buddy System: ALL TESTS PASSED\n");
    } else {
        cprintf("Buddy System: SOME TESTS FAILED\n");
    }
}
