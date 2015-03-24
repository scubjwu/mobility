#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	FILE *fin = fopen(argv[1], "r");
	FILE *fout = fopen(argv[2], "w");
	double _delay = 0;
	int cnt = 0;
	double start = 2;
	int dlen = 10;
	int plen = 0;
	double *average_delay = (double *)calloc(dlen, sizeof(double));

	ssize_t read;
	size_t len = 0;
	char *line = NULL;

	while((read = getline(&line, &len, fin)) != -1) {
		double delay, hops;
		sscanf(line, "%lf, %lf", &delay, &hops);

		if(hops == start) {
			_delay += delay;
			cnt++;
		}

		if(hops > start) {
			if(cnt) {
				average_delay[plen] = _delay/(double)cnt;
				fprintf(fout, "%lf,%lf\r\n", average_delay[plen], start);
			}
			_delay = 0;
			cnt = 0;

			fseek(fin, ftell(fin) - read, SEEK_SET);
			start++;
			plen++;
			if(plen == dlen) {
				dlen += 10;
				average_delay = (double *)realloc(average_delay, dlen * sizeof(double));
			}
		}
	}
	if(cnt) {
		average_delay[plen] = _delay/(double)cnt;
		fprintf(fout, "%lf %lf\r\n", average_delay[plen], start);
	}

	fclose(fin);
	fclose(fout);
	free(line);

	return 0;
}
