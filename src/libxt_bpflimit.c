/* ip6tables match extension for limiting packets per destination
 *
 * (C) 2003-2004 by Harald Welte <laforge@netfilter.org>
 *
 * Development of this code was funded by Astaro AG, http://www.astaro.com/
 *
 * Based on ipt_limit.c by
 * Jérôme de Vivie   <devivie@info.enserb.u-bordeaux.fr>
 * Hervé Eychenne    <rv@wallfire.org>
 * 
 * Error corections by nmalykh@bilim.com (22.01.2005)
 */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xtables.h>
#include <linux/netfilter/x_tables.h>
#include "xt_bpflimit.h"

#define XT_BPFLIMIT_BURST	5
#define XT_BPFLIMIT_BURST_MAX	10000

#define XT_BPFLIMIT_BYTE_EXPIRE	15
#define XT_BPFLIMIT_BYTE_EXPIRE_BURST	60

/* miliseconds */
#define XT_BPFLIMIT_GCINTERVAL	1000

struct bpflimit_mt_udata {
	uint32_t mult;
};

static void bpflimit_help(void)
{
	printf(
"bpflimit match options:\n"
"--bpflimit <avg>		max average match rate\n"
"                                [Packets per second unless followed by \n"
"                                /sec /minute /hour /day postfixes]\n"
"--bpflimit-mode <mode>		mode is a comma-separated list of\n"
"					dstip,srcip,dstport,srcport\n"
"--bpflimit-name <name>		name for /proc/net/ipt_bpflimit/\n"
"[--bpflimit-burst <num>]	number to match in a burst, default %u\n"
"[--bpflimit-htable-size <num>]	number of hashtable buckets\n"
"[--bpflimit-htable-max <num>]	number of hashtable entries\n"
"[--bpflimit-htable-gcinterval]	interval between garbage collection runs\n"
"[--bpflimit-htable-expire]	after which time are idle entries expired?\n",
XT_BPFLIMIT_BURST);
}

enum {
	O_UPTO = 0,
	O_ABOVE,
	O_LIMIT,
	O_MODE,
	O_SRCMASK,
	O_DSTMASK,
	O_NAME,
	O_BURST,
	O_HTABLE_SIZE,
	O_HTABLE_MAX,
	O_HTABLE_GCINT,
	O_HTABLE_EXPIRE,
	F_BURST         = 1 << O_BURST,
	F_UPTO          = 1 << O_UPTO,
	F_ABOVE         = 1 << O_ABOVE,
	F_HTABLE_EXPIRE = 1 << O_HTABLE_EXPIRE,
};

static void bpflimit_mt_help(void)
{
	printf(
"bpflimit match options:\n"
"  --bpflimit-upto <avg>           max average match rate\n"
"                                   [Packets per second unless followed by \n"
"                                   /sec /minute /hour /day postfixes]\n"
"  --bpflimit-above <avg>          min average match rate\n"
"  --bpflimit-mode <mode>          mode is a comma-separated list of\n"
"                                   dstip,srcip,dstport,srcport (or none)\n"
"  --bpflimit-srcmask <length>     source address grouping prefix length\n"
"  --bpflimit-dstmask <length>     destination address grouping prefix length\n"
"  --bpflimit-name <name>          name for /proc/net/ipt_bpflimit\n"
"  --bpflimit-burst <num>	    number to match in a burst, default %u\n"
"  --bpflimit-htable-size <num>    number of hashtable buckets\n"
"  --bpflimit-htable-max <num>     number of hashtable entries\n"
"  --bpflimit-htable-gcinterval    interval between garbage collection runs\n"
"  --bpflimit-htable-expire        after which time are idle entries expired?\n"
"\n", XT_BPFLIMIT_BURST);
}

#define s struct xt_bpflimit_info
static const struct xt_option_entry bpflimit_opts[] = {
	{.name = "bpflimit", .id = O_UPTO, .excl = F_ABOVE,
	 .type = XTTYPE_STRING},
	{.name = "bpflimit-burst", .id = O_BURST, .type = XTTYPE_UINT32,
	 .min = 1, .max = XT_BPFLIMIT_BURST_MAX, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.burst)},
	{.name = "bpflimit-htable-size", .id = O_HTABLE_SIZE,
	 .type = XTTYPE_UINT32, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.size)},
	{.name = "bpflimit-htable-max", .id = O_HTABLE_MAX,
	 .type = XTTYPE_UINT32, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.max)},
	{.name = "bpflimit-htable-gcinterval", .id = O_HTABLE_GCINT,
	 .type = XTTYPE_UINT32, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.gc_interval)},
	{.name = "bpflimit-htable-expire", .id = O_HTABLE_EXPIRE,
	 .type = XTTYPE_UINT32, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.expire)},
	{.name = "bpflimit-mode", .id = O_MODE, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND},
	{.name = "bpflimit-name", .id = O_NAME, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND | XTOPT_PUT, XTOPT_POINTER(s, name), .min = 1},
	XTOPT_TABLEEND,
};
#undef s

#define s struct xt_bpflimit_mtinfo1
static const struct xt_option_entry bpflimit_mt_opts[] = {
	{.name = "bpflimit-upto", .id = O_UPTO, .excl = F_ABOVE,
	 .type = XTTYPE_STRING, .flags = XTOPT_INVERT},
	{.name = "bpflimit-above", .id = O_ABOVE, .excl = F_UPTO,
	 .type = XTTYPE_STRING, .flags = XTOPT_INVERT},
	{.name = "bpflimit", .id = O_UPTO, .excl = F_ABOVE,
	 .type = XTTYPE_STRING, .flags = XTOPT_INVERT}, /* old name */
	{.name = "bpflimit-srcmask", .id = O_SRCMASK, .type = XTTYPE_PLEN},
	{.name = "bpflimit-dstmask", .id = O_DSTMASK, .type = XTTYPE_PLEN},
	{.name = "bpflimit-burst", .id = O_BURST, .type = XTTYPE_STRING},
	{.name = "bpflimit-htable-size", .id = O_HTABLE_SIZE,
	 .type = XTTYPE_UINT32, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.size)},
	{.name = "bpflimit-htable-max", .id = O_HTABLE_MAX,
	 .type = XTTYPE_UINT32, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.max)},
	{.name = "bpflimit-htable-gcinterval", .id = O_HTABLE_GCINT,
	 .type = XTTYPE_UINT32, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.gc_interval)},
	{.name = "bpflimit-htable-expire", .id = O_HTABLE_EXPIRE,
	 .type = XTTYPE_UINT32, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.expire)},
	{.name = "bpflimit-mode", .id = O_MODE, .type = XTTYPE_STRING},
	{.name = "bpflimit-name", .id = O_NAME, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND | XTOPT_PUT, XTOPT_POINTER(s, name), .min = 1},
	XTOPT_TABLEEND,
};
#undef s

static uint32_t cost_to_bytes(uint32_t cost)
{
	uint32_t r;

	r = cost ? UINT32_MAX / cost : UINT32_MAX;
	r = (r - 1) << XT_BPFLIMIT_BYTE_SHIFT;
	return r;
}

static uint64_t bytes_to_cost(uint32_t bytes)
{
	uint32_t r = bytes >> XT_BPFLIMIT_BYTE_SHIFT;
	return UINT32_MAX / (r+1);
}

static uint32_t get_factor(int chr)
{
	switch (chr) {
	case 'm': return 1024 * 1024;
	case 'k': return 1024;
	}
	return 1;
}

static void burst_error(void)
{
	xtables_error(PARAMETER_PROBLEM, "bad value for option "
			"\"--bpflimit-burst\", or out of range (1-%u).", XT_BPFLIMIT_BURST_MAX);
}

static uint32_t parse_burst(const char *burst, struct xt_bpflimit_mtinfo1 *info)
{
	uintmax_t v;
	char *end;

	if (!xtables_strtoul(burst, &end, &v, 1, UINT32_MAX) ||
	    (*end == 0 && v > XT_BPFLIMIT_BURST_MAX))
		burst_error();

	v *= get_factor(*end);
	if (v > UINT32_MAX)
		xtables_error(PARAMETER_PROBLEM, "bad value for option "
			"\"--bpflimit-burst\", value \"%s\" too large "
				"(max %umb).", burst, UINT32_MAX/1024/1024);
	return v;
}

static bool parse_bytes(const char *rate, uint32_t *val, struct bpflimit_mt_udata *ud)
{
	unsigned int factor = 1;
	uint64_t tmp;
	int r;
	const char *mode = strstr(rate, "b/s");
	if (!mode || mode == rate)
		return false;

	mode--;
	r = atoi(rate);
	if (r == 0)
		return false;

	factor = get_factor(*mode);
	tmp = (uint64_t) r * factor;
	if (tmp > UINT32_MAX)
		xtables_error(PARAMETER_PROBLEM,
			"Rate value too large \"%llu\" (max %u)\n",
					(unsigned long long)tmp, UINT32_MAX);

	*val = bytes_to_cost(tmp);
	if (*val == 0)
		xtables_error(PARAMETER_PROBLEM, "Rate too high \"%s\"\n", rate);

	ud->mult = XT_BPFLIMIT_BYTE_EXPIRE;
	return true;
}

static
int parse_rate(const char *rate, uint32_t *val, struct bpflimit_mt_udata *ud)
{
	const char *delim;
	uint32_t r;

	ud->mult = 1;  /* Seconds by default. */
	delim = strchr(rate, '/');
	if (delim) {
		if (strlen(delim+1) == 0)
			return 0;

		if (strncasecmp(delim+1, "second", strlen(delim+1)) == 0)
			ud->mult = 1;
		else if (strncasecmp(delim+1, "minute", strlen(delim+1)) == 0)
			ud->mult = 60;
		else if (strncasecmp(delim+1, "hour", strlen(delim+1)) == 0)
			ud->mult = 60*60;
		else if (strncasecmp(delim+1, "day", strlen(delim+1)) == 0)
			ud->mult = 24*60*60;
		else
			return 0;
	}
	r = atoi(rate);
	if (!r)
		return 0;

	*val = XT_BPFLIMIT_SCALE * ud->mult / r;
	if (*val == 0)
		/*
		 * The rate maps to infinity. (1/day is the minimum they can
		 * specify, so we are ok at that end).
		 */
		xtables_error(PARAMETER_PROBLEM, "Rate too fast \"%s\"\n", rate);
	return 1;
}

static void bpflimit_init(struct xt_entry_match *m)
{
	struct xt_bpflimit_info *r = (struct xt_bpflimit_info *)m->data;

	r->cfg.burst = XT_BPFLIMIT_BURST;
	r->cfg.gc_interval = XT_BPFLIMIT_GCINTERVAL;

}

static void bpflimit_mt4_init(struct xt_entry_match *match)
{
	struct xt_bpflimit_mtinfo1 *info = (void *)match->data;

	info->cfg.mode        = 0;
	info->cfg.burst       = XT_BPFLIMIT_BURST;
	info->cfg.gc_interval = XT_BPFLIMIT_GCINTERVAL;
	info->cfg.srcmask     = 32;
	info->cfg.dstmask     = 32;
}

static void bpflimit_mt6_init(struct xt_entry_match *match)
{
	struct xt_bpflimit_mtinfo1 *info = (void *)match->data;

	info->cfg.mode        = 0;
	info->cfg.burst       = XT_BPFLIMIT_BURST;
	info->cfg.gc_interval = XT_BPFLIMIT_GCINTERVAL;
	info->cfg.srcmask     = 128;
	info->cfg.dstmask     = 128;
}

/* Parse a 'mode' parameter into the required bitmask */
static int parse_mode(uint32_t *mode, const char *option_arg)
{
	char *tok;
	char *arg = strdup(option_arg);

	if (!arg)
		return -1;

	for (tok = strtok(arg, ",|");
	     tok;
	     tok = strtok(NULL, ",|")) {
		if (!strcmp(tok, "dstip"))
			*mode |= XT_BPFLIMIT_HASH_DIP;
		else if (!strcmp(tok, "srcip"))
			*mode |= XT_BPFLIMIT_HASH_SIP;
		else if (!strcmp(tok, "srcport"))
			*mode |= XT_BPFLIMIT_HASH_SPT;
		else if (!strcmp(tok, "dstport"))
			*mode |= XT_BPFLIMIT_HASH_DPT;
		else {
			free(arg);
			return -1;
		}
	}
	free(arg);
	return 0;
}

static void bpflimit_parse(struct xt_option_call *cb)
{
	struct xt_bpflimit_info *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_UPTO:
		if (!parse_rate(cb->arg, &info->cfg.avg, cb->udata))
			xtables_param_act(XTF_BAD_VALUE, "bpflimit",
			          "--bpflimit-upto", cb->arg);
		break;
	case O_MODE:
		if (parse_mode(&info->cfg.mode, cb->arg) < 0)
			xtables_param_act(XTF_BAD_VALUE, "bpflimit",
			          "--bpflimit-mode", cb->arg);
		break;
	}
}

static void bpflimit_mt_parse(struct xt_option_call *cb)
{
	struct xt_bpflimit_mtinfo1 *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_BURST:
		info->cfg.burst = parse_burst(cb->arg, info);
		break;
	case O_UPTO:
		if (cb->invert)
			info->cfg.mode |= XT_BPFLIMIT_INVERT;
		if (parse_bytes(cb->arg, &info->cfg.avg, cb->udata))
			info->cfg.mode |= XT_BPFLIMIT_BYTES;
		else if (!parse_rate(cb->arg, &info->cfg.avg, cb->udata))
			xtables_param_act(XTF_BAD_VALUE, "bpflimit",
			          "--bpflimit-upto", cb->arg);
		break;
	case O_ABOVE:
		if (!cb->invert)
			info->cfg.mode |= XT_BPFLIMIT_INVERT;
		if (parse_bytes(cb->arg, &info->cfg.avg, cb->udata))
			info->cfg.mode |= XT_BPFLIMIT_BYTES;
		else if (!parse_rate(cb->arg, &info->cfg.avg, cb->udata))
			xtables_param_act(XTF_BAD_VALUE, "bpflimit",
			          "--bpflimit-above", cb->arg);
		break;
	case O_MODE:
		if (parse_mode(&info->cfg.mode, cb->arg) < 0)
			xtables_param_act(XTF_BAD_VALUE, "bpflimit",
			          "--bpflimit-mode", cb->arg);
		break;
	case O_SRCMASK:
		info->cfg.srcmask = cb->val.hlen;
		break;
	case O_DSTMASK:
		info->cfg.dstmask = cb->val.hlen;
		break;
	}
}

static void bpflimit_check(struct xt_fcheck_call *cb)
{
	const struct bpflimit_mt_udata *udata = cb->udata;
	struct xt_bpflimit_info *info = cb->data;

	if (!(cb->xflags & (F_UPTO | F_ABOVE)))
		xtables_error(PARAMETER_PROBLEM,
				"You have to specify --bpflimit");
	if (!(cb->xflags & F_HTABLE_EXPIRE))
		info->cfg.expire = udata->mult * 1000; /* from s to msec */
}

static void bpflimit_mt_check(struct xt_fcheck_call *cb)
{
	const struct bpflimit_mt_udata *udata = cb->udata;
	struct xt_bpflimit_mtinfo1 *info = cb->data;

	if (!(cb->xflags & (F_UPTO | F_ABOVE)))
		xtables_error(PARAMETER_PROBLEM,
				"You have to specify --bpflimit");
	if (!(cb->xflags & F_HTABLE_EXPIRE))
		info->cfg.expire = udata->mult * 1000; /* from s to msec */

	if (info->cfg.mode & XT_BPFLIMIT_BYTES) {
		uint32_t burst = 0;
		if (cb->xflags & F_BURST) {
			if (info->cfg.burst < cost_to_bytes(info->cfg.avg))
				xtables_error(PARAMETER_PROBLEM,
					"burst cannot be smaller than %ub", cost_to_bytes(info->cfg.avg));

			burst = info->cfg.burst;
			burst /= cost_to_bytes(info->cfg.avg);
			if (info->cfg.burst % cost_to_bytes(info->cfg.avg))
				burst++;
			if (!(cb->xflags & F_HTABLE_EXPIRE))
				info->cfg.expire = XT_BPFLIMIT_BYTE_EXPIRE_BURST * 1000;
		}
		info->cfg.burst = burst;
	} else if (info->cfg.burst > XT_BPFLIMIT_BURST_MAX)
		burst_error();
}

static const struct rates
{
	const char *name;
	uint32_t mult;
} rates[] = { { "day", XT_BPFLIMIT_SCALE*24*60*60 },
	      { "hour", XT_BPFLIMIT_SCALE*60*60 },
	      { "min", XT_BPFLIMIT_SCALE*60 },
	      { "sec", XT_BPFLIMIT_SCALE } };

static uint32_t print_rate(uint32_t period)
{
	unsigned int i;

	if (period == 0) {
		printf(" %f", INFINITY);
		return 0;
	}

	for (i = 1; i < ARRAY_SIZE(rates); ++i)
		if (period > rates[i].mult
            || rates[i].mult/period < rates[i].mult%period)
			break;

	printf(" %u/%s", rates[i-1].mult / period, rates[i-1].name);
	/* return in msec */
	return rates[i-1].mult / XT_BPFLIMIT_SCALE * 1000;
}

static const struct {
	const char *name;
	uint32_t thresh;
} units[] = {
	{ "m", 1024 * 1024 },
	{ "k", 1024 },
	{ "", 1 },
};

static uint32_t print_bytes(uint32_t avg, uint32_t burst, const char *prefix)
{
	unsigned int i;
	unsigned long long r;

	r = cost_to_bytes(avg);

	for (i = 0; i < ARRAY_SIZE(units) -1; ++i)
		if (r >= units[i].thresh &&
		    bytes_to_cost(r & ~(units[i].thresh - 1)) == avg)
			break;
	printf(" %llu%sb/s", r/units[i].thresh, units[i].name);

	if (burst == 0)
		return XT_BPFLIMIT_BYTE_EXPIRE * 1000;

	r *= burst;
	printf(" %s", prefix);
	for (i = 0; i < ARRAY_SIZE(units) -1; ++i)
		if (r >= units[i].thresh)
			break;

	printf("burst %llu%sb", r / units[i].thresh, units[i].name);
	return XT_BPFLIMIT_BYTE_EXPIRE_BURST * 1000;
}

static void print_mode(unsigned int mode, char separator)
{
	bool prevmode = false;

	putchar(' ');
	if (mode & XT_BPFLIMIT_HASH_SIP) {
		fputs("srcip", stdout);
		prevmode = 1;
	}
	if (mode & XT_BPFLIMIT_HASH_SPT) {
		if (prevmode)
			putchar(separator);
		fputs("srcport", stdout);
		prevmode = 1;
	}
	if (mode & XT_BPFLIMIT_HASH_DIP) {
		if (prevmode)
			putchar(separator);
		fputs("dstip", stdout);
		prevmode = 1;
	}
	if (mode & XT_BPFLIMIT_HASH_DPT) {
		if (prevmode)
			putchar(separator);
		fputs("dstport", stdout);
	}
}

static void bpflimit_print(const void *ip,
                            const struct xt_entry_match *match, int numeric)
{
	const struct xt_bpflimit_info *r = (const void *)match->data;
	uint32_t quantum;

	fputs(" limit: avg", stdout);
	quantum = print_rate(r->cfg.avg);
	printf(" burst %u", r->cfg.burst);
	fputs(" mode", stdout);
	print_mode(r->cfg.mode, '-');
	if (r->cfg.size)
		printf(" htable-size %u", r->cfg.size);
	if (r->cfg.max)
		printf(" htable-max %u", r->cfg.max);
	if (r->cfg.gc_interval != XT_BPFLIMIT_GCINTERVAL)
		printf(" htable-gcinterval %u", r->cfg.gc_interval);
	if (r->cfg.expire != quantum)
		printf(" htable-expire %u", r->cfg.expire);
}

static void
bpflimit_mt_print(const struct xt_bpflimit_mtinfo1 *info, unsigned int dmask)
{
	uint32_t quantum;

	if (info->cfg.mode & XT_BPFLIMIT_INVERT)
		fputs(" limit: above", stdout);
	else
		fputs(" limit: up to", stdout);

	if (info->cfg.mode & XT_BPFLIMIT_BYTES) {
		quantum = print_bytes(info->cfg.avg, info->cfg.burst, "");
	} else {
		quantum = print_rate(info->cfg.avg);
		printf(" burst %u", info->cfg.burst);
	}
	if (info->cfg.mode & (XT_BPFLIMIT_HASH_SIP | XT_BPFLIMIT_HASH_SPT |
	    XT_BPFLIMIT_HASH_DIP | XT_BPFLIMIT_HASH_DPT)) {
		fputs(" mode", stdout);
		print_mode(info->cfg.mode, '-');
	}
	if (info->cfg.size != 0)
		printf(" htable-size %u", info->cfg.size);
	if (info->cfg.max != 0)
		printf(" htable-max %u", info->cfg.max);
	if (info->cfg.gc_interval != XT_BPFLIMIT_GCINTERVAL)
		printf(" htable-gcinterval %u", info->cfg.gc_interval);
	if (info->cfg.expire != quantum)
		printf(" htable-expire %u", info->cfg.expire);

	if (info->cfg.srcmask != dmask)
		printf(" srcmask %u", info->cfg.srcmask);
	if (info->cfg.dstmask != dmask)
		printf(" dstmask %u", info->cfg.dstmask);
}

static void
bpflimit_mt4_print(const void *ip, const struct xt_entry_match *match,
                   int numeric)
{
	const struct xt_bpflimit_mtinfo1 *info = (const void *)match->data;

	bpflimit_mt_print(info, 32);
}

static void
bpflimit_mt6_print(const void *ip, const struct xt_entry_match *match,
                   int numeric)
{
	const struct xt_bpflimit_mtinfo1 *info = (const void *)match->data;

	bpflimit_mt_print(info, 128);
}

static void bpflimit_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_bpflimit_info *r = (const void *)match->data;
	uint32_t quantum;

	fputs(" --bpflimit", stdout);
	quantum = print_rate(r->cfg.avg);
	printf(" --bpflimit-burst %u", r->cfg.burst);

	fputs(" --bpflimit-mode", stdout);
	print_mode(r->cfg.mode, ',');

	printf(" --bpflimit-name %s", r->name);

	if (r->cfg.size)
		printf(" --bpflimit-htable-size %u", r->cfg.size);
	if (r->cfg.max)
		printf(" --bpflimit-htable-max %u", r->cfg.max);
	if (r->cfg.gc_interval != XT_BPFLIMIT_GCINTERVAL)
		printf(" --bpflimit-htable-gcinterval %u", r->cfg.gc_interval);
	if (r->cfg.expire != quantum)
		printf(" --bpflimit-htable-expire %u", r->cfg.expire);
}

static void
bpflimit_mt_save(const struct xt_bpflimit_mtinfo1 *info, unsigned int dmask)
{
	uint32_t quantum;

	if (info->cfg.mode & XT_BPFLIMIT_INVERT)
		fputs(" --bpflimit-above", stdout);
	else
		fputs(" --bpflimit-upto", stdout);

	if (info->cfg.mode & XT_BPFLIMIT_BYTES) {
		quantum = print_bytes(info->cfg.avg, info->cfg.burst, "--bpflimit-");
	} else {
		quantum = print_rate(info->cfg.avg);
		printf(" --bpflimit-burst %u", info->cfg.burst);
	}

	if (info->cfg.mode & (XT_BPFLIMIT_HASH_SIP | XT_BPFLIMIT_HASH_SPT |
	    XT_BPFLIMIT_HASH_DIP | XT_BPFLIMIT_HASH_DPT)) {
		fputs(" --bpflimit-mode", stdout);
		print_mode(info->cfg.mode, ',');
	}

	printf(" --bpflimit-name %s", info->name);

	if (info->cfg.size != 0)
		printf(" --bpflimit-htable-size %u", info->cfg.size);
	if (info->cfg.max != 0)
		printf(" --bpflimit-htable-max %u", info->cfg.max);
	if (info->cfg.gc_interval != XT_BPFLIMIT_GCINTERVAL)
		printf(" --bpflimit-htable-gcinterval %u", info->cfg.gc_interval);
	if (info->cfg.expire != quantum)
		printf(" --bpflimit-htable-expire %u", info->cfg.expire);

	if (info->cfg.srcmask != dmask)
		printf(" --bpflimit-srcmask %u", info->cfg.srcmask);
	if (info->cfg.dstmask != dmask)
		printf(" --bpflimit-dstmask %u", info->cfg.dstmask);
}

static void
bpflimit_mt4_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_bpflimit_mtinfo1 *info = (const void *)match->data;

	bpflimit_mt_save(info, 32);
}

static void
bpflimit_mt6_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_bpflimit_mtinfo1 *info = (const void *)match->data;

	bpflimit_mt_save(info, 128);
}

static struct xtables_match bpflimit_mt_reg[] = {
	{
		.family        = NFPROTO_UNSPEC,
		.name          = "bpflimit",
		.version       = XTABLES_VERSION,
		.revision      = 0,
		.size          = XT_ALIGN(sizeof(struct xt_bpflimit_info)),
		.userspacesize = offsetof(struct xt_bpflimit_info, hinfo),
		.help          = bpflimit_help,
		.init          = bpflimit_init,
		.x6_parse      = bpflimit_parse,
		.x6_fcheck     = bpflimit_check,
		.print         = bpflimit_print,
		.save          = bpflimit_save,
		.x6_options    = bpflimit_opts,
		.udata_size    = sizeof(struct bpflimit_mt_udata),
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "bpflimit",
		.revision      = 1,
		.family        = NFPROTO_IPV4,
		.size          = XT_ALIGN(sizeof(struct xt_bpflimit_mtinfo1)),
		.userspacesize = offsetof(struct xt_bpflimit_mtinfo1, hinfo),
		.help          = bpflimit_mt_help,
		.init          = bpflimit_mt4_init,
		.x6_parse      = bpflimit_mt_parse,
		.x6_fcheck     = bpflimit_mt_check,
		.print         = bpflimit_mt4_print,
		.save          = bpflimit_mt4_save,
		.x6_options    = bpflimit_mt_opts,
		.udata_size    = sizeof(struct bpflimit_mt_udata),
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "bpflimit",
		.revision      = 1,
		.family        = NFPROTO_IPV6,
		.size          = XT_ALIGN(sizeof(struct xt_bpflimit_mtinfo1)),
		.userspacesize = offsetof(struct xt_bpflimit_mtinfo1, hinfo),
		.help          = bpflimit_mt_help,
		.init          = bpflimit_mt6_init,
		.x6_parse      = bpflimit_mt_parse,
		.x6_fcheck     = bpflimit_mt_check,
		.print         = bpflimit_mt6_print,
		.save          = bpflimit_mt6_save,
		.x6_options    = bpflimit_mt_opts,
		.udata_size    = sizeof(struct bpflimit_mt_udata),
	},
};

void _init(void)
{
	xtables_register_matches(bpflimit_mt_reg, ARRAY_SIZE(bpflimit_mt_reg));
}