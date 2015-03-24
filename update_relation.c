#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define GREP_CMD	"grep ^%d$ %s"

static char *cmd_system(const char *cmd)
{
	char *res = NULL;
	static char buf[1024];
	FILE *f;

	f = popen(cmd, "r");
	memset(buf, 0, sizeof(char) * 1024);
	while(fgets(buf, 1023, f) != NULL)
		res = buf;

	if(f != NULL)
		pclose(f);

	return res;
}

int main(int argc, char *argv[])
{
	/*
	 *	relation input file + unreachable input file + src node + output file
	 * */
	
	if(argc != 5) {
		printf("wrong usage\n");
		return -1;
	}

	int sid = atoi(argv[3]);
	FILE *fin = fopen(argv[1], "r");
	FILE *fout = fopen(argv[4], "w");

	ssize_t read;
	size_t len = 0;
	char *line = NULL;

	while((read = getline(&line, &len, fin)) != -1) {
		int id1, id2;
		sscanf(line, "%d %d", &id1, &id2);
		if(id1 > sid)
			break;
		if(id1 == sid) {
			char cmd[512] = {0};
			sprintf(cmd, GREP_CMD, id2, argv[2]);
			char *res = cmd_system(cmd);
			if(res == NULL)
				fprintf(fout, "%s", line);
		}
	}

	free(line);
	fclose(fin);
	fclose(fout);

	return 0;
}
