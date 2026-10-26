#ifndef __KERN_MM_SLUB_TEST_H__
#define __KERN_MM_SLUB_TEST_H__

/* Run after pmm_init() and slub_init(), with no other live SLUB objects. */
void slub_test(void);

#endif /* !__KERN_MM_SLUB_TEST_H__ */
