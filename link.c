#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#include "common.h"

typedef struct neighbor_t {
	unit_t id;
	unit_t times;
} NEIGHBOR;

typedef struct node_t {
	unit_t id;
	NEIGHBOR *nei;
	unit_t nei_num;
	int nei_p;	//also the node degree
} NODE;

int cmp_nei(const void *n1, const void *n2)
{
	return (((NEIGHBOR *)n2)->times - ((NEIGHBOR *)n1)->times);
}

int meet_times(char *str)
{
	char *token = NULL;
	int cnt = 0;
	token = strtok(str, ",");
	while(token != NULL) {
		cnt++;

		token = strtok(NULL, ",");
	}

	return cnt;
}

int main(int argc, char *argv[])
{
	char cmd[1024] = {0};
	sprintf(cmd, "cut -d , -f 1 %s | sort | uniq | wc -l", argv[1]);
	int n = atoi(cmd_system(cmd));

	NODE node = {0};
	node.nei_p = -1;
	array_needsize(false, NEIGHBOR, node.nei, node.nei_num, 10, array_zero_init);

	FILE *f = fopen(argv[1], "r");
	FILE *fout = fopen(argv[2], "w");
	FILE *fout1 = fopen("./node.degree", "w");
	ssize_t read;
	size_t len = 0;
	char *line = NULL;

	char pos[10240] = {0};
	int upper, j;
	while((read = getline(&line, &len, f)) != -1) {
		int n1, n2, t;
		pos[0] = 0;
		sscanf(line, "%d,%d,%s", &n1, &n2, pos);	
		
		t = meet_times(pos);
		if(n1 > node.id) {
			qsort(node.nei, node.nei_p + 1, sizeof(NEIGHBOR), cmp_nei);
		//	upper = node.nei_p * 0.2 + 1;
			for(j=0; j<=node.nei_p; j++) {
				fprintf(fout, "%ld %ld %ld\n", node.id, node.nei[j].id, node.nei[j].times);
			}
			fprintf(fout1, "%ld %d\n", node.id, node.nei_p + 1);

			for(j=0; j<node.nei_num; j++) {
				node.nei[j].times = 0;
				node.nei[j].id = 0;
			}
			node.nei_p = -1;
		}

		NODE *nn = &node;
		nn->id = n1;
		if(nn->nei_p == -1) {
			nn->nei_p = 0;
		}
		else if(n2 > nn->nei[nn->nei_p].id) {
			nn->nei_p++;
			if(nn->nei_p >= nn->nei_num)
				array_needsize(false, NEIGHBOR, nn->nei, nn->nei_num, nn->nei_num + 1, array_zero_init);
		}

		nn->nei[nn->nei_p].id = n2;
		nn->nei[nn->nei_p].times += t;
	}

//	upper = node.nei_p * 0.2 + 1;
	for(j=0; j<=node.nei_p; j++) {
		fprintf(fout, "%ld %ld %ld\n", node.id, node.nei[j].id, node.nei[j].times);
	}
	fprintf(fout1, "%ld %d\n", node.id, node.nei_p + 1);

	fclose(fout);
	fclose(fout1);
	fclose(f);

	free(node.nei);
	free(line);

	return 0;
}
