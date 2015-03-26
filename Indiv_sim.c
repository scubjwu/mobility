#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#include "common.h"

#define DST	"dst,status,delay,hops"
//#define PATH	"path:"

typedef struct path_t {
	int hops;
	double delay;
	unsigned long times;
	char *path;
} PATH;

typedef struct relay_t {
	unsigned long id;
	unsigned long times;
	double delay;
	double hops;
} RELAY;

PATH *path_lst;
unit_t path_p = 0;
unit_t path_n;

RELAY *relay_lst;
unit_t relay_p = 0;
unit_t relay_n;

int search_plst(char *line)
{
	int i;
	for(i=0; i<path_p; i++) {
		if(strcmp(line, path_lst[i].path) == 0)
			return i;
	}

	return -1;
}

void add_path(char *line, int hops, int delay)
{
	if(path_p == path_n) 
		array_needsize(false, PATH, path_lst, path_n, path_n + 1, array_zero_init);
	
	path_lst[path_p].hops = hops;
	path_lst[path_p].delay = delay;
	path_lst[path_p].times = 1;
	path_lst[path_p].path = (char *)calloc(strlen(line) + 5, sizeof(char));
	memcpy(path_lst[path_p].path, line, strlen(line) * sizeof(char));
//	strcpy(path_lst[path_p].path, line);

	path_p++;
}

void update_path(int index, int delay)
{
	path_lst[index].times++;
	path_lst[index].delay += delay;
}

void path_record(char *line, int hops, int delay)
{
	int index = search_plst(line);

	if(index == -1)
		add_path(line, hops, delay);
	else
		update_path(index, delay);
}

int search_rlst(int id)
{
	int i;
	for(i=0; i<relay_p; i++) {
		if(relay_lst[i].id == id)
			return i;
	}

	return -1;
}

void add_relay(int id, int hops, int delay)
{
	if(relay_p == relay_n)
		array_needsize(false, RELAY, relay_lst, relay_n, relay_n + 1, array_zero_init);

	relay_lst[relay_p].id = id;
	relay_lst[relay_p].times = 1;
	relay_lst[relay_p].delay = delay;
	relay_lst[relay_p].hops = hops;

	relay_p++;
}

void update_relay(int index, int hops, int delay)
{
	relay_lst[index].times++;
	relay_lst[index].delay += delay;
	relay_lst[index].hops += hops;
}

void _node_record(int id, int hops, int delay)
{
	int index = search_rlst(id);

	if(index == -1)
		add_relay(id, hops, delay);
	else
		update_relay(index, hops, delay);
}

void node_record(char *line, int hops, int delay)
{
	char *token = NULL;
	token = strtok(line, ",");
	while(token != NULL) {
		int id = atoi(token);
		_node_record(id, hops, delay);

		token = strtok(NULL, ",");
	}	
}

void relay_print(char *file)
{
	FILE *fp = fopen(file, "w");	
	int i;
	double _delay, _hops;
	RELAY *tmp;
	for(i=0; i<relay_p; i++) {
		tmp = &relay_lst[i];
		_delay = tmp->delay/(double)tmp->times;
		_hops = tmp->hops/(double)tmp->times;
		fprintf(fp, "%ld,%ld,%lf,%lf,%lf\r\n", tmp->id, tmp->times, _delay, _hops, _delay * _hops);
	}

	_free(relay_lst);
	fclose(fp);
}

void path_print(char *file)
{
	FILE *fp = fopen(file, "w");
	int i;
	double _delay;
	PATH *tmp;
	for(i=0; i<path_p; i++) {
		tmp = &path_lst[i];
		_delay = tmp->delay/(double)tmp->times;
		fprintf(fp, "%ld,%lf,%d:%s", tmp->times, _delay, tmp->hops, tmp->path);

		_free(tmp->path);
	}

	_free(path_lst);
	fclose(fp);
}

int main(int argc, char *argv[])
{
	/*
	 * input file + output file[delay + hops] + output file[path] + path.record + relay.record
	 * 
	*/

	if(argc != 6) {
		printf("wrong usage\n");
		return -1;
	}

	array_needsize(false, PATH, path_lst, path_n, 10, array_zero_init);
	array_needsize(false, RELAY, relay_lst, relay_n, 10, array_zero_init);

	FILE *fin = fopen(argv[1], "r");
	FILE *fout_dh = fopen(argv[2], "w");
	FILE *fout_path = fopen(argv[3], "w");
	double _delay = 0;
        double _hops = 0;
	int i, cnt = 0;

	ssize_t read;
	size_t len = 0;
	char *line = NULL;

	while((read = getline(&line, &len, fin)) != -1) {
		if(strstr(line, DST)) {
			int dst, status, d, h;
			getline(&line, &len, fin);
			sscanf(line, "%d,%d,%d,%d", &dst, &status, &d, &h);

			_delay += d;
			_hops += h;
			fprintf(fout_dh, "%d,%d\r\n", d, h);
			cnt++;

			//get the related path
			getline(&line, &len, fin);
			getline(&line, &len, fin);
			fprintf(fout_path, "%s", line);

			path_record(line, h, d);
			node_record(line, h, d);
		}
		/*
		else if(strstr(line, PATH)) {
			getline(&line, &len, fin);
			fprintf(fout_path, "%s", line);
		}
		*/
	}

	path_print(argv[4]);
	relay_print(argv[5]);

	_delay /= (double)cnt;
	_hops /= (double)cnt;

	printf("average delay: %lf\naverage hops: %lf\n", _delay, _hops);

	free(line);
	fclose(fin);
	fclose(fout_dh);
	fclose(fout_path);

	return 0;
}
