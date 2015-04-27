#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <gsl/gsl_rng.h>

#include "common.h"
#include "nodes.h"

#define RELATION_FILE	"./gowalla_edges.txt"
#define NODE_ID_FILE	"./node.id"

extern DATA_LST MSG_LST[MAX_MSGLST];
extern NODE *nlist;
extern time_t timer;
extern FILE *fmulti_path;
extern gsl_rng *Rng_r;
extern FILE *f_infect;

/*
 * if we use this scheme, we need to set:
 *  	TRANS_SCHEME = 2
 *  	uncomment SIMPLE_INFECT
 * */
#define COMMUNITY_INFECT

#define TRANS_SCHEME	2
//#define FLOODING
//#define SIMPLE_INFECT
#define NEIGHBOR_PORTION	2
//#define DYNAMIC_COPY

#define MAX_IHOP	3
#define MAX_ITRIES	2
#define MAX_CTRIES	5
#define MAX_CCOPY	5

static void msg_status_update(DATA_LST *d)
{
	int i;
	for(i=0; i<d->dst_len; i++) {
		if(d->dst[i].status == false)
			return;
	}

	d->deliver = true;
}

static bool check_node_id(FILE *fp, unit_t id)
{
	ssize_t read;
	size_t len = 0;
	char *line = NULL;

	while((read = getline(&line, &len, fp)) != -1) {
		unit_t n;
		sscanf(line, "%ld", &n);
		if(n < id)
			continue;
		if(n == id) {
			free(line);
			return true;
		}
		if(n > id) {
			//roll back
			fseek(fp, ftell(fp) - read, SEEK_SET);

			free(line);
			return false;
		}
	}	

	if(line)
		free(line);

	return false;
}

void msg_generate(MSG *new, unit_t src, time_t time, unit_t max_hops, unit_t msg_id)
{
	new->src = src;
	new->time = time;
	new->max_hops = max_hops;
	new->status = 0;
	new->copy = MAX_COPY;
	new->cnt = 0;
	new->id = msg_id;
	new->dpath = (unit_t *)calloc(10, sizeof(unit_t));
	new->hops = 10;
	new->hopc = 1;
	new->dpath[0] = src;
	new->dst_set = (unit_t *)calloc(10, sizeof(unit_t));
	new->set_len = 10;

	new->i_hop = MAX_IHOP;
	new->i_tries = MAX_ITRIES;
	new->c_copy = 1;
	new->c_tries = MAX_CTRIES;
	new->new_recv = false;

	FILE *fp1 = NULL;
	FILE *fp = fopen(RELATION_FILE, "r");
	ssize_t read;
	size_t len = 0;
	char *line = NULL;
	unit_t cnt = 0;
	while((read = getline(&line, &len, fp)) != -1) {
		unit_t id1, id2;
		sscanf(line, "%ld %ld", &id1, &id2);
		if(id1 > src)
			break;

		if(id1 == src) {
			if(fp1 == NULL)
				fp1 = fopen(NODE_ID_FILE, "r");

			if(check_node_id(fp1, id2) == false)
				continue;

			if(cnt == new->set_len) {
				new->set_len += 50;
				new->dst_set = (unit_t *)realloc(new->dst_set, new->set_len * sizeof(unit_t));
			}

			new->dst_set[cnt++] = id2;
		}
	}
	new->set_len = cnt;

	fclose(fp);
	fclose(fp1);
	free(line);
}

static void msg_copy(MSG *dst, MSG *src)
{
	dst->src = src->src;
	dst->time = src->time;
	dst->max_hops = src->max_hops;
	dst->id = src->id;
	dst->hopc = src->hopc;

	dst->i_tries = src->i_tries;
	dst->c_copy = 1;
	dst->c_tries = src->c_tries;

	if(dst->set_len < src->set_len) {
		dst->dst_set = (unit_t *)realloc(dst->dst_set, src->set_len * sizeof(unit_t));
	}
	dst->set_len = src->set_len;
	memcpy(dst->dst_set, src->dst_set, src->set_len * sizeof(unit_t));

	if(dst->hops < src->hops) {
		dst->dpath = (unit_t *)realloc(dst->dpath, src->hops * sizeof(unit_t));
	}
	dst->hops = src->hops;
	memcpy(dst->dpath, src->dpath, src->hops * sizeof(unit_t));
}

static inline void msg_drop(MSG *m, NODE *n)
{
	MSG *tail = &(n->buffer[n->buffer_p - 1]);
	msg_copy(m, tail);
	n->buffer_p--;
}

static bool check_dst(unit_t id, MSG *m)
{
	int i;
	for(i=0; i<m->set_len; i++)
		if(id == m->dst_set[i])
			return true;

	return false;
}

static void msg_path_wb(unit_t dst, MSG *m)
{
	static char path_str[WB_BUFFLEN] = {0};
	char *str;

	fprintf(fmulti_path, "dst,status,delay,hops\r\n");
	fprintf(fmulti_path, "%ld,%d,%ld,%ld\r\n", dst, 1, timer - m->time, m->hopc + 1);
	fprintf(fmulti_path, "path:\r\n");

	if(m->hopc == m->hops)
		m->dpath = (unit_t *)realloc(m->dpath, (m->hops + 1) * sizeof(unit_t));
	m->dpath[m->hopc] = dst;

	str = path_str;
	str[0] = 0;
	int_to_string(str, m->dpath, m->hopc + 1);
	fwrite(path_str, sizeof(char), strlen(path_str), fmulti_path);
}

static bool msg_deliver(MSG *m, NODE *n/*next hop*/, unit_t *nei_lst, unit_t len)
{
	int i,j;
	bool flag = false;

	for(j=0; j<len; j++) {	//check if the dst is the neighbor frist
		unit_t uid = nei_lst[j];

	if(check_dst(uid, m)) {
		DATA_LST *tmp = &MSG_LST[m->id];

		for(i=0; i<tmp->dst_len; i++) {
			if(tmp->dst[i].id == uid &&
			   tmp->dst[i].status == false) {
				tmp->copy++;
				tmp->dst[i].delay = timer - m->time;
				tmp->dst[i].status = true;
				tmp->dst[i].hops = m->hopc + 1;
				tmp->dst[i].path = (unit_t *)calloc(m->hopc + 1, sizeof(unit_t));
				memcpy(tmp->dst[i].path, m->dpath, m->hopc * sizeof(unit_t));
				tmp->dst[i].path[m->hopc] = uid;
				
				msg_status_update(tmp);
				
				fprintf(f_infect, "%ld,%ld,%ld,%ld\r\n", m->id, m->dpath[m->hopc-1], uid, timer);
				break;
			}
		}
		
		if(tmp->deliver)
			m->status = 1;

#ifndef MULTICAST
		msg_path_wb(uid, m);
#endif
		
		//test all neighbors for multicast case
		//if the next hop we select is the dst, mark it and skip the left trans to next hop
		if(n && uid == n->user_id)
			flag = true;
	}
	}
	if(flag)
		return true;

#ifdef COMMUNITY_INFECT
	if(m->c_tries == 0 && m->i_hop == 1)
		return false;
#else
	if(m->max_hops != 0 && m->hopc + 1 >= m->max_hops)
		return false;
#endif
	
	if(n == NULL)
		return false;

	for(i=0; i<n->buffer_p; i++) {
		if(m->id == n->buffer[i].id) {
			if(n->buffer[i].status == 1)
				m->status = 1;
			//duplicated msg, drop
			return false;
		}
	}

	if(n->buffer_p == n->buffer_num)
		array_needsize(false, MSG, n->buffer, n->buffer_num, n->buffer_num + 1, array_zero_init);

	MSG *dst = &(n->buffer[n->buffer_p]);
	dst->set_len = m->set_len;
	dst->dst_set = (unit_t *)calloc(m->set_len, sizeof(unit_t));

	
	dst->hops = m->hops;
	dst->dpath = (unit_t *)calloc(m->hops, sizeof(unit_t));
	msg_copy(dst, m);

	dst->i_hop = m->i_hop - 1;
	dst->new_recv = true;

	dst->status = 0;
#ifdef DYNAMIC_COPY
	dst->copy = MAX_COPY / (dst->hopc + 1);
#else
	dst->copy = MAX_COPY;
#endif
	dst->cnt = 0;

	if(dst->hopc == dst->hops)
		array_needsize(false, unit_t, dst->dpath, dst->hops, dst->hops + 1, array_zero_init);
	dst->dpath[dst->hopc] = n->user_id;

	dst->hopc++;
	n->buffer_p++;
	MSG_LST[m->id].copy++;

	fprintf(f_infect, "%ld,%ld,%ld,%ld\r\n", m->id, m->dpath[m->hopc-1], n->user_id, timer);
	return true;
}

static void deliver_scheme(MSG *m, NODE *n, unit_t *nei_lst, unit_t len)
{
#ifdef SIMPLE_INFECT
	if(m->status == 0 && msg_deliver(m, n, nei_lst, len)) {
		m->copy--;
		m->cnt++;
		if(m->copy == 0)
			m->status = 2;
	}

	return;
#endif

#ifdef COMMUNITY_INFECT
	if(m->status == -1) {
		msg_deliver(m, NULL, nei_lst, len);
	}

	if(m->status == 0) {
		if(m->i_hop) {
			if(msg_deliver(m, n, nei_lst, len)) {
				m->c_copy--;
				if(m->c_copy == 0)
					m->status = -1;
			}
		}
		else {
			if(m->i_tries) {
				m->i_hop = MAX_IHOP;
				m->i_tries--;
				if(msg_deliver(m, n, nei_lst, len)) {
					m->c_copy--;
					if(m->c_copy == 0)
						m->status = -1;
				}
			}
			else {
				if(m->c_tries == 0) {
					m->status = 3;
					return;
				}

				m->c_tries--;
				m->i_hop = MAX_IHOP;
				m->i_tries = MAX_ITRIES;
				m->c_copy = MAX_CCOPY;
				if(msg_deliver(m, n, nei_lst, len)) {
					m->c_copy--;
					if(m->c_copy == 0)
						m->status = -1;
				}
			}
		}
	}
#endif
}

static void buffer_refresh(NODE *n)
{
	int i;
	MSG *m;

	for(i=0; i<n->buffer_p; i++) {
		m = &(n->buffer[i]);
		if(m->new_recv)
			m->new_recv = false;	
	}
}

static void data_forward(NODE *src, NODE *dst, unit_t *nei_lst, unit_t len)
{
	int i;	
	MSG *m = NULL;
	
#ifdef MULTICAST
	for(i=0; i<src->buffer_p; i++) {
		m = &(src->buffer[i]);
		if(m->status == 1)
			continue;

#ifdef COMMUNITY_INFECT
		if(m->new_recv)
			continue;
#endif

#ifdef FLOODING
		msg_deliver(m, dst, nei_lst, len);
#else
		deliver_scheme(m, dst, nei_lst, len);
#endif
	}
#else
	for(i=0; i<src->buffer_p; i++) {
		m = &(src->buffer[i]);
		msg_deliver(m, dst, nei_lst, len);
	}

#endif
}

static void msg_exchange(NODE *n1, NODE *n2, unit_t *nei_lst, unit_t len)
{
	if(!(n1->buffer_p == 0 && n1->buffer_num == 2))
		data_forward(n1, n2, nei_lst, len);

	if(!(n2->buffer_p == 0 && n2->buffer_num == 2))
		data_forward(n2, n1, nei_lst, len);
}

static bool random_select(double ratio)
{
	double r = gsl_rng_uniform(Rng_r);

	if(r < ratio)
		return true;
	else
		return false;

}

static int random_neighbor(int src, int range, double ratio)
{
	int i;
	while(1) {
		for(i=0; i<range; i++) {
			if(i == src)
				continue;

			if(random_select(ratio))
				return i;
		}
	}
}

void data_trans(NODE *n, POS *p)
{
	unit_t i, j;
#if (TRANS_SCHEME == 1)
	//select part of its neighbors to send data
	for(i=0; i<p->node_p; i++) {
		for(j=0; j<p->node_p; j++) {
			if(i == j || j % NEIGHBOR_PORTION)
				continue;
			
			NODE *n1 = &nlist[p->node_id[i]];
			if(n1->buffer_p == 0 && n1->buffer_num == 2)
				continue;

			NODE *n2 = &nlist[p->node_id[j]];

			data_forward(n1, n2, p->node_id, p->node_p);
		}
	}
#elif (TRANS_SCHEME == 2)
	//select one random neighbor to send data
	double s_ratio = 1. / (double)p->node_p;
	for(i=0; i<p->node_p; i++) {
		NODE *n1 = &nlist[p->node_id[i]];
		if(n1->buffer_p == 0 && n1->buffer_num == 2)	//no data in node n1's buffer
			continue;

		int next_hop = random_neighbor(i, p->node_p, s_ratio);
		NODE *n2 = &nlist[p->node_id[next_hop]];

		data_forward(n1, n2, p->node_id, p->node_p);
	}

#ifdef COMMUNITY_INFECT
	for(i=0; i<p->node_p; i++) {
		NODE *tmp = &nlist[p->node_id[i]];
		buffer_refresh(tmp);
	}
#endif

#else
	//select all neighbors to send data
	for(i=0; i<p->node_p - 1; i++) {
		for(j=i+1; j<p->node_p; j++) {			
			NODE *n1 = &nlist[p->node_id[i]];
			NODE *n2 = &nlist[p->node_id[j]];

			msg_exchange(n1, n2, p->node_id, p->node_p);
		}
	}
#endif
}

#if 0
//TEST
int main(void)
{
	MSG test = {0};
	MSG *tmp = &test;
	msg_generate(tmp, 0, 10, 0, 1);	

	return 0;
}
#endif

