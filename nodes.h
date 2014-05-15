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
}NEIGHBOR;

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
}NODE;

//store the pos info at every timestamp
typedef struct pos {
	unit_t *node_id;	//list of nodes on this pos at time t
	unit_t node_num;	
	unit_t node_p;
	bool update;
	unit_t freq;
}POS;

#endif
