#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "nodes.h"

#define CMDLEN	512
#define LINELEN	512
#define BUFLEN	1024
#define MALLOC_ROUND	1024
#define TIME_SLOT	1800	//30 min

#define TIME_FORMAT	"%Y-%m-%d %H:%M:%S"
#define LINE_FORMAT	"%ld,%lf,%lf,%[^,],%ld"

static NODE *nlist;
static POS *plist;
static unit_t nodes_num;
static unit_t pos_num;
static time_t timer;
static FILE *FP;
static unit_t running_node;
static char *line = NULL;

#define array_needsize(type, base, cur, cnt, init)	\
	if((cnt) > (cur)) {	\
		unit_t ocur_ = (cur);	\
		(base) = (type *)array_realloc(sizeof(type), (base), &(cur), (cnt));	\
		init((base), (ocur_), (cur), sizeof(type));	\
	}

static inline unit_t array_nextsize(size_t elem, unit_t cur, unit_t cnt)
{
	unit_t ncur = cur + 1;

	do
		ncur <<= 1;
	while(cnt > ncur);

	if(elem * ncur > MALLOC_ROUND - sizeof(void *) * 4) {
		ncur *= elem;
		ncur = (ncur + elem + (MALLOC_ROUND - 1) + sizeof(void *) * 4) & ~(MALLOC_ROUND - 1);
		ncur = ncur - sizeof(void *) * 4;
		ncur /= elem;
	}

	return ncur;
}

//elem is the size of individual element; *cur is the current array size; cnt is the new size
static void* __attribute__((__noinline__)) array_realloc(size_t elem, void *base, unit_t *cur, unit_t cnt)
{
	*cur = array_nextsize(elem, *cur, cnt);
	base = realloc(base, elem * *cur);
	return base;
}

static inline void array_zero_init(void *p, size_t op, size_t np, size_t elem)
{
	memset(p + (op * elem), 0, (np - op) * elem);
}

static char *cmd_system(const char *cmd)
{
	char *res = "";
	char buf[BUFLEN] = {0};
	FILE *f;
	
	f = popen(cmd, "r");
	while(fgets(buf, BUFLEN-1,f) != NULL)
		res = buf;

	if(f != NULL)
		pclose(f);

	return res;
}

static void init_file(const char *file)
{
#define CMD_FORMAT	"sort -t , -k1,1n -k4,4 %s >> ./_tmp; mv ./_tmp %s"
	char cmd[CMDLEN] = {0};
	sprintf(cmd, CMD_FORMAT, file, file);
	cmd_system(cmd);

	FP = fopen(file, "r");
	if(FP == NULL) {
		printf("trace file failed to open");
		exit(1);
	}
#undef CMD_FORMAT
}

static inline void parse_line(const char *line, unit_t *id, double *x, double *y, time_t *time, unit_t *pos_id)
{
	char tmp[32] = {0};
	struct tm t = {0};
	sscanf(line, LINE_FORMAT, id, x, y, tmp, pos_id);
	strptime(tmp, TIME_FORMAT, &t);
	*time = mktime(&t);
}

static unit_t init_fpos(unit_t id)
{
	static size_t len = 0;
	static unit_t id_pos = 0;
	ssize_t read;
	double x,y;
	time_t time;
	unit_t pos;
	
	if(id <= id_pos)
		fseek(FP, 0, SEEK_SET);

	while((read = getline(&line, &len, FP)) != -1) {
		parse_line(line, &id_pos, &x, &y, &time, &pos);
		if(id_pos == id) {
		        nlist[id].time = time;	
			nlist[id].pos_x = x;
			nlist[id].pos_y = y;
			nlist[id].pos_id = pos;

			read = getline(&line, &len, FP);	//assume each node has at least two trace records
			parse_line(line, &id_pos, &x, &y, &time, &pos);
			nlist[id].next_time = time;

			return ftell(FP) - read;
		}
	}
}

static void init_node(NODE *n, unit_t id)
{
	n->user_id = id;
	n->fpos = init_fpos(id);
	
	array_needsize(NEIGHBOR, n->neighbor_D, n->neighbor_num, 0.1 * nodes_num, array_zero_init);

	array_needsize(unit_t, n->pos_D, n->pos_num, 0.01 * pos_num, array_zero_init);

	array_needsize(double, n->flight_D, n->flight_num, 1000, array_zero_init);

	array_needsize(unit_t, n->pause_D, n->pause_num, 1000, array_zero_init);
}

static inline void init_pos(POS *p)
{
	array_needsize(unit_t, p->node_id, p->node_num, 100, array_zero_init);
}

static void init_struct(const char *file)
{
	char cmd[CMDLEN] = {0};
	sprintf(cmd, "tail -1 %s | cut -d , -f 1", file);
	nodes_num = atol(cmd_system(cmd)) + 1;
	printf("node: %ld\n", nodes_num);

	cmd[0] = 0;
	sprintf(cmd, "sort -t , -k5,5nr %s | head -1 | cut -d , -f 5", file);
	pos_num = atol(cmd_system(cmd)) + 1;
	printf("pos: %ld\n", pos_num);

	cmd[0] = 0;
	struct tm time = {0};
	sprintf(cmd, "sort -t, -k4,4 %s | head -1 | cut -d , -f 4", file);
	strptime(cmd_system(cmd), TIME_FORMAT, &time);
	timer = mktime(&time);
	printf("time: %ld\n", timer);

	nlist = (NODE *)calloc(nodes_num, sizeof(NODE));
	plist = (POS *)calloc(pos_num, sizeof(POS));

	unit_t i;
	for(i=0; i<nodes_num; i++) {
		init_node(&nlist[i], i);
	}

	for(i=0; i<pos_num; i++) {
		init_pos(&plist[i]);
		plist[i].pos_id = i;
	}
}

static void free_node(NODE *n)
{
	if(n) {
		free(n->pos_D);
		free(n->flight_D);
		free(n->pause_D);

		unit_t i;
		for(i=0; i<n->neighbor_num; i++) 
			free(n->neighbor_D[i].meeting_pos);
			
		free(n->neighbor_D);
	}
}

static void free_struct(void)
{
	unit_t i;
	if(nlist) {
		for(i=0; i<nodes_num; i++)
			free_node(&nlist[i]);

		free(nlist);
	}

	nlist = NULL; 

	if(plist) {
		for(i=0; i<pos_num; i++) {
			free(plist[i].node_id);
		}
		free(plist);
	}
	plist = NULL;

	if(line) {
		free(line);
		line = NULL;
	}
}

static void pos_update(const NODE *n)
{
	unit_t i = n->pos_id;
	POS *p = &plist[i];

	if(p->node_p >= p->node_num) 
		array_needsize(unit_t, p->node_id, p->node_num, p->node_num + 1, array_zero_init);

	p->node_id[p->node_p] = n->user_id;
	p->node_p++;

	if(n->status == NEW)
		p->freq++;
}

static void node_pos_update(unit_t id)
{
	NODE *n = &nlist[id];
	if(n->pos_p >= n->pos_num)
		array_needsize(unit_t, n->pos_D, n->pos_num, n->pos_num + 1, array_zero_init);

	n->pos_D[n->pos_p] = n->pos_id;
	n->pos_p++;
}

static void node_flight_update(unit_t id, double l)
{
	NODE *n = &nlist[id];
	if(n->flight_p >= n->flight_num)
		array_needsize(double, n->flight_D, n->flight_num, n->flight_num + 1, array_zero_init);

	n->flight_D[n->flight_p] = l;
	n->flight_p++;
}

static void node_pause_update(unit_t id, unit_t t)
{
	NODE *n = &nlist[id];
	if(n->pause_p >= n->pause_num)
		array_needsize(unit_t, n->pause_D, n->pause_num, n->pause_num + 1, array_zero_init);

	n->pause_D[n->pause_p] = t;
	n->pause_p++;
}

static void neighbor_meeting_update(NEIGHBOR *n, unit_t p)
{
	if(n->meeting_p >= n->meeting_num)
		array_needsize(unit_t, n->meeting_pos, n->meeting_num, n->meeting_num + 1, array_zero_init);

	n->meeting_pos[n->meeting_p] = p;
	n->meeting_p++;
}

static void neighbor_meeting_add(unit_t neighbor, NODE *n)
{
	if(n->neighbor_p >= n->neighbor_num)
		array_needsize(NEIGHBOR, n->neighbor_D, n->neighbor_num, n->neighbor_num + 1, array_zero_init);

	NEIGHBOR *new = &(n->neighbor_D[n->neighbor_p]);
	new->id = neighbor;
	array_needsize(unit_t, new->meeting_pos, new->meeting_num, 100, array_zero_init);
	new->meeting_pos[new->meeting_p] = n->pos_id;
	
	new->meeting_p++;
	n->neighbor_p++;
}

static inline double rad(double d)
{
	return d * M_PI / 180.;
}

static inline double distance_cal(double x1, double y1, double x2, double y2)
{
	static const double EARTH_RADIUS = 6378.137;

	double rx1 = rad(x1);
	double rx2 = rad(x2);
	double a = rx1 - rx2;
	double b = rad(y1) - rad(y2);
	double s = 2 * asin(sqrt(pow(sin(a/2), 2) + cos(rx1) * cos(rx2) * pow(sin(b/2), 2)));
	s = s * EARTH_RADIUS;
	s = round(s * 10000) / 10000;
	return s;
}

static void get_node_info(unit_t i)
{
	NODE *n = &nlist[i];
	static size_t len = 0;
	ssize_t read;

	unit_t id, pos;
	double x,y;
	time_t time;

	if(n->time > timer)
		return;	//it means the node does not join the network yet

	if(n->time <= timer && n->next_time > timer) {
		if(n->status == UNINIT) {
			n->status = NEW;
			node_pos_update(i);
		}
		if(n->status == NEW)
			n->status = STAYING;

		pos_update(n);
		return;
	}

	if(n->next_time < timer) {	//now we need to try the next trace record
		unit_t n_pos;
		double n_x, n_y;
		time_t n_time;

		fseek(FP, n->fpos, SEEK_SET);
		read = getline(&line, &len, FP);
		parse_line(line, &id, &x, &y, &time, &pos);
NEXT:
		read = getline(&line, &len, FP);	//read the next trace record
		if(read == -1) {
			n->status = EXITING;
			return;
		}
		parse_line(line, &id, &n_x, &n_y, &n_time, &n_pos);
		if(id > i) {
			n->status = EXITING;
			return;
		}

		if(n_time < timer) {
			x = n_x;
			y = n_y;
			time = n_time;
			pos = n_pos;
			goto NEXT;
		}

		if(time <= timer && n_time > timer) {
			if(n->pos_id == pos) {
			//not moving
				n->status = STAYING;
				n->next_time = n_time;
				n->fpos = ftell(FP) - read;

				pos_update(n);
				return;
			}
			//MOVEING!!!
			unit_t pause_time = time - n->time;
			double flight = distance_cal(n->pos_x, n->pos_y, x, y);

			n->status = NEW;
			n->pos_id = pos;
			n->pos_x = x;
			n->pos_y = y;
			n->time = time;
			n->next_time = n_time;
			n->fpos = ftell(FP) - read;

			//update the statistic record of node
			node_pos_update(i);
			node_flight_update(i, flight);
			node_pause_update(i, pause_time);

			pos_update(n);
		}
	}
}

static bool node_run(unit_t id)
{
//replay the trace data based on the node id and system time. Then update fields in NODE
//return true if the node is in its lifetime
	if(nlist[id].status == EXITING)
		return false;

	get_node_info(id);

	if(nlist[id].status == EXITING)
		return false;

	return true;
}

static void record_pos(void)
{
	//TODO:
}

static void record_node_pos(void)
{
	//TODO:
}

static void record_node_flight(void)
{
	//TODO:
}

static void record_node_pause(void)
{
	//TODO:
}

static void record_node_neighbor(void)
{
	//TODO:
}

//TODO: we need also provide utility functions to analyze the simulation resutls!!!

static void save_simulation(void)
{
	//record the visiting freq of each pos
	record_pos();

	//record the visiting pos of each node
	record_node_pos();

	//record the flight of each node
	record_node_flight();

	//record the pause time of each node
	record_node_pause();

	//record the neighbor and corresponding meeting pos of each node
	record_node_neighbor();
}

static int cmp_nei(const void *n1, const void *n2)
{
	return (((NEIGHBOR *)n1)->id - ((NEIGHBOR *)n2)->id);
}

//update node id's neighbors
static void node_neighbor_update(unit_t id, POS *p)
{
	unit_t i;
	for(i=0; i<p->node_p; i++) {
		if(id == p->node_id[i])
			continue;

		NODE *n = &nlist[id];
		NEIGHBOR key, *res;
		key.id = p->node_id[i];
		res = bsearch(&key, n->neighbor_D, n->neighbor_p, sizeof(NEIGHBOR), cmp_nei);
		if(res) {	//update the neighbor record
			neighbor_meeting_update(res, n->pos_id);
		}
		else {	//new neighbor
			neighbor_meeting_add(/*neighbor*/p->node_id[i], n);
			//sort for the next search
			qsort(n->neighbor_D, n->neighbor_p, sizeof(NEIGHBOR), cmp_nei);
		}
	}
}

static void node_update(unit_t id)
{
	if(nlist[id].status != NEW)
		return;

	POS *p = &plist[nlist[id].pos_id];
	if(p->update)
		return;	//have been updated already 

	//update the neighbors of nodes on this pos
	unit_t i;
	for(i=0; i<p->node_p; i++) {
		node_neighbor_update(/*which node*/p->node_id[i], /*pos info*/p);
	}
	p->update = true;

	//TODO: add data transmition function here...
}

static void plist_clear(void)
{
	unit_t i;
	for(i=0; i<pos_num; i++) {
		plist[i].update = 0;
		plist[i].node_p = 0;
	}
}

int main(int argc, char *argv[])
{
	if(argc < 2) {
		printf("need trace file");
		exit(1);
	}	

	init_file(argv[1]);

	init_struct(argv[1]);
	
//start to run
	bool r_status = false;
	while(1) {
		//replay the trace for each nodes first
		unit_t i;
		for(i=0; i<nodes_num; i++) 
			r_status |= node_run(i);

		if(r_status == false) {
			save_simulation();
			printf("simulation done!\n");
			break;
		}
		
		//after finish simulation at time t, update the neighbor record for each node and etc.
		for(i=0; i<nodes_num; i++)
			node_update(i);

		timer += TIME_SLOT;
		r_status = false;
		plist_clear();	//reset plist every time...
	}

	free_struct();
	fclose(FP);

	return 0;
}

