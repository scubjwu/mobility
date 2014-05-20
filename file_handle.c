#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

unsigned int BKDRHash(char *str)
{
	unsigned int seed = 131;
	unsigned int hash = 0;
	while(*str)
		hash = hash * seed + (*str++);

	return (hash & 0x7FFFFFFF);
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

	unsigned long user_id, pos_id;
	double x, y;
	char time[32] = {0};
	char hash_id[64] = {0};

	while((read = getline(&line, &len, fr)) != -1) {
		if(hash == 0)
			sscanf(line, "%ld %s %lf %lf %ld", &user_id, time, &x, &y, &pos_id);
		else {
			sscanf(line, "%ld %s %lf %lf %s", &user_id, time, &x, &y, hash_id);
			pos_id = BKDRHash(hash_id);
		}
		fprintf(fw, "%ld,%lf,%lf,%s,%ld\r\n", user_id, x, y, time, pos_id);

		time[0] = 0;
		hash_id[0] = 0;
	}

	fclose(fr);
	fclose(fw);

	return 0;
}
