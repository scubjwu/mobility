#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>

#include "common.h"

static inline unit_t array_nextsize(size_t elem, unit_t cur, unit_t cnt)
{
	unit_t ncur = cur + 1;

	do
		ncur <<= 1;
	while(cnt > ncur);

	if(elem * ncur > MALLOC_ROUND - sizeof(void *) * 4) {
		ncur *= elem;
		ncur = (ncur + elem + (MALLOC_ROUND - 1) + sizeof(void *) * 4) & ~(MALLOC_ROUND - 1);
		ncur = ncur - sizeof(void *) * 4;
		ncur /= elem;
	}

	return ncur;
}

//elem is the size of individual element; *cur is the current array size; cnt is the new size
void * declare_noinline array_realloc(size_t elem, void *base, unit_t *cur, unit_t cnt, bool limit)
{
	*cur = array_nextsize(elem, *cur, cnt);
	if(limit)
		*cur = *cur > WB_THRESHOLD ? WB_THRESHOLD:*cur;
	base = realloc(base, elem * *cur);
	return base;
}

inline void array_zero_init(void *p, size_t op, size_t np, size_t elem)
{
	memset(p + (op * elem), 0, (np - op) * elem);
}

char *cmd_system(const char *cmd)
{
	char *res = NULL;
	static char buf[BUFLEN];
	FILE *f;
	
	f = popen(cmd, "r");
	memset(buf, 0, BUFLEN * sizeof(char));
	while(fgets(buf, BUFLEN-1,f) != NULL)
		res = buf;

	if(f != NULL)
		pclose(f);

	return res;
}

void BlockSignal(bool block, int signo)
{
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, signo);
	sigprocmask(block?SIG_BLOCK:SIG_UNBLOCK, &set, NULL);
}

signal_fn CatchSignal(int signo, signal_fn handler)
{
	struct sigaction act, oldact;

	memset(&act, 0, sizeof(act));
	act.sa_handler = handler;

#ifdef SA_RESTART
	if(signo != SIGALRM)
		act.sa_flags |= SA_RESTART;
#endif

	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, signo);
	sigaction(signo, &act, &oldact);

	return oldact.sa_handler;
}

time_t convert_time(char *str)
{
	struct tm t = {0};

#ifdef UTC_TIME
	char date1[32] = {0};
	char date2[32] = {0};
	sscanf(str, "%[0-9,-]T%[0-9,:]Z", date1, date2);
	str[0] = 0;
	sprintf(str, "%s %s", date1, date2);
#endif

	strptime(str, TIME_FORMAT, &t);
	return mktime(&t);
}

void int_to_string(char *str, const unit_t *array, int len)
{
	int i;
	for(i=0; i<len-1; i++)
		str += sprintf(str, "%ld,", array[i]);

	sprintf(str, "%ld\r\n", array[i]);
}

void double_to_string(char *str, const double *array, int len)
{
	int i;
	for(i=0; i<len-1; i++)
		str += sprintf(str, "%.2lf,", array[i]);

	sprintf(str, "%.2lf\r\n", array[i]);
}

unsigned int BKDRHash(char *str)
{
	static unsigned int seed = 131;
	unsigned int hash = 0;
	while(*str)
		hash = hash * seed + (*str++);

	return (hash & 0x7FFFFFFF);
}
