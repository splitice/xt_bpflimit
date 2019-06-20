/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _XT_BPFLIMIT_H
#define _XT_BPFLIMIT_H

/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_XT_BPFLIMIT_H
#define _UAPI_XT_BPFLIMIT_H

#include <linux/types.h>
#include <linux/limits.h>
#include <linux/if.h>

/* timings are in milliseconds. */
#define XT_BPFLIMIT_SCALE 10000
#define XT_BPFLIMIT_SCALE_v2 1000000llu
/* 1/10,000 sec period => max of 10,000/sec.  Min rate is then 429490
 * seconds, or one packet every 59 hours.
 */
 
 
#if LINUX_VERSION_CODE <= KERNEL_VERSION(5,0,0)

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#endif

/* packet length accounting is done in 16-byte steps */
#define XT_BPFLIMIT_BYTE_SHIFT 4

/* details of this structure hidden by the implementation */
struct xt_bpflimit_htable;

enum {
	XT_BPFLIMIT_HASH_DIP		= 1 << 0,
	XT_BPFLIMIT_HASH_DPT		= 1 << 1,
	XT_BPFLIMIT_HASH_SIP		= 1 << 2,
	XT_BPFLIMIT_HASH_SPT		= 1 << 3,
	XT_BPFLIMIT_INVERT		= 1 << 4,
	XT_BPFLIMIT_BYTES		= 1 << 5,
	XT_BPFLIMIT_RATE_MATCH		= 1 << 6,
};

struct bpflimit_cfg {
	__u32 mode;	  /* bitmask of XT_BPFLIMIT_HASH_* */
	__u32 avg;    /* Average secs between packets * scale */
	__u32 burst;  /* Period multiplier for upper limit. */

	/* user specified */
	__u32 size;		/* how many buckets */
	__u32 max;		/* max number of entries */
	__u32 gc_interval;	/* gc interval */
	__u32 expire;	/* when do entries expire? */
};

struct xt_bpflimit_info {
	char name [IFNAMSIZ];		/* name */
	struct bpflimit_cfg cfg;

	/* Used internally by the kernel */
	struct xt_bpflimit_htable *hinfo;
	union {
		void *ptr;
		struct xt_bpflimit_info *master;
	} u;
};

struct bpflimit_cfg1 {
	__u32 mode;	  /* bitmask of XT_BPFLIMIT_HASH_* */
	__u32 avg;    /* Average secs between packets * scale */
	__u32 burst;  /* Period multiplier for upper limit. */

	/* user specified */
	__u32 size;		/* how many buckets */
	__u32 max;		/* max number of entries */
	__u32 gc_interval;	/* gc interval */
	__u32 expire;	/* when do entries expire? */

	__u8 srcmask, dstmask;
};

struct bpflimit_cfg2 {
	__u64 avg;		/* Average secs between packets * scale */
	__u64 burst;		/* Period multiplier for upper limit. */
	__u32 mode;		/* bitmask of XT_BPFLIMIT_HASH_* */

	/* user specified */
	__u32 size;		/* how many buckets */
	__u32 max;		/* max number of entries */
	__u32 gc_interval;	/* gc interval */
	__u32 expire;		/* when do entries expire? */

	__u8 srcmask, dstmask;
};

struct bpflimit_cfg3 {
	__u64 avg;		/* Average secs between packets * scale */
	__u64 burst;		/* Period multiplier for upper limit. */
	__u32 mode;		/* bitmask of XT_BPFLIMIT_HASH_* */

	/* user specified */
	__u32 size;		/* how many buckets */
	__u32 max;		/* max number of entries */
	__u32 gc_interval;	/* gc interval */
	__u32 expire;		/* when do entries expire? */

	__u32 interval;
	__u8 srcmask, dstmask;
};

struct xt_bpflimit_mtinfo1 {
	char name[IFNAMSIZ];
	struct bpflimit_cfg1 cfg;

	/* Used internally by the kernel */
	struct xt_bpflimit_htable *hinfo __attribute__((aligned(8)));
};

struct xt_bpflimit_mtinfo2 {
	char name[NAME_MAX];
	struct bpflimit_cfg2 cfg;

	/* Used internally by the kernel */
	struct xt_bpflimit_htable *hinfo __attribute__((aligned(8)));
};

struct xt_bpflimit_mtinfo3 {
	char name[NAME_MAX];
	struct bpflimit_cfg3 cfg;

	/* Used internally by the kernel */
	struct xt_bpflimit_htable *hinfo __attribute__((aligned(8)));
};

#endif /* _UAPI_XT_BPFLIMIT_H */

#define XT_BPFLIMIT_ALL (XT_BPFLIMIT_HASH_DIP | XT_BPFLIMIT_HASH_DPT | \
			  XT_BPFLIMIT_HASH_SIP | XT_BPFLIMIT_HASH_SPT | \
			  XT_BPFLIMIT_INVERT | XT_BPFLIMIT_BYTES |\
			  XT_BPFLIMIT_RATE_MATCH)
#endif /*_XT_BPFLIMIT_H*/