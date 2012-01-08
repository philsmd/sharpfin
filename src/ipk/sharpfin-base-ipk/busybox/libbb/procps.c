/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright 1998 by Albert Cahalan; all rights reserved.
 * Copyright (C) 2002 by Vladimir Oleynik <dzo@simtreas.ru>
 * SELinux support: (c) 2007 by Yuichi Nakamura <ynakam@hitachisoft.jp>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"


typedef struct unsigned_to_name_map_t {
	unsigned id;
	char name[USERNAME_MAX_SIZE];
} unsigned_to_name_map_t;

typedef struct cache_t {
	unsigned_to_name_map_t *cache;
	int size;
} cache_t;

static cache_t username, groupname;

static void clear_cache(cache_t *cp)
{
	free(cp->cache);
	cp->cache = NULL;
	cp->size = 0;
}
void clear_username_cache(void)
{
	clear_cache(&username);
	clear_cache(&groupname);
}

#if 0 /* more generic, but we don't need that yet */
/* Returns -N-1 if not found. */
/* cp->cache[N] is allocated and must be filled in this case */
static int get_cached(cache_t *cp, unsigned id)
{
	int i;
	for (i = 0; i < cp->size; i++)
		if (cp->cache[i].id == id)
			return i;
	i = cp->size++;
	cp->cache = xrealloc(cp->cache, cp->size * sizeof(*cp->cache));
	cp->cache[i++].id = id;
	return -i;
}
#endif

typedef char* ug_func(char *name, int bufsize, long uid);
static char* get_cached(cache_t *cp, unsigned id, ug_func* fp)
{
	int i;
	for (i = 0; i < cp->size; i++)
		if (cp->cache[i].id == id)
			return cp->cache[i].name;
	i = cp->size++;
	cp->cache = xrealloc(cp->cache, cp->size * sizeof(*cp->cache));
	cp->cache[i].id = id;
	/* Never fails. Generates numeric string if name isn't found */
	fp(cp->cache[i].name, sizeof(cp->cache[i].name), id);
	return cp->cache[i].name;
}
const char* get_cached_username(uid_t uid)
{
	return get_cached(&username, uid, bb_getpwuid);
}
const char* get_cached_groupname(gid_t gid)
{
	return get_cached(&groupname, gid, bb_getgrgid);
}


#define PROCPS_BUFSIZE 1024

static int read_to_buf(const char *filename, void *buf)
{
	int fd;
	/* open_read_close() would do two reads, checking for EOF.
	 * When you have 10000 /proc/$NUM/stat to read, it isn't desirable */
	ssize_t ret = -1;
	fd = open(filename, O_RDONLY);
	if (fd >= 0) {
		ret = read(fd, buf, PROCPS_BUFSIZE-1);
		close(fd);
	}
	((char *)buf)[ret > 0 ? ret : 0] = '\0';
	return ret;
}

static procps_status_t *alloc_procps_scan(void)
{
	unsigned n = getpagesize();
	procps_status_t* sp = xzalloc(sizeof(procps_status_t));
	sp->dir = xopendir("/proc");
	while (1) {
		n >>= 1;
		if (!n) break;
		sp->shift_pages_to_bytes++;
	}
	sp->shift_pages_to_kb = sp->shift_pages_to_bytes - 10;
	return sp;
}

void free_procps_scan(procps_status_t* sp)
{
	closedir(sp->dir);
	free(sp->argv0);
	USE_SELINUX(free(sp->context);)
	free(sp);
}

#if ENABLE_FEATURE_TOPMEM
static unsigned long fast_strtoul_16(char **endptr)
{
	unsigned char c;
	char *str = *endptr;
	unsigned long n = 0;

	while ((c = *str++) != ' ') {
		c = ((c|0x20) - '0');
		if (c > 9)
			// c = c + '0' - 'a' + 10:
			c = c - ('a' - '0' - 10);
		n = n*16 + c;
	}
	*endptr = str; /* We skip trailing space! */
	return n;
}
/* TOPMEM uses fast_strtoul_10, so... */
#undef ENABLE_FEATURE_FAST_TOP
#define ENABLE_FEATURE_FAST_TOP 1
#endif

#if ENABLE_FEATURE_FAST_TOP
/* We cut a lot of corners here for speed */
static unsigned long fast_strtoul_10(char **endptr)
{
	char c;
	char *str = *endptr;
	unsigned long n = *str - '0';

	while ((c = *++str) != ' ')
		n = n*10 + (c - '0');

	*endptr = str + 1; /* We skip trailing space! */
	return n;
}
static char *skip_fields(char *str, int count)
{
	do {
		while (*str++ != ' ')
			continue;
		/* we found a space char, str points after it */
	} while (--count);
	return str;
}
#endif

void BUG_comm_size(void);
procps_status_t *procps_scan(procps_status_t* sp, int flags)
{
	struct dirent *entry;
	char buf[PROCPS_BUFSIZE];
	char filename[sizeof("/proc//cmdline") + sizeof(int)*3];
	char *filename_tail;
	long tasknice;
	unsigned pid;
	int n;
	struct stat sb;

	if (!sp)
		sp = alloc_procps_scan();

	for (;;) {
		entry = readdir(sp->dir);
		if (entry == NULL) {
			free_procps_scan(sp);
			return NULL;
		}
		pid = bb_strtou(entry->d_name, NULL, 10);
		if (errno)
			continue;

		/* After this point we have to break, not continue
		 * ("continue" would mean that current /proc/NNN
		 * is not a valid process info) */

		memset(&sp->vsz, 0, sizeof(*sp) - offsetof(procps_status_t, vsz));

		sp->pid = pid;
		if (!(flags & ~PSSCAN_PID)) break;

#if ENABLE_SELINUX
		if (flags & PSSCAN_CONTEXT) {
			if (getpidcon(sp->pid, &sp->context) < 0)
				sp->context = NULL;
		}
#endif

		filename_tail = filename + sprintf(filename, "/proc/%d", pid);

		if (flags & PSSCAN_UIDGID) {
			if (stat(filename, &sb))
				break;
			/* Need comment - is this effective or real UID/GID? */
			sp->uid = sb.st_uid;
			sp->gid = sb.st_gid;
		}

		if (flags & PSSCAN_STAT) {
			char *cp, *comm1;
			int tty;
#if !ENABLE_FEATURE_FAST_TOP
			unsigned long vsz, rss;
#endif

			/* see proc(5) for some details on this */
			strcpy(filename_tail, "/stat");
			n = read_to_buf(filename, buf);
			if (n < 0)
				break;
			cp = strrchr(buf, ')'); /* split into "PID (cmd" and "<rest>" */
			/*if (!cp || cp[1] != ' ')
				break;*/
			cp[0] = '\0';
			if (sizeof(sp->comm) < 16)
				BUG_comm_size();
			comm1 = strchr(buf, '(');
			/*if (comm1)*/
				safe_strncpy(sp->comm, comm1 + 1, sizeof(sp->comm));

#if !ENABLE_FEATURE_FAST_TOP
			n = sscanf(cp+2,
				"%c %u "               /* state, ppid */
				"%u %u %d %*s "        /* pgid, sid, tty, tpgid */
				"%*s %*s %*s %*s %*s " /* flags, min_flt, cmin_flt, maj_flt, cmaj_flt */
				"%lu %lu "             /* utime, stime */
				"%*s %*s %*s "         /* cutime, cstime, priority */
				"%ld "                 /* nice */
				"%*s %*s "             /* timeout, it_real_value */
				"%lu "                 /* start_time */
				"%lu "                 /* vsize */
				"%lu "                 /* rss */
			/*	"%lu %lu %lu %lu %lu %lu " rss_rlim, start_code, end_code, start_stack, kstk_esp, kstk_eip */
			/*	"%u %u %u %u "         signal, blocked, sigignore, sigcatch */
			/*	"%lu %lu %lu"          wchan, nswap, cnswap */
				,
				sp->state, &sp->ppid,
				&sp->pgid, &sp->sid, &tty,
				&sp->utime, &sp->stime,
				&tasknice,
				&sp->start_time,
				&vsz,
				&rss);
			if (n != 11)
				break;
			/* vsz is in bytes and we want kb */
			sp->vsz = vsz >> 10;
			/* vsz is in bytes but rss is in *PAGES*! Can you believe that? */
			sp->rss = rss << sp->shift_pages_to_kb;
			sp->tty_major = (tty >> 8) & 0xfff;
			sp->tty_minor = (tty & 0xff) | ((tty >> 12) & 0xfff00);
#else
/* This costs ~100 bytes more but makes top faster by 20%
 * If you run 10000 processes, this may be important for you */
			sp->state[0] = cp[2];
			cp += 4;
			sp->ppid = fast_strtoul_10(&cp);
			sp->pgid = fast_strtoul_10(&cp);
			sp->sid = fast_strtoul_10(&cp);
			tty = fast_strtoul_10(&cp);
			sp->tty_major = (tty >> 8) & 0xfff;
			sp->tty_minor = (tty & 0xff) | ((tty >> 12) & 0xfff00);
			cp = skip_fields(cp, 6); /* tpgid, flags, min_flt, cmin_flt, maj_flt, cmaj_flt */
			sp->utime = fast_strtoul_10(&cp);
			sp->stime = fast_strtoul_10(&cp);
			cp = skip_fields(cp, 3); /* cutime, cstime, priority */
			tasknice = fast_strtoul_10(&cp);
			cp = skip_fields(cp, 2); /* timeout, it_real_value */
			sp->start_time = fast_strtoul_10(&cp);
			/* vsz is in bytes and we want kb */
			sp->vsz = fast_strtoul_10(&cp) >> 10;
			/* vsz is in bytes but rss is in *PAGES*! Can you believe that? */
			sp->rss = fast_strtoul_10(&cp) << sp->shift_pages_to_kb;
#endif

			if (sp->vsz == 0 && sp->state[0] != 'Z')
				sp->state[1] = 'W';
			else
				sp->state[1] = ' ';
			if (tasknice < 0)
				sp->state[2] = '<';
			else if (tasknice) /* > 0 */
				sp->state[2] = 'N';
			else
				sp->state[2] = ' ';

		}

#if ENABLE_FEATURE_TOPMEM
		if (flags & (PSSCAN_SMAPS)) {
			FILE *file;

			strcpy(filename_tail, "/smaps");
			file = fopen(filename, "r");
			if (!file)
				break;
			while (fgets(buf, sizeof(buf), file)) {
				unsigned long sz;
				char *tp;
				char w;
#define SCAN(str, name) \
	if (strncmp(buf, str, sizeof(str)-1) == 0) { \
		tp = skip_whitespace(buf + sizeof(str)-1); \
		sp->name += fast_strtoul_10(&tp); \
		continue; \
	}
				SCAN("Shared_Clean:" , shared_clean );
				SCAN("Shared_Dirty:" , shared_dirty );
				SCAN("Private_Clean:", private_clean);
				SCAN("Private_Dirty:", private_dirty);
#undef SCAN
				// f7d29000-f7d39000 rw-s ADR M:m OFS FILE
				tp = strchr(buf, '-');
				if (tp) {
					*tp = ' ';
					tp = buf;
					sz = fast_strtoul_16(&tp); /* start */
					sz = (fast_strtoul_16(&tp) - sz) >> 10; /* end - start */
					// tp -> "rw-s" string
					w = tp[1];
					// skipping "rw-s ADR M:m OFS "
					tp = skip_whitespace(skip_fields(tp, 4));
					// filter out /dev/something (something != zero)
					if (strncmp(tp, "/dev/", 5) != 0 || strcmp(tp, "/dev/zero\n") == 0) {
						if (w == 'w') {
							sp->mapped_rw += sz;
						} else if (w == '-') {
							sp->mapped_ro += sz;
						}
					}
//else printf("DROPPING %s (%s)\n", buf, tp);
					if (strcmp(tp, "[stack]\n") == 0)
						sp->stack += sz;
				}
			}
			fclose(file);
		}
#endif /* TOPMEM */

#if 0 /* PSSCAN_CMD is not used */
		if (flags & (PSSCAN_CMD|PSSCAN_ARGV0)) {
			free(sp->argv0);
			sp->argv0 = NULL;
			free(sp->cmd);
			sp->cmd = NULL;
			strcpy(filename_tail, "/cmdline");
			/* TODO: to get rid of size limits, read into malloc buf,
			 * then realloc it down to real size. */
			n = read_to_buf(filename, buf);
			if (n <= 0)
				break;
			if (flags & PSSCAN_ARGV0)
				sp->argv0 = xstrdup(buf);
			if (flags & PSSCAN_CMD) {
				do {
					n--;
					if ((unsigned char)(buf[n]) < ' ')
						buf[n] = ' ';
				} while (n);
				sp->cmd = xstrdup(buf);
			}
		}
#else
		if (flags & (PSSCAN_ARGV0|PSSCAN_ARGVN)) {
			free(sp->argv0);
			sp->argv0 = NULL;
			strcpy(filename_tail, "/cmdline");
			n = read_to_buf(filename, buf);
			if (n <= 0)
				break;
#if ENABLE_PGREP || ENABLE_PKILL
			if (flags & PSSCAN_ARGVN) {
				do {
					n--;
					if (buf[n] == '\0')
						buf[n] = ' ';
				} while (n);
			}
#endif
			sp->argv0 = xstrdup(buf);
		}
#endif
		break;
	}
	return sp;
}

void read_cmdline(char *buf, int col, unsigned pid, const char *comm)
{
	ssize_t sz;
	char filename[sizeof("/proc//cmdline") + sizeof(int)*3];

	sprintf(filename, "/proc/%u/cmdline", pid);
	sz = open_read_close(filename, buf, col);
	if (sz > 0) {
		buf[sz] = '\0';
		while (--sz >= 0)
			if ((unsigned char)(buf[sz]) < ' ')
				buf[sz] = ' ';
	} else {
		snprintf(buf, col, "[%s]", comm);
	}
}

/* from kernel:
	//             pid comm S ppid pgid sid tty_nr tty_pgrp flg
	sprintf(buffer,"%d (%s) %c %d  %d   %d  %d     %d       %lu %lu \
%lu %lu %lu %lu %lu %ld %ld %ld %ld %d 0 %llu %lu %ld %lu %lu %lu %lu %lu \
%lu %lu %lu %lu %lu %lu %lu %lu %d %d %lu %lu %llu\n",
		task->pid,
		tcomm,
		state,
		ppid,
		pgid,
		sid,
		tty_nr,
		tty_pgrp,
		task->flags,
		min_flt,
		cmin_flt,
		maj_flt,
		cmaj_flt,
		cputime_to_clock_t(utime),
		cputime_to_clock_t(stime),
		cputime_to_clock_t(cutime),
		cputime_to_clock_t(cstime),
		priority,
		nice,
		num_threads,
		// 0,
		start_time,
		vsize,
		mm ? get_mm_rss(mm) : 0,
		rsslim,
		mm ? mm->start_code : 0,
		mm ? mm->end_code : 0,
		mm ? mm->start_stack : 0,
		esp,
		eip,
the rest is some obsolete cruft
*/
