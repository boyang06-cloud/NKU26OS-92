#include <slub_test.h>
#include <slub.h>
#include <pmm.h>
#include <stdio.h>

#define TEST_SLOTS (PGSIZE / SLUB_ALIGN + 1)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define STRESS_SLOTS 64
#define STRESS_STEPS 1024

struct test_object {
    uint8_t *ptr;
    size_t size;
    uint32_t tag;
};

/* Keep bookkeeping off the 8 KiB kernel stack and independent of SLUB. */
static struct test_object objects[TEST_SLOTS];
static bool refill[TEST_SLOTS];
static unsigned int test_round;
static const char *test_group;

#define CHECK(condition, ...)                                         \
    do {                                                             \
        if (!(condition)) {                                          \
            cprintf("[SLUB Test] round %u %s FAIL\n",                  \
                    test_round, test_group);                         \
            panic(__VA_ARGS__);                                      \
        }                                                            \
    } while (0)

static uint8_t
pattern(uint32_t tag, size_t offset) {
    uint32_t x = tag * 2654435761U ^ (uint32_t)offset * 2246822519U;
    x ^= x >> 13;
    return (uint8_t)(1 + x % 255);
}

static void
verify_object(size_t index) {
    struct test_object *o = &objects[index];
    CHECK(o->ptr != NULL, "slot %lu is not live", (unsigned long)index);
    size_t j;
    for (j = 0; j < o->size; j++) {
        uint8_t expected = pattern(o->tag, j);
        CHECK(o->ptr[j] == expected,
              "slot %lu size %lu offset %lu: got %u expected %u",
              (unsigned long)index, (unsigned long)o->size,
              (unsigned long)j, (unsigned int)o->ptr[j],
              (unsigned int)expected);
    }
}

static void
verify_all(void) {
    size_t i;
    for (i = 0; i < TEST_SLOTS; i++) {
        if (objects[i].ptr != NULL) {
            verify_object(i);
        }
    }
}

static void
allocate_object(size_t index, size_t size, uint32_t tag) {
    CHECK(index < TEST_SLOTS, "test slot out of bounds");
    CHECK(objects[index].ptr == NULL, "slot %lu already live",
          (unsigned long)index);
    uint8_t *ptr = slub_alloc(size);
    CHECK(ptr != NULL, "allocation failed: slot %lu size %lu",
          (unsigned long)index, (unsigned long)size);
    uintptr_t start = (uintptr_t)ptr;
    CHECK(start % SLUB_ALIGN == 0, "unaligned slot %lu size %lu",
          (unsigned long)index, (unsigned long)size);
    CHECK(size <= (uintptr_t)-1 - start, "payload address overflow");

    /* Compare full requested ranges before writing the new payload. */
    size_t i;
    for (i = 0; i < TEST_SLOTS; i++) {
        if (objects[i].ptr != NULL) {
            uintptr_t other = (uintptr_t)objects[i].ptr;
            CHECK(start + size <= other || other + objects[i].size <= start,
                  "overlap: new slot %lu size %lu and live slot %lu size %lu",
                  (unsigned long)index, (unsigned long)size,
                  (unsigned long)i, (unsigned long)objects[i].size);
        }
    }
    objects[index].ptr = ptr;
    objects[index].size = size;
    objects[index].tag = tag;
    for (i = 0; i < size; i++) {
        ptr[i] = pattern(tag, i);
    }
}

static void
release_object(size_t index) {
    verify_object(index);
    slub_free(objects[index].ptr);
    objects[index].ptr = NULL;
}

static void
check_pages(size_t expected) {
    size_t actual = nr_free_pages();
    CHECK(actual == expected, "free pages: got %lu expected %lu",
          (unsigned long)actual, (unsigned long)expected);
}

static void
finish_group(size_t baseline) {
    size_t i;
    for (i = 0; i < TEST_SLOTS; i++) {
        CHECK(objects[i].ptr == NULL, "slot %lu leaked by test",
              (unsigned long)i);
    }
    check_pages(baseline);
    cprintf("[SLUB Test] round %u %s PASS\n", test_round, test_group);
}

static void
test_init_and_empty(void) {
    test_group = "initialization and empty operations";
    size_t baseline = nr_free_pages();
    slub_init();
    CHECK(slub_alloc(0) == NULL, "zero-size allocation must return NULL");
    slub_free(NULL);
    check_pages(baseline);

    allocate_object(0, 37, 1);
    size_t held = nr_free_pages();
    slub_init();
    check_pages(held);
    verify_all();
    allocate_object(1, 100, 2);
    verify_all();
    release_object(0);
    verify_all();
    release_object(1);
    finish_group(baseline);
}

static void
test_size_boundaries(void) {
    static const size_t sizes[] = {
        1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65,
        127, 128, 129, 255, 256, 257, 511, 512, 513,
        1023, 1024, 1025, 2047, 2048, 2049,
        PGSIZE - 1, PGSIZE, PGSIZE + 1, 2 * PGSIZE, 2 * PGSIZE + 1,
        3, 37, 100, 777, 3000, 6000,
    };
    test_group = "size boundaries (39 sizes)";
    size_t baseline = nr_free_pages();
    size_t i;
    for (i = 0; i < ARRAY_SIZE(sizes); i++) {
        allocate_object(0, sizes[i], 100 + i);
        release_object(0); /* Verifies every requested byte before freeing. */
        check_pages(baseline);
    }
    finish_group(baseline);
}

static void
test_object_isolation(void) {
    static const size_t sizes[] = {
        8, 8, 9, 15, 16, 33, 37, 100,
        100, 777, 1025, 2048, 2049, 4097, 8193, 6000,
    };
    test_group = "live object isolation and unordered frees";
    size_t baseline = nr_free_pages();
    size_t i;
    for (i = 0; i < ARRAY_SIZE(sizes); i++) {
        allocate_object(i, sizes[i], 200 + i);
    }
    verify_all();
    /* Multiplication by 5 permutes the 16 slots. */
    for (i = 0; i < ARRAY_SIZE(sizes) / 2; i++) {
        release_object((5 * i + 3) % ARRAY_SIZE(sizes));
        verify_all();
    }
    for (i = 0; i < ARRAY_SIZE(sizes); i++) {
        if (objects[i].ptr == NULL) {
            allocate_object(i, sizes[ARRAY_SIZE(sizes) - 1 - i], 300 + i);
            verify_all();
        }
    }
    for (i = 0; i < ARRAY_SIZE(sizes); i++) {
        release_object((5 * i + 3) % ARRAY_SIZE(sizes));
        verify_all();
    }
    finish_group(baseline);
}

static uintptr_t
object_page(size_t index) {
    return ROUNDDOWN((uintptr_t)objects[index].ptr, PGSIZE);
}

static void
test_slab_transitions(void) {
    test_group = "slab expansion, full/partial reuse and reclaim";
    size_t baseline = nr_free_pages();
    size_t i;
    for (i = 0; i < TEST_SLOTS; i++) {
        refill[i] = 0;
        allocate_object(i, SLUB_ALIGN, 400 + i);
    }
    CHECK(nr_free_pages() < baseline && baseline - nr_free_pages() >= 2,
          "small allocations did not exercise multiple slab pages");
    verify_all();

    /* Retain only the first slab, which was filled before expansion.
     * Discover membership from returned addresses, without reading headers.
     */
    uintptr_t first_page = object_page(0);
    size_t members = 0, probe = TEST_SLOTS;
    for (i = 0; i < TEST_SLOTS; i++) {
        if (object_page(i) == first_page) {
            members++;
        } else {
            release_object(i);
            probe = i;
            verify_all();
        }
    }
    CHECK(members > 2 && probe < TEST_SLOTS, "cannot construct full-slab test");
    check_pages(baseline - 1);

    /* Confirm fullness: one more object must require another page. */
    allocate_object(probe, SLUB_ALIGN, 1000);
    CHECK(object_page(probe) != first_page, "first slab was not full");
    check_pages(baseline - 2);
    release_object(probe);
    check_pages(baseline - 1);
    verify_all();

    release_object(0);
    check_pages(baseline - 1); /* Other objects keep this slab alive. */
    verify_all();
    allocate_object(0, SLUB_ALIGN, 1001);
    check_pages(baseline - 1); /* Reuse the full -> partial slab. */
    verify_all();

    for (i = 1; i < TEST_SLOTS; i += 2) {
        if (objects[i].ptr != NULL) {
            release_object(i);
            refill[i] = 1;
            check_pages(baseline - 1); /* Slot 0 remains a live anchor. */
            verify_all();
        }
    }
    for (i = 0; i < TEST_SLOTS; i++) {
        if (refill[i]) {
            allocate_object(i, SLUB_ALIGN, 1100 + i);
            check_pages(baseline - 1);
            verify_all();
        }
    }
    cprintf("[SLUB Test] first full slab contained %lu objects\n",
            (unsigned long)members);
    /* Keep slot 0 until last to check the exact last-object transition. */
    for (i = 1; i < TEST_SLOTS; i++) {
        if (objects[i].ptr != NULL) {
            release_object(i);
            check_pages(baseline - 1);
            verify_all();
        }
    }
    release_object(0);
    finish_group(baseline);
}

static void
test_large_objects(void) {
    static const size_t sizes[] = {2049, 4096, 4097, 8193, 16391};
    static const size_t order[] = {2, 0, 4, 1, 3};
    static size_t consumed[ARRAY_SIZE(sizes)];
    test_group = "large objects and complete page return";
    size_t baseline = nr_free_pages();
    size_t i;
    for (i = 0; i < ARRAY_SIZE(sizes); i++) {
        size_t before = nr_free_pages();
        allocate_object(i, sizes[i], 1700 + i);
        CHECK(nr_free_pages() < before, "large object consumed no pages");
        consumed[i] = before - nr_free_pages();
        CHECK(consumed[i] >= sizes[i] / PGSIZE + (sizes[i] % PGSIZE != 0),
              "large slot %lu has insufficient backing pages", (unsigned long)i);
        verify_all();
    }
    for (i = 0; i < ARRAY_SIZE(order); i++) {
        size_t index = order[i];
        size_t before = nr_free_pages();
        release_object(index);
        check_pages(before + consumed[index]);
        verify_all();
    }
    finish_group(baseline);
}

static void
expect_failure(size_t size) {
    size_t before = nr_free_pages();
    CHECK(slub_alloc(size) == NULL, "request %lu should fail", (unsigned long)size);
    check_pages(before);
    verify_all();
}

static void
test_allocation_failures(void) {
    test_group = "overflow, resource shortage and recovery";
    size_t baseline = nr_free_pages();
    allocate_object(0, 37, 1800);
    allocate_object(1, 8193, 1801);
    verify_all();
    expect_failure((size_t)-1);
    expect_failure((size_t)-1 - 7);
    expect_failure((size_t)-1 - PGSIZE);

    size_t available = nr_free_pages();
    CHECK(available < (size_t)-1 / PGSIZE, "test request would overflow");
    size_t impossible = (available + 1) * PGSIZE;
    CHECK(impossible > SLUB_SMALL_MAX, "test request must take large path");
    expect_failure(impossible);

    allocate_object(2, 100, 1802);
    allocate_object(3, 4097, 1803);
    verify_all();
    release_object(1);
    verify_all();
    release_object(2);
    verify_all();
    release_object(0);
    verify_all();
    release_object(3);
    finish_group(baseline);
}

static uint32_t
next_random(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void
test_mixed_stress(void) {
    static const size_t sizes[] = {
        1, 7, 8, 9, 16, 17, 37, 100, 255, 257, 511, 513,
        777, 1024, 1025, 2048, 2049, 3000, 4096, 4097, 6000, 8193, 16391,
    };
    test_group = "deterministic mixed stress (1024 operations)";
    size_t baseline = nr_free_pages();
    uint32_t state = 0x13579BDFU;
    size_t step, allocations = 0, releases = 0;
    for (step = 0; step < STRESS_STEPS; step++) {
        size_t index = next_random(&state) % STRESS_SLOTS;
        if (objects[index].ptr != NULL) {
            release_object(index);
            releases++;
        } else {
            size_t size = sizes[next_random(&state) % ARRAY_SIZE(sizes)];
            allocate_object(index, size, 2000 + step);
            allocations++;
        }
        if (step % 16 == 0) {
            verify_all();
        }
    }
    verify_all();
    for (step = 0; step < STRESS_SLOTS; step++) {
        if (objects[step].ptr != NULL) {
            release_object(step);
            verify_all();
        }
    }
    CHECK(allocations >= 100 && releases >= 100, "insufficient mixed operations");
    cprintf("[SLUB Test] stress: %lu allocations, %lu frees before cleanup\n",
            (unsigned long)allocations, (unsigned long)releases);
    finish_group(baseline);
}

void
slub_test(void) {
    size_t baseline = nr_free_pages();
    for (test_round = 1; test_round <= 2; test_round++) {
        test_init_and_empty();
        test_size_boundaries();
        test_object_isolation();
        test_slab_transitions();
        test_large_objects();
        test_allocation_failures();
        test_mixed_stress();
        check_pages(baseline);
    }
    cprintf("[SLUB Test] ALL PASS (2 rounds, free pages restored to %lu)\n",
            (unsigned long)baseline);
}
