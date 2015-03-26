#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

typedef struct id_pool {
	unsigned int hash_id;
	unsigned long id;
} POOL;

static POOL *id_pool;
static unsigned long pool_p;
static unsigned long pool_num;

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
		array_needsize(false, POOL, id_pool, pool_num, pool_num + 128, array_zero_init);

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

	array_needsize(false, POOL, id_pool, pool_num, 10240, array_zero_init);

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

