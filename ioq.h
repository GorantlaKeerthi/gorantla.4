#ifndef IOQ_H
#define IOQ_H

#include "proc.h"

struct io_queue {
	int queue[USERS_COUNT];	/* value is ctrl_block->id */
	int count;
};

void ioq_init();

int ioq_enq(const int p);

int ioq_deq();
int ioq_top();

int ioq_size();

int ioq_ready(const struct clock * clock, const struct process * procs);

#endif
