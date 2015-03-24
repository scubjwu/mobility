#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DST	"dst,status,delay,hops"
#define PATH	"path:"

int main(int argc, char *argv[])
{
	/*
	 * input file + output file[delay + hops] + output file[path]
	 * 
	*/

	if(argc != 4) {
		printf("wrong usage\n");
		return -1;
	}

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
		}
		else if(strstr(line, PATH)) {
			getline(&line, &len, fin);
			fprintf(fout_path, "%s", line);
		}
	}

	_delay /= (double)cnt;
	_hops /= (double)cnt;

	printf("average delay: %lf\naverage hops: %lf\n", _delay, _hops);

	free(line);
	fclose(fin);
	fclose(fout_dh);
	fclose(fout_path);

	return 0;
}
