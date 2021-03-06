#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <gsl/gsl_rng.h>

#include "common.h"
#include "nodes.h"
#include "fifo.h"
#include "flood_trans.h"

#define CMDLEN	512
#define LINELEN	512
#define TIME_SLOT	1800	//30 min
//#define SLEEP_T	1000

NODE *nlist;
DATA_LST MSG_LST[MAX_MSGLST] = {0};
time_t timer;
FILE *fmulti_path;
FILE *f_infect;
gsl_rng *Rng_r;

static POS *plist;
static unit_t nodes_num;
static unit_t pos_num;
static FILE *FP;
static unit_t running_node;
static char *line = NULL;
static WM monitor = {0};

//wb buffer
static char fb[WB_THRESHOLD][WB_BUFFLEN];	//node id,flight1,flight2,...\r\n
static char pob[WB_THRESHOLD][WB_BUFFLEN];	//node id,pos1,pos2,...\r\n
static char pab[WB_THRESHOLD][WB_BUFFLEN];	//node id,pause1,pause2,...\r\n
static char nb[WB_THRESHOLD][WB_BUFFLEN];	//node id,neighbor id,meeting_pos1,meeting_pos2,...\r\n
static char ntb[WB_THRESHOLD][WB_BUFFLEN];	//node id,neighbor id,0,inter-contact time1,inter-contact time2,...\r\n

static unsigned int fb_num; //always mod WB_THRESHOLD
static unsigned int pob_num;
static unsigned int pab_num;
static unsigned int nb_num;

static FIFO *fb_queue;
static FIFO *pob_queue;
static FIFO *pab_queue;
static FIFO *nb_queue;
static FIFO *ntb_queue;

static unit_t MSG_ID = 0;

#define fb_i	(fb_num & (WB_THRESHOLD - 1))
#define pob_i	(pob_num & (WB_THRESHOLD - 1))
#define pab_i	(pab_num & (WB_THRESHOLD - 1))
#define nb_i	(nb_num & (WB_THRESHOLD - 1))

#define general_wb(fifo, req, mtx, file)	\
	{	\
		fifo_data_t tmp;	\
		size_t len;	\
		BlockSignal(true, SIGUSR1);	\
		for(;;) {	\
			while(_fifo_get((fifo), &tmp) != 0)	\
				pthread_cond_wait(&(req), &(mtx));	\
			len = strlen((char *)tmp);	\
			if(fwrite(tmp, sizeof(char), len, (file)) != len && ferror(file))	\
					perror("fwrite flight");	\
		}	\
	}

void *tf_wb(void *arg)
{
	general_wb(fb_queue, monitor.f_req, monitor.f_mtx, monitor.flight);
}

void *tpo_wb(void *arg)
{
	general_wb(pob_queue, monitor.po_req, monitor.po_mtx, monitor.pos);
}

void *tpa_wb(void *arg)
{
	general_wb(pab_queue, monitor.pa_req, monitor.pa_mtx, monitor.pause);
}

void *tnp_wb(void *arg)
{
	general_wb(nb_queue, monitor.neighbor_pos_req, monitor.neighbor_pos_mtx, monitor.neighbor_pos);
}

void *tnt_wb(void *arg)
{
	general_wb(ntb_queue, monitor.neighbor_time_req, monitor.neighbor_time_mtx, monitor.neighbor_time);
}

static void init_file(const char *file)
{
#define CMD_FORMAT	"sort -s -t , -k1,1n -k4,4 %s -o ./_tmp; mv ./_tmp %s"
	char cmd[CMDLEN] = {0};
	sprintf(cmd, CMD_FORMAT, file, file);
	cmd_system(cmd);

	FP = fopen(file, "r");
	if(FP == NULL) {
		printf("trace file failed to open");
		exit(1);
	}

#ifndef MULTICAST
	fmulti_path = fopen("Indiv_sim.csv", "w");
	if(fmulti_path == NULL) {
		printf("fail to create Indiv simulation log file");
		exit(1);
	}
#endif

	f_infect = fopen("./infect_nodes.csv", "w");
	if(f_infect == NULL) {
		printf("fail to create infected nodes log file");
		exit(1);
	}

#undef CMD_FORMAT
}

static inline void parse_line(const char *l, unit_t *id, double *x, double *y, time_t *time, unit_t *pos_id)
{
	char tmp[32] = {0};
	struct tm t = {0};
	sscanf(l, LINE_FORMAT, id, x, y, tmp, pos_id);

	*time = convert_time(tmp);
}

static unit_t init_fpos(unit_t id)
{
	static size_t len = 0;
	static unit_t id_pos = 0;
	ssize_t read;
	double x,y;
	time_t time;
	unit_t pos, res;
	
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
			
			//set info back
			id_pos = id;
			res = ftell(FP) - read;
			fseek(FP, res, SEEK_SET);

			return res;
		}
		if(id_pos > id) {
			nlist[id].status = EXITING;	//do not find this node record, put EXITING status
			res = ftell(FP) - read;
			fseek(FP, res, SEEK_SET);
			id_pos = id;
			break;
		}
	}
}

static void init_node(NODE *n, unit_t id)
{
	n->user_id = id;
	n->fpos = init_fpos(id);
	if(n->status == EXITING)	//this node does not exist...
		return;
	
	array_needsize(false, NEIGHBOR, n->neighbor_D, n->neighbor_num, 10, array_zero_init);

	//wb if num of visiting pos bigger than WB_THRESHOLD
	array_needsize(true, unit_t, n->pos_D, n->pos_num, 10, array_zero_init);

	//wb if num of flight bigger than WB_THRESHOLD
	array_needsize(true, double, n->flight_D, n->flight_num, 10, array_zero_init);

	//wb if num of pause bigger than WB_THRESHOLD
	array_needsize(true, unit_t, n->pause_D, n->pause_num, 10, array_zero_init);

	array_needsize(false, MSG, n->buffer, n->buffer_num, 2, array_zero_init);
}

static inline void init_pos(POS *p)
{
	array_needsize(false, unit_t, p->node_id, p->node_num, 10, array_zero_init);
}

static void init_wb(void)
{
	array_needsize(false, unit_t, monitor.flight_m, monitor.fm_num, 10, array_zero_init);
	array_needsize(false, unit_t, monitor.pos_m, monitor.pom_num, 10, array_zero_init);
	array_needsize(false, unit_t, monitor.pause_m, monitor.pam_num, 10, array_zero_init);
	array_needsize(false, unit_t, monitor.cneighbor_m, monitor.cnm_num, 10, array_zero_init);
	array_needsize(false, NT, monitor.neighbor_m, monitor.nm_num, 10, array_zero_init);
}

static void init_monitor(void)
{
	monitor.f_mtx = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	monitor.f_req = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
	monitor.flight = fopen("./node_flight.csv", "w");

	monitor.po_mtx = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	monitor.po_req = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
	monitor.pos = fopen("./node_pos.csv", "w");

	monitor.pa_mtx = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	monitor.pa_req = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
	monitor.pause = fopen("./node_pause.csv", "w");

	monitor.neighbor_pos_mtx = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	monitor.neighbor_pos_req = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
	monitor.neighbor_pos = fopen("./node_neighbor_pos.csv", "w");

	monitor.neighbor_time_mtx = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	monitor.neighbor_time_req = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
	monitor.neighbor_time = fopen("./node_neighbor_time.csv", "w");

	//start wb threads at last
	pthread_create(&(monitor.f_tid), NULL, tf_wb, NULL);
	pthread_create(&(monitor.po_tid), NULL, tpo_wb, NULL);
	pthread_create(&(monitor.pa_tid), NULL, tpa_wb, NULL);
	pthread_create(&(monitor.neighbor_pos_tid), NULL, tnp_wb, NULL);
	pthread_create(&(monitor.neighbor_time_tid), NULL, tnt_wb, NULL);
}

static void init_fifo(void)
{
	fb_queue = fifo_alloc(QUEUE_LEN);
	pob_queue = fifo_alloc(QUEUE_LEN);
	pab_queue = fifo_alloc(QUEUE_LEN);
	nb_queue = fifo_alloc(QUEUE_LEN);
	ntb_queue = fifo_alloc(QUEUE_LEN);
}

static void init_rng(void)
{
	const gsl_rng_type *T;
	gsl_rng_env_setup();

	T = gsl_rng_default;
	Rng_r = gsl_rng_alloc(T);
}

static void init_struct(const char *file)
{
	char cmd[CMDLEN] = {0};
	sprintf(cmd, "tail -1 %s | cut -d , -f 1", file);
	nodes_num = atol(cmd_system(cmd)) + 1;
	printf("node: %ld\n", nodes_num);

	cmd[0] = 0;
	sprintf(cmd, "sort -t, -k5,5nr %s | head -1 | cut -d , -f 5", file);
	pos_num = atol(cmd_system(cmd)) + 1;
	printf("pos: %ld\n", pos_num);

	cmd[0] = 0;
	struct tm time = {0};
	sprintf(cmd, "sort -t, -k4,4 %s | head -1 | cut -d , -f 4", file);
	timer = convert_time(cmd_system(cmd));
	printf("time: %ld\n", timer);

	nlist = (NODE *)calloc(nodes_num, sizeof(NODE));
	plist = (POS *)calloc(pos_num, sizeof(POS));

	unit_t i;
	for(i=0; i<nodes_num; i++) 
		init_node(&nlist[i], i);

	for(i=0; i<pos_num; i++) {
		init_pos(&plist[i]);
		plist[i].pos_id = i;
	}

	//init fifo first
	init_fifo();

	init_wb();

	init_monitor();

	init_rng();
}

static void free_node(NODE *n)
{
	if(n) {
		_free(n->pos_D);
		_free(n->flight_D);
		_free(n->pause_D);

		unit_t i;
		for(i=0; i<n->neighbor_num; i++) {
			_free(n->neighbor_D[i].meeting_pos);
			_free(n->neighbor_D[i].meeting_delay);
		}
			
		_free(n->neighbor_D);

		for(i=0; i<n->buffer_num; i++) {
			_free(n->buffer[i].dpath);
			_free(n->buffer[i].dst_set);
		}
		_free(n->buffer);
	}
}

#define wm_free	\
	do {	\
		if(monitor.flight_m) {	\
			free(monitor.flight_m);	\
			monitor.flight_m = NULL;	\
		}	\
		if(monitor.pos_m) {	\
			free(monitor.pos_m);	\
			monitor.pos_m = NULL;	\
		}	\
		if(monitor.pause_m) {	\
			free(monitor.pause_m);	\
			monitor.pause_m = NULL;	\
		}	\
		if(monitor.neighbor_m) {	\
			free(monitor.neighbor_m);	\
			monitor.neighbor_m = NULL;	\
		}	\
	}while(0)

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
			_free(plist[i].node_id);
		}
		free(plist);
	}
	plist = NULL;

	if(line) {
		free(line);
		line = NULL;
	}

	pthread_mutex_destroy(&(monitor.f_mtx));
	pthread_cond_destroy(&(monitor.f_req));

	pthread_mutex_destroy(&(monitor.po_mtx));
	pthread_cond_destroy(&(monitor.po_req));

	pthread_mutex_destroy(&(monitor.pa_mtx));
	pthread_cond_destroy(&(monitor.pa_req));

	pthread_mutex_destroy(&(monitor.neighbor_pos_mtx));
	pthread_cond_destroy(&(monitor.neighbor_pos_req));

	pthread_mutex_destroy(&(monitor.neighbor_time_mtx));
	pthread_cond_destroy(&(monitor.neighbor_time_req));

	wm_free;

	fifo_free(fb_queue);
	fifo_free(pob_queue);
	fifo_free(pab_queue);
	fifo_free(nb_queue);
	fifo_free(ntb_queue);

	gsl_rng_free(Rng_r);
}

static void pos_update(const NODE *n)
{
	unit_t i = n->pos_id;
	POS *p = &plist[i];

	if(p->node_p >= p->node_num) 
		array_needsize(false, unit_t, p->node_id, p->node_num, p->node_num + 1, array_zero_init);

	p->node_id[p->node_p] = n->user_id;
	p->node_p++;

	if(n->status == NEW)
		p->freq++;
}

static void wm_pos_add(unit_t id)
{
	if(monitor.pom_p >= monitor.pom_num)
		array_needsize(false, unit_t, monitor.pos_m, monitor.pom_num, monitor.pom_num + 1, array_zero_init);

	monitor.pos_m[monitor.pom_p] = id;
	monitor.pom_p++;
}

static void node_pos_update(unit_t id)
{
	NODE *n = &nlist[id];
	if(n->pos_p >= n->pos_num)
		array_needsize(true, unit_t, n->pos_D, n->pos_num, n->pos_num + 1, array_zero_init);

	n->pos_D[n->pos_p] = n->pos_id;
	n->pos_p++;

	if(n->pos_p >= WB_THRESHOLD) 
		wm_pos_add(id);	
}

static void wm_flight_add(unit_t id)
{
	if(monitor.fm_p >= monitor.fm_num)
		array_needsize(false, unit_t, monitor.flight_m, monitor.fm_num, monitor.fm_num + 1, array_zero_init);

	monitor.flight_m[monitor.fm_p] = id;
	monitor.fm_p++;
}

static void node_flight_update(unit_t id, double l)
{
	NODE *n = &nlist[id];
	if(n->flight_p >= n->flight_num)
		array_needsize(true, double, n->flight_D, n->flight_num, n->flight_num + 1, array_zero_init);

	n->flight_D[n->flight_p] = l;
	n->flight_p++;

	if(n->flight_p >= WB_THRESHOLD)
		wm_flight_add(id);
}

static void wm_pause_add(unit_t id)
{
	if(monitor.pam_p >= monitor.pam_num)
		array_needsize(false, unit_t, monitor.pause_m, monitor.pam_num, monitor.pam_num + 1, array_zero_init);

	monitor.pause_m[monitor.pam_p] = id;
	monitor.pam_p++;
}

static void node_pause_update(unit_t id, unit_t t)
{
	NODE *n = &nlist[id];
	if(n->pause_p >= n->pause_num)
		array_needsize(true, unit_t, n->pause_D, n->pause_num, n->pause_num + 1, array_zero_init);

	n->pause_D[n->pause_p] = t;
	n->pause_p++;

	if(n->pause_p >= WB_THRESHOLD)
		wm_pause_add(id);
}

static void neighbor_meeting_update(NEIGHBOR *n, unit_t p)
{
	unit_t t_num = n->meeting_num;
	if(n->meeting_p >= t_num) {
		array_needsize(true, unit_t, n->meeting_pos, t_num, t_num + 1, array_zero_init);
		t_num = n->meeting_num;

		array_needsize(true, unit_t, n->meeting_delay, t_num, t_num + 1, array_zero_init);
		n->meeting_num = t_num;
	}

	n->meeting_pos[n->meeting_p] = p;
	n->meeting_delay[n->meeting_p] = timer - n->r_timer;
	n->r_timer = timer;
	n->meeting_p++;
}

static void wm_cneighbor_add(unit_t id)
{
	if(monitor.cnm_p >= monitor.cnm_num)
		array_needsize(false, unit_t, monitor.cneighbor_m, monitor.cnm_num, monitor.cnm_num + 1, array_zero_init);

	monitor.cneighbor_m[monitor.cnm_p] = id;
	monitor.cnm_p++;
}

static void neighbor_meeting_add(unit_t neighbor, NODE *n)
{
	if(n->neighbor_p >= n->neighbor_num)
		array_needsize(false, NEIGHBOR, n->neighbor_D, n->neighbor_num, n->neighbor_num + 1, array_zero_init);

	NEIGHBOR *new = &(n->neighbor_D[n->neighbor_p]);
	new->id = neighbor;
	//wb if num of meeting pos bigger than WB_THRESHOLD
	if(new->meeting_num == 0) {
		array_needsize(true, unit_t, new->meeting_pos, new->meeting_num, 10, array_zero_init);
		new->meeting_delay = (unit_t *)calloc(new->meeting_num, sizeof(unit_t));
	}

	new->meeting_pos[new->meeting_p] = n->pos_id;
//	if(new->r_timer == 0)
		new->meeting_delay[new->meeting_p] = 0;	//alway to be 0 for the first time
//	else
//		new->meeting_delay[new->meeting_p] = timer - new->r_timer;

	new->r_timer = timer;
	new->meeting_p++;
	
	n->neighbor_p++;
	if(n->neighbor_p >= WB_THRESHOLD)
		wm_cneighbor_add(n->user_id);
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

static bool msg_genPro(void)
{
// 0.1 probability to generate a msg...
	int a = (rand() % 1000) + 1;
	if(a < 100)
		return true;
	else
		return false;
}

static void node_make_msg(NODE *n)
{
//generate the new msg for data dissemination
#define SRC	0
	if(MSG_ID == MAX_MSGLST)
		return;

/*
	if(msg_genPro() == false)
		return;
*/

	if(n->user_id != SRC)
		return;

	int i;
	if(n->buffer_p == n->buffer_num)
		array_needsize(false, MSG, n->buffer, n->buffer_num, n->buffer_num + 1, array_zero_init);
	
	MSG *new_msg = &(n->buffer[n->buffer_p]);
	msg_generate(new_msg, n->user_id, timer, MAX_HOPS, MSG_ID);

	MSG_LST[MSG_ID].id = MSG_ID;
	MSG_LST[MSG_ID].dst_len = new_msg->set_len;
	MSG_LST[MSG_ID].dst = (DATA *)calloc(new_msg->set_len, sizeof(DATA));

	for(i=0; i<new_msg->set_len; i++)
		MSG_LST[MSG_ID].dst[i].id = new_msg->dst_set[i];

	MSG_ID++;
	n->buffer_p++;
#undef SRC
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

	if(n->time <= timer && n->next_time >= timer) {
		if(n->status == UNINIT) {
			n->s_time = timer;
			n->status = NEW;
			node_pos_update(i);

			node_make_msg(n);

		}
		else if(n->status == NEW)
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

		if(n_time <= timer) {
			x = n_x;
			y = n_y;
			time = n_time;
			pos = n_pos;
			goto NEXT;
		}

		if(time <= timer && n_time > timer) {
		// not init yet...
			if(n->status == UNINIT) {
				n->s_time = timer;
				n->status = NEW;
				n->pos_id = pos;
				n->pos_x = x;
				n->pos_y = y;
				n->time = time;
				n->next_time = n_time;
				n->fpos = ftell(FP) - read;

				node_pos_update(i);
				pos_update(n);

				node_make_msg(n);
				return;
			}

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

			node_make_msg(n);
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
	FILE *fp = fopen("./pos_visiting.csv","w");
	unit_t i;
	char pos_str[32] = {0};
	char *tmp;
	POS *pt;
	for(i=0; i<pos_num; i++) {
		//reset tmp every time
		tmp = pos_str;
		tmp[0] = 0;
		pt = &(plist[i]);
		sprintf(tmp, "%ld,%ld\r\n", pt->pos_id, pt->freq);
		fwrite(tmp, sizeof(char), strlen(tmp), fp);
	}

	fclose(fp);
}

static void record_node_pos(void)
{
	unit_t i;
	char npos_str[WB_BUFFLEN] = {0};
	char *tmp;
	NODE *n;
	for(i=0; i<nodes_num; i++) {
		n = &(nlist[i]);
		if(n->pos_p) {
			tmp = npos_str;
			tmp[0] = 0;
			tmp += sprintf(tmp, "%ld,", i);
			int_to_string(tmp, n->pos_D, n->pos_p);	
			fwrite(npos_str, sizeof(char), strlen(npos_str), monitor.pos);
		}
	}

	fclose(monitor.pos);
}

static void record_node_flight(void)
{
	unit_t i;
	char nflight_str[WB_BUFFLEN] = {0};
	char *tmp;
	NODE *n;
	for(i=0; i<nodes_num; i++) {
		n = &(nlist[i]);
		if(n->flight_p) {
			tmp = nflight_str;
			tmp[0] = 0;
			tmp += sprintf(tmp, "%ld,", i);
			double_to_string(tmp, n->flight_D, n->flight_p);	
			fwrite(nflight_str, sizeof(char), strlen(nflight_str), monitor.flight);
		}
	}

	fclose(monitor.flight);
}

static void record_node_pause(void)
{
	unit_t i;
	char npause_str[WB_BUFFLEN] = {0};
	char *tmp;
	NODE *n;
	for(i=0; i<nodes_num; i++) {
		n = &(nlist[i]);
		if(n->pause_p) {
			tmp = npause_str;
			tmp[0] = 0;
			tmp += sprintf(tmp, "%ld,", i);
			int_to_string(tmp, n->pause_D, n->pause_p);	
			fwrite(npause_str, sizeof(char), strlen(npause_str), monitor.pause);
		}
	}

	fclose(monitor.pause);
}

static void record_node_neighbor(void)
{
	unit_t i, j;
	char neighbor_str[WB_BUFFLEN] = {0};
	char *tmp;
	NODE *n;
	NEIGHBOR *ne;
	for(i=0; i<nodes_num; i++) {
		n = &nlist[i];
		if(n->neighbor_p) {
			for(j=0; j<n->neighbor_p; j++) {
				ne = &(n->neighbor_D[j]);
				//neighbor pos
				tmp = &neighbor_str[0];
				tmp[0] = 0;
				tmp += sprintf(tmp, "%ld,%ld,", i, ne->id);
				int_to_string(tmp, ne->meeting_pos, ne->meeting_p);
				fwrite(neighbor_str, sizeof(char), strlen(neighbor_str), monitor.neighbor_pos);
				//inter-contact time
				tmp = &neighbor_str[0];
				tmp[0] = 0;
				tmp += sprintf(tmp, "%ld,%ld,", i, ne->id);
				int_to_string(tmp, ne->meeting_delay, ne->meeting_p);
				fwrite(neighbor_str, sizeof(char), strlen(neighbor_str), monitor.neighbor_time);
			}
		}
	}

	fclose(monitor.neighbor_pos);
	fclose(monitor.neighbor_time);
}

//TODO: we need also provide utility functions to analyze the simulation resutls!!!

static void save_simulation(void)
{
	//record the neighbor and corresponding meeting pos of each node
	record_node_neighbor();

	//record the visiting pos of each node
	record_node_pos();

	//record the flight of each node
	record_node_flight();

	//record the pause time of each node
	record_node_pause();

	//record the visiting freq of each pos
	record_pos();
}

static int cmp_nei(const void *n1, const void *n2)
{
	return (((NEIGHBOR *)n1)->id - ((NEIGHBOR *)n2)->id);
}

static void wm_neighbor_add(unit_t node_id, unit_t neighbor_id)
{
	if(monitor.nm_p >= monitor.nm_num)
		array_needsize(false, NT, monitor.neighbor_m, monitor.nm_num, monitor.nm_num + 1, array_zero_init);

	monitor.neighbor_m[monitor.nm_p].node_id = node_id;
	monitor.neighbor_m[monitor.nm_p].neighbor_id = neighbor_id;
	monitor.nm_p++;
}

//update node id's neighbors
static void node_neighbor_update(unit_t index, POS *p)
{
	unit_t i;
	NODE *n = &nlist[p->node_id[index]];

	for(i=0; i<p->node_p; i++) {
		if(index >= i ||
			(n->status == STAYING &&
			 nlist[p->node_id[i]].status == STAYING))
			continue;

		NEIGHBOR key, *res;
		key.id = p->node_id[i];
		res = bsearch(&key, n->neighbor_D, n->neighbor_p, sizeof(NEIGHBOR), cmp_nei);
		if(res) {	//update the neighbor record
			neighbor_meeting_update(res, n->pos_id);
			if(res->meeting_p >= WB_THRESHOLD)
				wm_neighbor_add(p->node_id[index], res->id);
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
	if(p->update || p->node_p == 1)
		return;	//have been updated already or no other nodes on this pos

	//update the neighbors of nodes on this pos
	unit_t i;
	for(i=0; i<p->node_p; i++) {
		node_neighbor_update(/*index of which node on this pos*/i, /*pos info*/p);
	}
	p->update = true;

	//add data transmition function here...
	data_trans(&nlist[id], p);
}

static void plist_clear(void)
{
	unit_t i;
	for(i=0; i<pos_num; i++) {
		plist[i].update = 0;
		plist[i].node_p = 0;
	}
}

#define fifo_is_full(f) ((f)->size - (f)->in + (f)->out == 0 ? true : false)

#define fifo_test(f,s) \
	do {	\
		bool flag = false;	\
		while(fifo_is_full(f)) {	\
			if(flag) 	\
				continue;	\
			pthread_cond_signal(&(s));	\
			flag = true;	\
		}	\
	}while(0)

static void wm_flight_wb(void)
{
	unit_t i, id;
	NODE *n;
	int p;
	char *str;
	for(i=0; i<monitor.fm_p; i++){
		id = monitor.flight_m[i];	
		n = &nlist[id];
		{
			fifo_test(fb_queue, monitor.f_req);
			//convert flight into string format
			p = fb_i;
			str = fb[p];
			str[0] = 0;
			str += sprintf(str, "%ld,", id);
			double_to_string(str, n->flight_D, WB_THRESHOLD);
			//write to buffer
			_fifo_put(fb_queue, fb[p]);

			fb_num++;
		}
		n->flight_p = 0;
	}
	monitor.fm_p = 0;
//	printf("flight WB\n");
}


static void wm_pos_wb(void)
{
	unit_t i, id;
	NODE *n;
	int p;
	char *str;
	for(i=0; i<monitor.pom_p; i++) {
		id = monitor.pos_m[i];
		n = &nlist[id];
		{
			fifo_test(pob_queue, monitor.po_req);
			//convert visiting pos into string format
			p = pob_i;
			str = pob[p];
			str[0] = 0;
			str += sprintf(str, "%ld,", id);
			int_to_string(str, n->pos_D, WB_THRESHOLD);
			//write to buffer
			_fifo_put(pob_queue, pob[p]);

			pob_num++;
		}
		n->pos_p = 0;
	}
	monitor.pom_p = 0;
//	printf("pos WB\n");
}

static void wm_pause_wb(void)
{
	unit_t i, id;
	NODE *n;
	int p;
	char *str;
	for(i=0; i<monitor.pam_p; i++){
		id = monitor.pause_m[i];
		n = &nlist[id];
		{
			fifo_test(pab_queue, monitor.pa_req);
			//convert pause time into string format
			p = pab_i;
			str = pab[p];
			str[0] = 0;
			str += sprintf(str, "%ld,", id);
			int_to_string(str, n->pause_D, WB_THRESHOLD);
			//write to buffer
			_fifo_put(pab_queue, pab[p]);

			pab_num++;
		}
		n->pause_p = 0;
	}
	monitor.pam_p = 0;
//	printf("pause WB\n");
}

static void wm_cneighbor_wb(void)
{
	unit_t i, id;
	NODE *n;
	int p;
	char *str;
	for(i=0; i<monitor.cnm_p; i++) {
		id = monitor.cneighbor_m[i];
		n = &nlist[id];

		int j;
		for(j=0; j<n->neighbor_p; j++) {
			NEIGHBOR *tmp = &(n->neighbor_D[j]);
			{
				fifo_test(nb_queue, monitor.neighbor_pos_req);
				//convert meeting pos into string format
				p = nb_i;
				str = nb[p];
				str[0] = 0;
				str += sprintf(str, "%ld,%ld,", id, tmp->id);
				int_to_string(str, tmp->meeting_pos, tmp->meeting_p);
				//write to buffer
				_fifo_put(nb_queue, nb[p]);

				fifo_test(ntb_queue, monitor.neighbor_time_req);
				//convert inter-contact time into string format
				str = ntb[p];
				str[0] = 0;
				str += sprintf(str, "%ld,%ld,", id, tmp->id);
				int_to_string(str, tmp->meeting_delay, tmp->meeting_p);
				//write to buffer
				_fifo_put(ntb_queue, ntb[p]);

				nb_num++;
			}
			tmp->meeting_p = 0;
		}
		n->neighbor_p = 0;
	}
	monitor.cnm_p = 0;
}


static void wm_neighbor_wb(void)
{
	unit_t i, id, nid;
	NODE *n;
	NEIGHBOR key, *res, tmp;
	int p;
	char *str;

	for(i=0; i<monitor.nm_p; i++) {
		id = monitor.neighbor_m[i].node_id;
		nid = monitor.neighbor_m[i].neighbor_id;
		n = &nlist[id];
		key.id = nid;
		res = bsearch(&key, n->neighbor_D, n->neighbor_p, sizeof(NEIGHBOR), cmp_nei);

		{
			fifo_test(nb_queue, monitor.neighbor_pos_req);
			//convert meeting pos into string format
			p = nb_i;
			str = nb[p];
			str[0] = 0;
			str += sprintf(str, "%ld,%ld,", id, res->id);
			int_to_string(str, res->meeting_pos, WB_THRESHOLD);
			//write to buffer
			_fifo_put(nb_queue, nb[p]);
			
			fifo_test(ntb_queue, monitor.neighbor_time_req);
			//convert inter-contact time into string format
			str = ntb[p];
			str[0] = 0;
			str += sprintf(str, "%ld,%ld,", id, res->id);
			int_to_string(str, res->meeting_delay, WB_THRESHOLD);
			//write to buffer
			_fifo_put(ntb_queue, ntb[p]);

			nb_num++;
		}
		res->meeting_p = 0;

		//after writing back this block, we swap it to the last position of the array and re-sort the array
		memcpy(&tmp, res, sizeof(NEIGHBOR));
		memcpy(res, &(n->neighbor_D[n->neighbor_p - 1]), sizeof(NEIGHBOR));
		memcpy(&(n->neighbor_D[n->neighbor_p - 1]), &tmp, sizeof(NEIGHBOR));

		n->neighbor_p--;
		qsort(n->neighbor_D, n->neighbor_p, sizeof(NEIGHBOR), cmp_nei);
	}
	monitor.nm_p = 0;
//	printf("neighor WB\n");
}

#define wm_writeback 	\
	do {	\
		if(monitor.fm_p)	\
			wm_flight_wb();	\
		if(monitor.pom_p)	\
			wm_pos_wb();	\
		if(monitor.pam_p)	\
			wm_pause_wb();	\
		if(monitor.nm_p)	\
			wm_neighbor_wb();	\
		if(monitor.cnm_p)	\
			wm_cneighbor_wb();	\
	}while(0)

#define flush_wb_buff	\
	do {	\
		pthread_cond_signal(&(monitor.f_req));	\
		pthread_cond_signal(&(monitor.po_req));	\
		pthread_cond_signal(&(monitor.pa_req));	\
		pthread_cond_signal(&(monitor.neighbor_pos_req));	\
		pthread_cond_signal(&(monitor.neighbor_time_req));	\
		sleep(1);	\
	} while(0)

static void print_runtime(int para)
{
	BlockSignal(true, SIGUSR1);
	printf("runing time: %s\n", ctime(&timer));
	BlockSignal(false, SIGUSR1);
}

static void wm_msg_wb(void)
{
	FILE *fp = fopen("./msg_deliver.csv", "w");
	char path_str[WB_BUFFLEN] = {0};
	char *str;
	
	int i;
	for(i=0; i<MSG_ID; i++) {
		DATA_LST *tmp = &MSG_LST[i];
		fprintf(fp, "id, deliver, copy\r\n");
		fprintf(fp, "%ld,%d,%ld\r\n", tmp->id, tmp->deliver, tmp->copy);
		int j;
		for(j=0; j<tmp->dst_len;j++) {
			DATA *tmp1 = &(tmp->dst[j]);
			fprintf(fp, "dst,status,delay,hops\r\n");
			fprintf(fp, "%ld,%d,%ld,%ld\r\n", tmp1->id, tmp1->status, tmp1->delay, tmp1->hops);
			if(tmp1->status) {
				fprintf(fp, "path:\r\n");
				str = path_str;
				str[0] = 0;
				int_to_string(str, tmp1->path, tmp1->hops);
				fwrite(path_str, sizeof(char), strlen(path_str), fp);
			}
			_free(tmp1->path);
		}
		_free(tmp->dst);
	}
	
	fclose(fp);
}

int main(int argc, char *argv[])
{
	if(argc < 2) {
		printf("need trace file");
		exit(1);
	}	

	init_file(argv[1]);

	init_struct(argv[1]);

	BlockSignal(false, SIGUSR1);
	CatchSignal(SIGUSR1, print_runtime);
	
//start to run
	bool r_status = false;
	printf("start to run...\n");

	for(;;) {
		//replay the trace for each nodes first
		unit_t i;
		for(i=0; i<nodes_num; i++) 
			r_status |= node_run(i);

		if(r_status == false) {
			flush_wb_buff;
			save_simulation();
			printf("simulation done!\n");
			break;
		}
		
		//after finish simulation at time t, update the neighbor record for each node and etc.
		for(i=0; i<nodes_num; i++)
			node_update(i);

		timer += TIME_SLOT;
		r_status = false;
		
		//see if we need write back any record
		wm_writeback;

		//reset plist very time
		plist_clear();
	}

	free_struct();
	fclose(FP);
#ifndef MULTICAST
	fclose(fmulti_path);
#endif
	
	wm_msg_wb();

	return 0;
}

