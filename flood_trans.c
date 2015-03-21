#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "nodes.h"

#define RELATION_FILE	"./gowalla_edges.txt"
#define NODE_ID_FILE	"./node.id"

extern DATA_LST MSG_LST[MAX_MSGLST];
extern NODE *nlist;
extern time_t timer;

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
	new->id = msg_id;
	new->dpath = (unit_t *)calloc(10, sizeof(unit_t));
	new->hops = 10;
	new->hopc = 1;
	new->dpath[0] = src;
	new->dst_set = (unit_t *)calloc(10, sizeof(unit_t));
	new->set_len = 10;

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

static bool check_dst(unit_t id, MSG *m)
{
	int i;
	for(i=0; i<m->set_len; i++)
		if(id == m->dst_set[i])
			return true;

	return false;
}

static void msg_deliver(MSG *m, NODE *n)
{
	int i;

	if(check_dst(n->user_id, m)) {
		DATA_LST *tmp = &MSG_LST[m->id];

		for(i=0; i<tmp->dst_len; i++) {
			if(tmp->dst[i].id == n->user_id) {
				if(tmp->dst[i].status == false) {
					tmp->copy++;
					tmp->dst[i].delay = timer - m->time;
					tmp->dst[i].status = true;
					tmp->dst[i].hops = m->hopc + 1;
					tmp->dst[i].path = (unit_t *)calloc(m->hopc + 1, sizeof(unit_t));
					memcpy(tmp->dst[i].path, m->dpath, m->hopc * sizeof(unit_t));
					tmp->dst[i].path[m->hopc] = n->user_id;
				}
				break;
			}
		}
		
		return;
	}
	
	for(i=0; i<n->buffer_p; i++) {
		if(m->id == n->buffer[i].id)
			//duplicated msg, drop
			return;
	}

	if(n->buffer_p == n->buffer_num)
		array_needsize(false, MSG, n->buffer, n->buffer_num, n->buffer_num + 1, array_zero_init);

	MSG *dst = &(n->buffer[n->buffer_p]);
	dst->set_len = m->set_len;
	dst->dst_set = (unit_t *)calloc(m->set_len, sizeof(unit_t));

	
	dst->hops = m->hops;
	dst->dpath = (unit_t *)calloc(m->hops, sizeof(unit_t));
	msg_copy(dst, m);

	if(dst->hopc == dst->hops)
		array_needsize(false, unit_t, dst->dpath, dst->hops, dst->hops + 1, array_zero_init);
	dst->dpath[dst->hopc] = n->user_id;

	dst->hopc++;
	n->buffer_p++;
	MSG_LST[m->id].copy++;
}

static void data_forward(NODE *src, NODE *dst)
{
	int i, flag = 0;	
	unit_t _buffer_p = src->buffer_p;
	MSG *m = NULL;
	
	for(i=0; i<src->buffer_p; i++) {
		if(flag)
			i--;
		
		m = &(src->buffer[i]);
		if(MSG_LST[m->id].deliver) {
		//drop this msg
			MSG *tail = &(src->buffer[src->buffer_p - 1]);
			msg_copy(m, tail);

			src->buffer_p--;
			flag = 1;
			continue;
		}

		flag = 0;
		msg_deliver(m, dst);
		
	}
	
	if(i < _buffer_p) {
		m = &(src->buffer[i]);
		if(MSG_LST[m->id].deliver) 
			src->buffer_p--;
		else 
			msg_deliver(m, dst);
	}
}

static void msg_exchange(NODE *n1, NODE *n2)
{
	data_forward(n1, n2);
	data_forward(n2, n1);
}

void data_trans(NODE *n, POS *p)
{
	unit_t i, j;
	for(i=0; i<p->node_num-1; i++) {
		for(j=i+1; j<p->node_num; j++) {			
			NODE *n1 = &nlist[p->node_id[i]];
			NODE *n2 = &nlist[p->node_id[j]];

			msg_exchange(n1, n2);
		}
	}
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

