#ifndef _NODES_H
#define _NODES_H

#define WB_BUFFLEN	2000
#define QUEUE_LEN	50

#define MAX_MSGLST	10

//#define MULTICAST

/*
 * MAX_HOPS = 3 single relay
 * MAX_HOPS > 3 multiple relays
 * MAX_HOPS = 0 no hop limitation
 * */
#define MAX_HOPS	0
/*
 * MAX_COPY = -1 no copy limitation
 * */
#define MAX_COPY	200

typedef enum node_status {
	UNINIT=0,
	NEW,
	STAYING,
	EXITING
} NODE_STATUS;

typedef struct data {
	unit_t id;		//node id
	bool status;
	time_t delay;
	unit_t hops;
	unit_t *path;
} DATA; 

typedef struct data_lst {
	unit_t id;		//msg id
	bool deliver;
	unit_t copy;
	DATA *dst;
	unit_t dst_len;
} DATA_LST;

typedef struct neighbor {
	unit_t id;	//neighbor id

	unit_t *meeting_delay;	//record the inter-contact meeting time
	unit_t *meeting_pos;	//record all the meeting pos

	unit_t meeting_p;
	unit_t meeting_num;

	unit_t r_timer;		//last time the two nodes meet
} __attribute__ ((packed)) NEIGHBOR;

typedef struct message {
	unit_t id;	//id of this msg
	unit_t max_hops;	//0 means no hop limit
	int status;		//0: needs to send; 1: already delivered; 2: reach the copy limitation; 3: reach the trans round limitation
	int copy;		//-1: no limitation; >= 1: max copy of data on the node could be sent out before setting status = 1
	unit_t cnt;		//record how many times the data has been copied when copy = -1

	int i_hop;	//# of hops within a community
	int i_tries;	//# of trans round with in a community

	int c_copy;	//# of copies a node could re-trans if failed in a community
	int c_tries;	//# of trans round among different communities

	bool new_recv;	// true: received this data in this time slot; false: old data received in previous time slots
	
	unit_t src;
	unit_t *dst_set;
	unit_t set_len;
	time_t time;

	unit_t hops;
	unit_t hopc;
	unit_t *dpath;
} __attribute__ ((packed)) MSG;

typedef struct node {
	unit_t fpos; //the pos of current record in trace file
	unit_t user_id;	
	double pos_x, pos_y;
	time_t time, next_time, s_time;
	unit_t pos_id;
	NODE_STATUS status;

	NEIGHBOR *neighbor_D;	//neighbor distribution
	unit_t neighbor_num;
	unit_t neighbor_p;

	unit_t *pos_D;		//the visiting pos
	unit_t pos_num;
	unit_t pos_p;

	double *flight_D;	//flight distribution
	unit_t flight_num;
	unit_t flight_p;
	
	unit_t *pause_D;		//pause time distribution
	unit_t pause_num;
	unit_t pause_p;

	MSG *buffer;
	unit_t buffer_num;
	unit_t buffer_p;
} __attribute__ ((packed)) NODE;

//store the pos info at every timestamp
typedef struct pos {
	bool update;
	unit_t node_p;
	unit_t *node_id;	//list of nodes on this pos at time t
	unit_t node_num;	
	unit_t pos_id;
	unit_t freq;
} POS;

typedef struct n_taular {
	unit_t node_id;
	unit_t neighbor_id;
} NT;

typedef struct wb_monitor {
	//flight monitor
	unit_t *flight_m;
	unit_t fm_num;
	unit_t fm_p;
	//flight wb
	pthread_t f_tid;
	pthread_mutex_t f_mtx;
	pthread_cond_t f_req;
	FILE *flight;

	//node pos monitor
	unit_t *pos_m;
	unit_t pom_num;
	unit_t pom_p;
	//pos wb
	pthread_t po_tid;
	pthread_mutex_t po_mtx;
	pthread_cond_t po_req;
	FILE *pos;

	//pause monitor
	unit_t *pause_m;
	unit_t pam_num;
	unit_t pam_p;
	//pause wb
	pthread_t pa_tid;
	pthread_mutex_t pa_mtx;
	pthread_cond_t pa_req;
	FILE *pause;

	//neighbor monitor
	unit_t *cneighbor_m;
	unit_t cnm_num;
	unit_t cnm_p;
	
	//neighbor meeting monitor
	NT *neighbor_m;
	unit_t nm_num;
	unit_t nm_p;

	//neighbor wb
	pthread_t neighbor_pos_tid;
	pthread_mutex_t neighbor_pos_mtx;
	pthread_cond_t neighbor_pos_req;
	FILE *neighbor_pos;

	pthread_t neighbor_time_tid;
	pthread_mutex_t neighbor_time_mtx;
	pthread_cond_t neighbor_time_req;
	FILE *neighbor_time;
} WM;

#endif
