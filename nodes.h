#ifndef _NODES_H
#define _NODES_H

typedef unsigned long unit_t;

typedef enum node_status {
	UNINIT=0,
	NEW,
	STAYING,
	EXITING
}NODE_STATUS;

typedef struct neighbor {
	unit_t id;	//neighbor id

	unit_t *meeting_pos;	//record all the meeting pos
	unit_t meeting_p;
	unit_t meeting_num;
} __attribute__ ((packed)) NEIGHBOR;

typedef struct node {
	unit_t fpos; //the pos of current record in trace file
	unit_t user_id;	
	double pos_x, pos_y;
	time_t time, next_time;
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
} __attribute__ ((packed)) NODE;

//store the pos info at every timestamp
typedef struct pos {
	unit_t *node_id;	//list of nodes on this pos at time t
	unit_t node_num;	
	unit_t node_p;
	unit_t pos_id;
	unit_t freq;
	bool update;
} __attribute__ ((packed)) POS;

typedef struct n_taular {
	unit_t node_id;
	unit_t neighbor_id;
} NT;

typedef struct wb_monitor {
	//flight monitor
	unit_t *flight_m;
	unit_t fm_num;
	unit_t fm_p;

	//node pos monitor
	unit_t *pos_m;
	unit_t pom_num;
	unit_t pom_p;

	//pause monitor
	unit_t *pause_m;
	unit_t pam_num;
	unit_t pam_p;

	//neighbor monitor
	unit_t *cneighbor_m;
	unit_t cnm_num;
	unit_t cnm_p;
	
	//neighbor meeting monitor
	NT *neighbor_m;
	unit_t nm_num;
	unit_t nm_p;
} WM;

#endif
