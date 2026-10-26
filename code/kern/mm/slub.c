#include <slub.h>
#include <pmm.h>
#include <list.h>
#include <stdio.h>

#define SLUB_CACHE_COUNT 9

struct slub_cache {
    size_t object_size;             /* Payload capacity; also the slot stride. */
    list_entry_t slabs;             /* Includes both partial and full slabs. */
};

/* Stored inside the allocated page, never in the PMM's struct Page.
 * Large allocations use the same header with cache == NULL.
 */
struct slub_page {
    struct Page *base;
    struct slub_cache *cache;
    size_t nr_pages;
    size_t inuse;
    size_t capacity;
    void *freelist;
    list_entry_t link;
};

#define SLUB_HEADER_SIZE \
    ((sizeof(struct slub_page) + SLUB_ALIGN - 1) / SLUB_ALIGN * SLUB_ALIGN)

static struct slub_cache caches[SLUB_CACHE_COUNT];
static bool initialized;

static struct slub_page *
new_slab(struct slub_cache *cache) {
    struct Page *page = alloc_page();
    if (page == NULL) {
        return NULL;
    }

    struct slub_page *slab =
        (struct slub_page *)(page2pa(page) + va_pa_offset);
    slab->base = page;
    slab->cache = cache;
    slab->nr_pages = 1;
    slab->inuse = 0;
    slab->capacity = (PGSIZE - SLUB_HEADER_SIZE) / cache->object_size;
    slab->freelist = NULL;

    /* Link slots only while free: allocated payload has no object header. */
    size_t i;
    for (i = slab->capacity; i > 0; i--) {
        void *slot = (char *)slab + SLUB_HEADER_SIZE +
                     (i - 1) * cache->object_size;
        *(void **)slot = slab->freelist;
        slab->freelist = slot;
    }
    list_add(&cache->slabs, &slab->link);
    return slab;
}

static void *
alloc_small(struct slub_cache *cache) {
    struct slub_page *slab = NULL;
    list_entry_t *le;
    for (le = list_next(&cache->slabs); le != &cache->slabs;
         le = list_next(le)) {
        struct slub_page *candidate = to_struct(le, struct slub_page, link);
        if (candidate->freelist != NULL) {
            slab = candidate;
            break;
        }
    }
    if (slab == NULL) {
        slab = new_slab(cache);
        if (slab == NULL) {
            return NULL;
        }
    }

    void *ptr = slab->freelist;
    slab->freelist = *(void **)ptr;
    slab->inuse++;
    return ptr;
}

static void *
alloc_large(size_t size) {
    /* Check addition before adding the header. Division/remainder avoids
     * overflowing total + PGSIZE - 1 when rounding up to whole pages.
     */
    if (size > (size_t)-1 - SLUB_HEADER_SIZE) {
        return NULL;
    }
    size_t total = size + SLUB_HEADER_SIZE;
    size_t count = total / PGSIZE + (total % PGSIZE != 0);
    if (count > npage - nbase || count > nr_free_pages()) {
        return NULL;
    }

    struct Page *page = alloc_pages(count);
    if (page == NULL) {
        return NULL;
    }
    struct slub_page *block =
        (struct slub_page *)(page2pa(page) + va_pa_offset);
    block->base = page;
    block->cache = NULL;
    block->nr_pages = count;
    block->inuse = 1;
    block->capacity = 1;
    block->freelist = NULL;
    list_init(&block->link);
    return (char *)block + SLUB_HEADER_SIZE;
}

void
slub_init(void) {
    if (initialized) {
        return;
    }
    /* Check the fixed header layout, alignment and largest slot at compile time. */
    static_assert(SLUB_ALIGN >= sizeof(void *));
    static_assert(SLUB_HEADER_SIZE % SLUB_ALIGN == 0);
    static_assert(SLUB_HEADER_SIZE + SLUB_SMALL_MAX <= PGSIZE);
    static_assert((SLUB_ALIGN << (SLUB_CACHE_COUNT - 1)) == SLUB_SMALL_MAX);

    size_t i;
    for (i = 0; i < SLUB_CACHE_COUNT; i++) {
        caches[i].object_size = (size_t)SLUB_ALIGN << i;
        list_init(&caches[i].slabs);
    }
    initialized = 1;
    cprintf("slub_init() succeeded! (9 size classes, 8-byte alignment)\n");
}

void *
slub_alloc(size_t size) {
    assert(initialized);
    if (size == 0) {
        return NULL;
    }
    if (size > SLUB_SMALL_MAX) {
        return alloc_large(size);
    }

    size_t i = 0;
    while (caches[i].object_size < size) {
        i++;
    }
    return alloc_small(&caches[i]);
}

void
slub_free(void *ptr) {
    assert(initialized);
    if (ptr == NULL) {
        return;
    }

    /* Small payloads stay in their slab page. Large payloads start directly
     * after the header, also in the first page of their allocation.
     */
    struct slub_page *slab =
        (struct slub_page *)ROUNDDOWN((uintptr_t)ptr, PGSIZE);
    struct Page *base = slab->base;
    if (slab->cache == NULL) {
        size_t count = slab->nr_pages;
        assert(ptr == (char *)slab + SLUB_HEADER_SIZE);
        free_pages(base, count);
        return;
    }

    assert(slab->inuse > 0 && slab->inuse <= slab->capacity);
    *(void **)ptr = slab->freelist;
    slab->freelist = ptr;
    slab->inuse--;
    if (slab->inuse == 0) {
        /* Unlink before returning the page; never access slab afterwards. */
        list_del(&slab->link);
        free_page(base);
    }
}
