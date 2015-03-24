#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void double_to_string(char *str, const double *array, int len)
{
	int i;
	for(i=0; i<len-1; i++)
		str += sprintf(str, "%.5lf,", array[i]);

	sprintf(str, "%.5lf\r\n", array[i]);
}

static void distribution_wb(double *array, int len, double s, int l, FILE *fp)
{
	fprintf(fp, "x,y\r\n");
	int i;
	for(i=0; i<len; i++) {
		fprintf(fp, "%lf,%lf\r\n", s, array[i]);
		s += l;
	}
}

void distribution_cmp(FILE *in, FILE *out, int l)
{
	double start = 2.;
	int total = 0;
	double *probability;
	int cur = 0;
	int cnt = 10;
	probability = (double *)calloc(cnt, sizeof(double));

	ssize_t read;
	size_t len = 0;
	char *line = NULL;
	while((read = getline(&line, &len, in)) != -1) {
		double value;
		sscanf(line, "%lf", &value);

		if(value <= start) {
			probability[cur]++;
			total++;
		}

		if(value > start) {
			fseek(in, ftell(in) - read, SEEK_SET);
			start += l;
			cur++;
			if(cur == cnt) {
				cnt += 10;
				probability = (double *)realloc(probability, cnt * sizeof(double));
			}
		}
	}

	int i;
	for(i=0; i<cur; i++) 
		probability[i] /= (double)total;

	distribution_wb(probability, cur, 2, l, out);

	fclose(in);
	free(line);
	fclose(out);
}

int main(int argc, char *argv[])
{
	/*
	 *	input file + array len + output file
	 * */

	if(argc != 4) {
		printf("wrong usage\n");	
		exit(1);
	}

	FILE *fin = fopen(argv[1], "r");
	FILE *fout = fopen(argv[3], "w");
	int len = atoi(argv[2]);

	distribution_cmp(fin, fout, len);

	return 0;
}

