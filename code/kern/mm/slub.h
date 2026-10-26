#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__

#include <defs.h>

#define SLUB_ALIGN 8
#define SLUB_SMALL_MAX 2048

/* Single-core, non-concurrent API. Call after pmm_init(); safe to repeat. */
void slub_init(void);

/* Return an aligned payload of at least size bytes, or NULL on failure/zero.
 * Payload contents are unspecified. Requests above SLUB_SMALL_MAX use pages.
 */
void *slub_alloc(size_t size);

/* NULL is accepted. Otherwise ptr must be a live allocation's start address;
 * invalid pointers, interior pointers and double frees are unsupported.
 * Empty slabs and large allocations are immediately returned to the PMM.
 */
void slub_free(void *ptr);

#endif /* !__KERN_MM_SLUB_H__ */
