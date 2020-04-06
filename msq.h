#ifndef MSQ_H
#define MSQ_H

//#include "clock.h"

struct msgbuf {
	long mtype;
	int msg;
  struct clock burst;
};

int msq_attach(const int flags);
void msq_detach(const int clear);

int msq_enq(const struct msgbuf *mb);
int msq_deq(struct msgbuf *mb);

#endif
