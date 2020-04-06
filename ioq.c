#include <string.h>
#include "ioq.h"

static struct io_queue Q;          // blocked queue

void ioq_init(){
  memset(Q.queue, -1, sizeof(int)*USERS_COUNT);
  Q.count = 0;
}

int ioq_enq(const int p){
  if(Q.count < USERS_COUNT){
    Q.queue[Q.count++] = p;
    return Q.count - 1;
  }else{
    return -1;
  }
}

int ioq_deq(const int pos){
  const unsigned int pi = Q.queue[pos];
  Q.count--;

  //shift queue left
  int i;
  for(i=pos; i < Q.count; i++){
    Q.queue[i] = Q.queue[i+1];
  }
  Q.queue[i] = -1;

  return pi;
}

// find user we can unblocked
int ioq_ready(const struct clock * clock, const struct process * procs){
  int i;
  for(i=0; i < Q.count; i++){
    const int pi = Q.queue[i];
    if(cmp_clocks(clock, &procs[pi].tms[BLOCKED_UNTIL])){	//if our event time is reached
      return ioq_deq(i);
    }
  }
  return -1;
}

int ioq_top(){
  return Q.queue[0];
}

int ioq_size(){
  return Q.count;
}
