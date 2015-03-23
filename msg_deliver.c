#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define START	"id, deliver, copy"
#define DST	"dst,status,delay,hops"
#define PATH	"path:"

double cmp_average(double *array, int len)
{
	int i;
	double sum = 0;
	for(i=0; i<len; i++)
		sum += array[i];

	return (sum/(double)len);
}

int main(int argc, char *argv[])
{
	/*
	 * usage: in_filename + # of elements + out_filename
	 * */

	if(argc != 4) {
		printf("wrong usage!\n");
		return -1;
	}

	FILE *fin = fopen(argv[1], "r");
	FILE *fout = fopen(argv[3], "w");
	double _delay, _copy, _hops;
	int i;

	double copy[10] = {0.};
	int num = atoi(argv[2]);
	double *hops;
	hops = (double *)malloc(num * sizeof(double));

	double *delay;
	delay = (double *)malloc(num * sizeof(double));

	int cnt = 0;
	int cct = 0;
	ssize_t read;
	size_t len = 0;
	char *line = NULL;

	fprintf(fout, "delay hops\r\n");

	while((read = getline(&line, &len, fin)) != -1) {
		if(strstr(line, START)) {
			int id, deliver, c;
			getline(&line, &len, fin);
			sscanf(line, "%d,%d,%d", &id, &deliver, &c);
			copy[cct++] = (double)c;	

			_copy += cmp_average(copy, cct);
			if(cnt) {
				_delay += cmp_average(delay, cnt);
				_hops += cmp_average(hops, cnt);

				for(i=0; i<cnt; i++) 
					fprintf(fout, "%lf,%lf\r\n", delay[i], hops[i]);
			}


			cnt = 0;
			memset(delay, 0, sizeof(double) * num);
			memset(hops, 0, sizeof(double) * num);
		}
		else if(strstr(line, DST)) {
			int dst, status, d, h;
			getline(&line, &len, fin);
			sscanf(line, "%d,%d,%d,%d", &dst, &status, &d, &h);
			if(status == 0)
				continue;
			
			delay[cnt] += d;
			hops[cnt] += h;

			cnt++;
		
		}
		else if(strstr(line, PATH)) {
		//TODO: 
		}
	}
	
	for(i=0; i<cnt; i++) 
		fprintf(fout, "%lf,%lf\r\n", delay[i], hops[i]);
	_delay += cmp_average(delay, cnt);
	_hops += cmp_average(hops, cnt);

	fclose(fin);
	free(line);

	_copy /= (double)cct;
	_delay /= (double)cct;
	_hops /= (double)cct;

	printf("average copy: %lf\naverage delay: %lf\naverage hops: %lf\n", _copy, _delay, _hops);

	fclose(fout);
	free(delay);
	free(hops);

	return 0;


}
