#include <string.h>

#include "mlfq.h"

static struct queue Q[MLFQ_SIZE]; //ready process queue
static int deq_age = 0;

//Move processes that have waited for too long in a lower queue
static void aging(const int q, struct clock * clock,  const struct process * procs){
  int i;
  for(i=0; i < Q[q].count; i++){

    //calculate the age ( time process waited ) in this queue
    const int pi = Q[q].queue[i];
    const struct process * pe = &procs[pi];

    struct clock wait_time;
    sub_clock(&wait_time, clock, &pe->tms[READY_FROM]);

    if(wait_time.ns > MAX_WAIT_AGE){
      if(Q[q-1].count < USERS_COUNT){  //if we have space in queue
        const int p = mlfq_deq(q, i);
        mlfq_enq(q-1, p);
      }
    }
  }
}

void mlfq_age(struct clock * c,  const struct process * procs){
  ++deq_age;

  int i;
  for(i=1; i < MLFQ_SIZE; i++){
    const int old = deq_age - Q[i].deq_age;
    if(old > OLDEST_DEQUE_AGE){
      aging(i, c, procs);
      Q[i].deq_age = deq_age;
    }
  }

}

void mlfq_init(){
  memset(Q[0].queue, -1, sizeof(int)*USERS_COUNT);
  Q[0].count = 0;
  Q[0].quant = QUANTUM;
  Q[0].deq_age = 0;

  int i;
  for(i=1; i < MLFQ_SIZE; i++){
    memset(Q[i].queue, -1, sizeof(int)*USERS_COUNT);
    Q[i].count = 0;
    Q[i].quant = Q[i-1].quant * 2;
    Q[i].deq_age = 0;
  }
}


int mlfq_ready(const struct process * procs){

  int i;
  for(i=0; i < MLFQ_SIZE; i++){

    const int pi = Q[i].queue[0];
    if(procs[pi].state == READY){    /* if process is ready */
      return i;
    }
  }
  return -1;
}

//push pe index at end of queue
int mlfq_enq(const int q, const int pi){

  if(Q[q].count < USERS_COUNT){
    Q[q].queue[Q[q].count++] = pi;
    return Q[q].count - 1;  //return insert position
  }else{
    return -1;
  }
}

//Pop item at pos, from queue
int mlfq_deq(const int q, const int pos){

  const unsigned int pi = Q[q].queue[pos];
  Q[q].count--;

  //shift queue left
  int i;
  for(i=pos; i < Q[q].count; i++){
    Q[q].queue[i] = Q[q].queue[i+1];
  }
  Q[q].queue[i] = -1;

  return pi;
}

int mlfq_top(const int q){
  return Q[q].queue[0];
}

unsigned int mlfq_quant(const int q){
  return Q[q].quant;
}
