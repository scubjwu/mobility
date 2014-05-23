#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

typedef unsigned long unit_t;

typedef struct id_pool {
	unsigned int hash_id;
	unsigned long id;
} POOL;

static POOL *id_pool;
static unsigned long pool_p;
static unsigned long pool_num;

#define _expect_false(expr)		__builtin_expect(!!(expr), 0)
#define _expect_true(expr)		__builtin_expect(!!(expr), 1)
#define expect_false(cond)		_expect_false(cond)
#define expect_true(cond)		_expect_true(cond)

#define set_attribute(attrlist)		__attribute__(attrlist)
#define declare_noinline		set_attribute((__noinline__))
#define declare_unused			set_attribute ((__unused__))

#define prefetch(x)	__builtin_prefetch(x)
#define offsetof(a,b)	__builtin_offsetof(a,b)

#define MALLOC_ROUND 1024
#define array_needsize(type, base, cur, cnt, init)	\
	if(expect_false((cnt) > (cur))) {	\
		unit_t ocur_ = (cur);	\
		(base) = (type *)array_realloc(sizeof(type), (base), &(cur), (cnt));	\
		init((base), (ocur_), (cur), sizeof(type));	\
	}

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
static void * declare_noinline array_realloc(size_t elem, void *base, unit_t *cur, unit_t cnt)
{
	*cur = array_nextsize(elem, *cur, cnt);
	base = realloc(base, elem * *cur);
	return base;
}

static inline void array_zero_init(void *p, size_t op, size_t np, size_t elem)
{
	memset(p + (op * elem), 0, (np - op) * elem);
}

unsigned int BKDRHash(char *str)
{
	static unsigned int seed = 131;
	unsigned int hash = 0;
	while(*str)
		hash = hash * seed + (*str++);

	return (hash & 0x7FFFFFFF);
}

static int cmp_id(const void *n1, const void *n2)
{
	return (((POOL *)n1)->hash_id - ((POOL *)n2)->hash_id);
}

static unsigned long id_get(unsigned int hash_id)
{
	POOL key, *res;
	key.hash_id = hash_id;
	res = bsearch(&key, id_pool, pool_p, sizeof(POOL), cmp_id);
	if(res)
		return res->id;

	if(expect_false(pool_p == pool_num))
		array_needsize(POOL, id_pool, pool_num, pool_num + 128, array_zero_init);

	id_pool[pool_p].hash_id = hash_id;
	id_pool[pool_p].id = pool_p++;

	qsort(id_pool, pool_p, sizeof(POOL), cmp_id);

	return (pool_p - 1);
}

int main(int argc, char *argv[])
{
	if(argc < 4) {
		printf("need input\n");
		exit(1);
	}

	FILE *fr = fopen(argv[1], "r");
	FILE *fw = fopen(argv[2], "w");
	char *line = NULL;
	ssize_t read = 0;
	size_t len = 0;
	int hash = atoi(argv[3]);

	unsigned long user_id, pos_id = 0;
	double x = 0, y = 0;
	char time[32] = {0};
	char hash_id[64] = {0};

	array_needsize(POOL, id_pool, pool_num, 10240, array_zero_init);

	while((read = getline(&line, &len, fr)) != -1) {
		if(hash == 0) {
			sscanf(line, "%ld %s %lf %lf %ld", &user_id, time, &x, &y, &pos_id);
			if(expect_false(x == 0 || y == 0 || pos_id == 0))
				goto RESET;

			pos_id = id_get(pos_id);
		}
		else {
			sscanf(line, "%ld %s %lf %lf %s", &user_id, time, &x, &y, hash_id);
			if(expect_false(x == 0 || y == 0 || hash_id[0] == 0))
				goto RESET;

			pos_id = id_get(BKDRHash(hash_id));
		}
		
		fprintf(fw, "%ld,%lf,%lf,%s,%ld\r\n", user_id, x, y, time, pos_id);
RESET:
		x = 0;
		y = 0;
		time[0] = 0;
		hash_id[0] = 0;
		pos_id = 0;
	}

	fclose(fr);
	fclose(fw);
	free(id_pool);
	id_pool = NULL;

	return 0;
}

