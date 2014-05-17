#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "fifo.h"

#define is_power_of_2(x)	((x) != 0 && (((x) & ((x) - 1)) == 0))

#define ilog2(n)				\
(						\
	__builtin_constant_p(n) ? (		\
		(n) < 1 ? ____ilog2_NaN() :	\
		(n) & (1ULL << 63) ? 63 :	\
		(n) & (1ULL << 62) ? 62 :	\
		(n) & (1ULL << 61) ? 61 :	\
		(n) & (1ULL << 60) ? 60 :	\
		(n) & (1ULL << 59) ? 59 :	\
		(n) & (1ULL << 58) ? 58 :	\
		(n) & (1ULL << 57) ? 57 :	\
		(n) & (1ULL << 56) ? 56 :	\
		(n) & (1ULL << 55) ? 55 :	\
		(n) & (1ULL << 54) ? 54 :	\
		(n) & (1ULL << 53) ? 53 :	\
		(n) & (1ULL << 52) ? 52 :	\
		(n) & (1ULL << 51) ? 51 :	\
		(n) & (1ULL << 50) ? 50 :	\
		(n) & (1ULL << 49) ? 49 :	\
		(n) & (1ULL << 48) ? 48 :	\
		(n) & (1ULL << 47) ? 47 :	\
		(n) & (1ULL << 46) ? 46 :	\
		(n) & (1ULL << 45) ? 45 :	\
		(n) & (1ULL << 44) ? 44 :	\
		(n) & (1ULL << 43) ? 43 :	\
		(n) & (1ULL << 42) ? 42 :	\
		(n) & (1ULL << 41) ? 41 :	\
		(n) & (1ULL << 40) ? 40 :	\
		(n) & (1ULL << 39) ? 39 :	\
		(n) & (1ULL << 38) ? 38 :	\
		(n) & (1ULL << 37) ? 37 :	\
		(n) & (1ULL << 36) ? 36 :	\
		(n) & (1ULL << 35) ? 35 :	\
		(n) & (1ULL << 34) ? 34 :	\
		(n) & (1ULL << 33) ? 33 :	\
		(n) & (1ULL << 32) ? 32 :	\
		(n) & (1ULL << 31) ? 31 :	\
		(n) & (1ULL << 30) ? 30 :	\
		(n) & (1ULL << 29) ? 29 :	\
		(n) & (1ULL << 28) ? 28 :	\
		(n) & (1ULL << 27) ? 27 :	\
		(n) & (1ULL << 26) ? 26 :	\
		(n) & (1ULL << 25) ? 25 :	\
		(n) & (1ULL << 24) ? 24 :	\
		(n) & (1ULL << 23) ? 23 :	\
		(n) & (1ULL << 22) ? 22 :	\
		(n) & (1ULL << 21) ? 21 :	\
		(n) & (1ULL << 20) ? 20 :	\
		(n) & (1ULL << 19) ? 19 :	\
		(n) & (1ULL << 18) ? 18 :	\
		(n) & (1ULL << 17) ? 17 :	\
		(n) & (1ULL << 16) ? 16 :	\
		(n) & (1ULL << 15) ? 15 :	\
		(n) & (1ULL << 14) ? 14 :	\
		(n) & (1ULL << 13) ? 13 :	\
		(n) & (1ULL << 12) ? 12 :	\
		(n) & (1ULL << 11) ? 11 :	\
		(n) & (1ULL << 10) ? 10 :	\
		(n) & (1ULL <<  9) ?  9 :	\
		(n) & (1ULL <<  8) ?  8 :	\
		(n) & (1ULL <<  7) ?  7 :	\
		(n) & (1ULL <<  6) ?  6 :	\
		(n) & (1ULL <<  5) ?  5 :	\
		(n) & (1ULL <<  4) ?  4 :	\
		(n) & (1ULL <<  3) ?  3 :	\
		(n) & (1ULL <<  2) ?  2 :	\
		(n) & (1ULL <<  1) ?  1 :	\
		(n) & (1ULL <<  0) ?  0 :	\
		____ilog2_NaN()			\
				   ) :		\
	(sizeof(n) <= 4) ?			\
	__ilog2_u32(n) :			\
	__ilog2_u64(n)				\
)

#define roundup_pow_of_two(n)			\
	(						\
		__builtin_constant_p(n) ? (		\
			(n == 1) ? 1 :			\
			(1UL << (ilog2((n) - 1) + 1))	\
				   ) :		\
		__roundup_pow_of_two(n)			\
	)
	
#define min(x,y) ({ \
	typeof(x) _x = (x);	\
	typeof(y) _y = (y);	\
	(void) (&_x == &_y);	\
	_x < _y ? _x : _y; })

static inline int fls(int x)
{
	int r;

	asm("bsrl %1,%0\n\t"
	    "cmovzl %2,%0"
	    : "=&r" (r) : "rm" (x), "rm" (-1));

	return r + 1;
}

static __always_inline int fls64(unsigned long long x)
{
	unsigned int h = x >> 32;
	if (h)
		return fls(h) + 32;
	return fls(x);
}

static inline unsigned fls_long(unsigned long l)
{
	if (sizeof(l) == 4)
		return fls(l);
	return fls64(l);
}

static unsigned long __roundup_pow_of_two(unsigned long n)
{
	return 1UL << fls_long(n - 1);
}

static FIFO *fifo_init(fifo_data_t *buffer, unsigned int size)
{
	FIFO *fifo;

	/* size must be a power of 2 */
	if(!is_power_of_2(size)) {
		fprintf(stderr, "size is not ok\n");
		return NULL;
	}

	fifo = (FIFO *)malloc(sizeof(FIFO));
	if(!fifo) {
		perror("malloc");
		abort();
	}

	fifo->data = buffer;
	fifo->size = size;
	fifo->in = fifo->out = 0;

	return fifo;
}

FIFO *fifo_alloc(unsigned int size)
{
	fifo_data_t *data;
	FIFO *ret;

	if (!is_power_of_2(size)) {
		if(size > 0x80000000) {
			fprintf(stderr, "size too big\n");
			abort();
		}
		size = roundup_pow_of_two(size);
	}

	data = (fifo_data_t)calloc(size, sizeof(fifo_data_t));
	if (!data) {
		perror("calloc");
		abort();
	}

	ret = fifo_init(data, size);

	if(!ret)
		free(data);

	return ret;
}

void fifo_free(FIFO *fifo)
{
	if(fifo->in - fifo->out) {
		printf("may have memory leak...\n");
		int i = fifo->size;
		while(i) {
			if(fifo->data[i-1])
				free(fifo->data[i-1]);
			i--;
		}
	}

	free(fifo->data);
	free(fifo);
}

unsigned int _fifo_put(FIFO *fifo, fifo_data_t put)
{
	if(fifo->size - fifo->in + fifo->out == 0)
		return -1;

	__sync_synchronize();

	fifo->data[fifo->in & (fifo->size - 1)] = put;

	__sync_synchronize();

	fifo->in++;

	return 0;
}

unsigned int _fifo_get(FIFO *fifo, fifo_data_t *get)
{
	if(fifo->in - fifo->out == 0)
		return -1;

	__sync_synchronize();
	
	*get = fifo->data[fifo->out & (fifo->size - 1)];

	__sync_synchronize();

	fifo->out++;

	return 0;
}

#if 0
//////////////////test/////////////////////////////////////////////
#define f_len	4

FIFO *f;
char w[] = "writes:";

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t req = PTHREAD_COND_INITIALIZER;

void *p_writer(void *arg)
{
	int i = 0, sflag;
	char *data;
	
	sleep(1);

	for(;;) {
		data = malloc(128);
		sprintf(data, "%s %s %d", __func__, w, i++);
		sflag = 0;

		while(_fifo_put(f, data) != 0)	{//always try to write data...
			if(sflag)
				continue;

			printf("write block\n");
			pthread_cond_signal(&req);
			sflag = 1;
		}

		if(i == 10) {
			i = 0;
			sleep(1);
		}
	} 
	
	printf("writer finishes\n");
}

void *p_reader(void *arg)
{
	fifo_data_t r;

	for(;;) {
		while(_fifo_get(f, &r) != 0) {
			printf("read block\n");
			pthread_cond_wait(&req, &mutex);
		}
		printf("read data: '%s'\n", r);
		free(r);
	}
	
	printf("reader finishes\n");
}

int main(void)
{
	f = fifo_alloc(f_len);
	
	pthread_t pw, pr;
	
	pthread_create(&pw, NULL, p_writer, NULL);
	pthread_create(&pr, NULL, p_reader, NULL);
	
	pthread_join(pw, NULL);
	pthread_join(pr, NULL);

	fifo_free(f);
	
	return 0;
}
#endif

