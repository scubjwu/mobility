#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#include "common.h"

typedef struct path_t {
	int repeat;
	double delay;
	int hops;
	double weight;

	int *path;
} PATH;

typedef struct relay_t {
	int id;
	int loss;
	double weight;
} RELAY;

int *relay;
unit_t relay_num;
unit_t relay_p;

void update_relay(int id)
{
	int i;
	for(i=0; i<relay_p; i++) {
		if(relay[i] == id)
			return;
	}

	if(relay_p == relay_num)
		array_needsize(false, int, relay, relay_num, relay_num + 1, array_zero_init);

	relay[relay_p] = id;
	relay_p++;
}

void get_path(int *array, char *path)
{
	char *token = NULL;
	int i = 0;
	token = strtok(path, ",");
	while(token != NULL) {
		int id = atoi(token);
		array[i] = id;
		update_relay(id);

		token = strtok(NULL, ",");
		i++;
	}
}

bool path_search(int *array, int len, int key)
{
	int i;
	for(i=0; i<len; i++) {
		if(array[i] == key)
			return true;
	}

	return false;
}

int cmp_relay(const void *n1, const void *n2)
{
	return (((RELAY *)n1)->weight - ((RELAY *)n2)->weight);
}

void cdf_cal(RELAY *r, double base, double interval)
{
	int i, cnt = 0;
	double t = 0;

	qsort(r, relay_p, sizeof(RELAY), cmp_relay);
	for(i=0; i<relay_p; i++) {
		double v = base + t * interval;
		if(r[i].weight <= v)
			cnt++;
		else {
			while(r[i].weight > v) {
				printf("%lf,%lf\n", v, (double)cnt/(double)relay_p);
				v += interval;
				t++;
			}
			cnt++;
		}
	}
}

int main(int argc, char *argv[])
{
	/*
	 * usage: input file + src + dst + output file
	 * */

	if(argc != 5) {
		printf("wrong usage\n");
		return -1;
	}

	FILE *fin = fopen(argv[1], "r");
	FILE *fout = fopen(argv[4], "w");
	int sid = atoi(argv[2]);
	int did = atoi(argv[3]);

	char cmd[BUFLEN] = {0};
	sprintf(cmd, "cat %s | wc -l", argv[1]);
	int path_num = atoi(cmd_system(cmd));

	PATH *total_path = (PATH *)calloc(path_num, sizeof(PATH));
	array_needsize(false, int, relay, relay_num, 100, array_zero_init);
	relay_p = 0;

	ssize_t read;
	size_t len = 0;
	char *line = NULL;
	int i = 0;
	char tmp[BUFLEN] = {0};
	char path[BUFLEN] = {0};
	double total_weight = 0;

	while((read = getline(&line, &len, fin)) != -1) {
		int repeat, hops;
		double delay;
		tmp[0] = 0;
		path[0] = 0;

		sscanf(line, "%d,%lf,%s", &repeat, &delay, tmp);
		sscanf(tmp, "%d:%s", &hops, path);

		total_path[i].repeat = repeat;
		total_path[i].delay = delay;
		total_path[i].hops = hops;
		total_path[i].weight = (double)repeat/delay;
		total_weight += total_path[i].weight;
		total_path[i].path = (int *)calloc(hops, sizeof(int));
		get_path(total_path[i].path, path);

		i++;
	}

	RELAY *total_relay = (RELAY *)calloc(relay_p, sizeof(RELAY));

	for(i=0; i<relay_p; i++) {
		int j, loss = 0;
		double _weight = 0;
		if(relay[i] == sid || relay[i] == did)
			continue;
		
		//remove the node relay[i] to see what is the affect for all the path
		for(j=0; j<path_num; j++) {
			PATH *tmp = &total_path[j];
			if(path_search(tmp->path, tmp->hops, relay[i])) {
				loss++;
				_weight += tmp->weight;	
			}
		}

		fprintf(fout, "%d,%d,%lf\n", relay[i], loss, _weight * 10000000000);

		total_relay[i].loss = loss;
		total_relay[i].weight = _weight * 10000000000;
	}

	cdf_cal(total_relay, 1000, 1000);

	printf("total weight: %lf\n", total_weight);
}


