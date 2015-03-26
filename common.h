#ifndef _COMMON_H
#define _COMMON_H

typedef unsigned long unit_t;

typedef void (*signal_fn) (int);

#define UTC_TIME
#define MALLOC_ROUND	1024
#define BUFLEN			1024
#define WB_THRESHOLD	256	//power of 2!

#define _expect_false(expr)		__builtin_expect(!!(expr), 0)
#define _expect_true(expr)		__builtin_expect(!!(expr), 1)
#define expect_false(cond)		_expect_false(cond)
#define expect_true(cond)		_expect_true(cond)

#define set_attribute(attrlist)		__attribute__(attrlist)
#define declare_noinline		set_attribute((__noinline__))
#define declare_unused			set_attribute ((__unused__))

#define prefetch(x)	__builtin_prefetch(x)
#define offsetof(a,b)	__builtin_offsetof(a,b)

#define TIME_FORMAT	"%Y-%m-%d %H:%M:%S"
#define LINE_FORMAT	"%ld,%lf,%lf,%[^,],%ld"

inline void array_zero_init(void *p, size_t op, size_t np, size_t elem);
char *cmd_system(const char *cmd);
void BlockSignal(bool block, int signo);
signal_fn CatchSignal(int signo, signal_fn handler);
time_t convert_time(char *str);
void int_to_string(char *str, const unit_t *array, int len);
void double_to_string(char *str, const double *array, int len);
void * declare_noinline array_realloc(size_t elem, void *base, unit_t *cur, unit_t cnt, bool limit);
unsigned int BKDRHash(char *str);

#define array_needsize(limit, type, base, cur, cnt, init)	\
	if((cnt) > (cur)) {	\
		unit_t ocur_ = (cur);	\
		(base) = (type *)array_realloc(sizeof(type), (base), &(cur), (cnt), (limit));	\
		init((base), (ocur_), (cur), sizeof(type));	\
	}
	
#define _free(x) \
	{	\
		if((x))	free((x));	\
	}

#endif