#ifndef MLFQ_H
#define MLFQ_H

#include "proc.h"
#include "oss.h"

// number of feedback queue levels
#define MLFQ_SIZE 4

//if we repeat the same queue, MAX_WAIT_AGE times, we run the aging algorithm
#define OLDEST_DEQUE_AGE 100
//if a process have waited in an aged queue, more than 10 quantums, it will be re-queued
#define MAX_WAIT_AGE 10*QUANTUM

struct queue {
	int queue[USERS_COUNT];	/* value is ctrl_block->id */
	int count;
	unsigned int quant;
	unsigned int deq_age;
};

void mlfq_init();
int mlfq_ready(const struct process * procs);


void mlfq_age(struct clock * c,  const struct process * procs);

int mlfq_enq(const int q, const int pi);
int mlfq_deq(const int q, const int pos);
int mlfq_top(const int q);

unsigned int mlfq_quant(const int q);

#endif
