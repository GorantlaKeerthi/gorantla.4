#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "shm.h"
#include "proc.h"
#include "mlfq.h"
#include "ioq.h"
#include "msq.h" //message   queue

// clock step
#define CLOCK_NS 1000

// total processes to start
#define USERS_GENERATED 100

static int scheduled_count = 0; //users passed through scheduler
static int io_count = 0;  //users blocked for IO

static int started = 0; //started users
static int exited = 0;  //exited users

enum stat_times {IDLE_TIME, TURN_TIME, WAIT_TIME, SLEEP_TIME};
static struct clock stat_tms[4];

static struct memory *mem = NULL;   /* shared memory region pointer */

static int interrupted = 0;

static int exec_user(void){

  struct process *pe;

  if((pe = process_new(mem->procs, started)) == NULL){
    return 0; //no free processes
  }
  started++;

  const pid_t pid = fork();
  if(pid == -1){
    perror("fork");
    return -1;
  }else if(pid == 0){
    execl("./user", "./user", NULL);
    perror("execl");
    exit(-1);
  }else{
    pe->pid = pid;
    pe->tms[START_TIME] =
    pe->tms[READY_FROM] = mem->clock;
  }

  const int pi = pe - mem->procs; //process index
  const int rv = mlfq_enq(0, pi);
  if(rv < 0){
    fprintf(stderr, "[%i: %i] Error: Queueing process with PID %d failed\n", mem->clock.s, mem->clock.ns, pe->pid);
  }else{
    printf("[%i:%i] OSS: Generating process with PID %u in queue 0\n", mem->clock.s, mem->clock.ns, pe->id);
  }
  return rv;
}

static int new_state(struct process * pe, const int q){
  int rv = 0;
  switch(pe->state){

    case READY:
      printf("[%i:%i] OSS: Receiving that process with PID %u ran for %u nanoseconds\n",
        mem->clock.s, mem->clock.ns, pe->id, pe->tms[BURST_TIME].ns);

      add_clocks(&pe->tms[CPU_TOTAL], &pe->tms[BURST_TIME]);
      add_clocks(&mem->clock, &pe->tms[BURST_TIME]);  //advance clock with burst time
      break;

    case IOBLK:

      io_count++;

      printf("[%i:%i] OSS: Process with PID %u blocked until %u:%u\n",
          mem->clock.s, mem->clock.ns, pe->id, pe->tms[BLOCKED_UNTIL].s, pe->tms[BLOCKED_UNTIL].ns);
      /* add burst and current timer to make blocked timestamp */
  		add_clocks(&pe->tms[BLOCKED_UNTIL], &pe->tms[BURST_TIME]);
  		add_clocks(&pe->tms[BLOCKED_UNTIL], &mem->clock);
      break;

    case TERMINATE:

      exited++;
      add_clocks(&pe->tms[CPU_TOTAL], &pe->tms[BURST_TIME]);
      sub_clock(&pe->tms[SYS_TOTAL], &mem->clock, &pe->tms[START_TIME]);
      printf("[%i:%i] OSS: Process with PID %u exited\n", mem->clock.s, mem->clock.ns, pe->id);
      break;

    default:
      printf("[%i:%i] OSS: Process with PID %d has invalid state\n", mem->clock.s, mem->clock.ns, pe->pid);
      rv = -1;
      break;
  }
  return rv;
}

static void new_queue(struct process * pe, int q){

  const int pi = mlfq_deq(q, 0);
  struct clock res;

  switch(pe->state){
    case TERMINATE:

      //stat_tms[TURN_TIME] time = system time / num processes
      add_clocks(&stat_tms[TURN_TIME], &pe->tms[SYS_TOTAL]);

      /* wait time = total_system time - total cpu time */
      sub_clock(&res, &pe->tms[SYS_TOTAL], &pe->tms[CPU_TOTAL]);
      add_clocks(&stat_tms[WAIT_TIME], &res);

      printf("[%i:%i] OSS: Process with PID %u terminated, removed from queue %d\n", mem->clock.s, mem->clock.ns, pe->id, q);
      process_free(mem->procs, pi);
      break;

    case IOBLK:
      printf("[%i:%i] OSS: Putting process with PID %u into blocked queue\n", mem->clock.s, mem->clock.ns, pe->id);
      ioq_enq(pi);
      break;

    default:

      if(pe->tms[BURST_TIME].ns == mlfq_quant(q)){  //if entire quantum was used

        if(q < (MLFQ_SIZE - 1)){  //if there is next queue
          q++;  //move to next queue
        }
      }else{
        printf("OSS: not using its entire time quantum\n");
      }
      pe->tms[READY_FROM] = mem->clock;

      printf("[%i:%i] OSS: Process with PID %u moved to queue %d\n", mem->clock.s, mem->clock.ns, pe->id, q);
      mlfq_enq(q, pi);

      break;
  }
}

static int dispatching(const int q){

  const int pi = mlfq_top(q);
  struct process * pe = &mem->procs[pi];

  printf("[%i:%i] OSS: Dispatching process with PID %u from queue %i\n", mem->clock.s, mem->clock.ns, pe->id, q);

  /* send message to process */
  struct msgbuf mb;
  mb.mtype = pe->pid;
  mb.msg = mlfq_quant(q);

  if( (msq_enq(&mb) == -1) || (msq_deq(&mb) == -1) ){
    return -1;
  }

  pe->tms[BURST_TIME] = mb.burst;
  pe->state = mb.msg;
  scheduled_count++;

  new_state(pe, q);
  new_queue(pe, q);

  //calculate dispatch time
  struct clock temp;
  temp.s = 0;
  temp.ns = rand() % 100;
  printf("[%i:%i] OSS: total time this dispatching was %d nanoseconds\n", mem->clock.s, mem->clock.ns, temp.ns);
  add_clocks(&mem->clock, &temp);

  return 0;
}

static int unblocking(){

  const int pi = ioq_ready(&mem->clock, mem->procs);
  if(pi == -1){
    return -1;
  }
  struct process * pe = &mem->procs[pi];

  // update total sleep time
  add_clocks(&stat_tms[SLEEP_TIME], &pe->tms[BURST_TIME]);  // burst holds how long user was BLOCKED

  // udpate process state and timers
  pe->state = READY;
  pe->tms[BLOCKED_UNTIL].s = pe->tms[BLOCKED_UNTIL].ns = 0;
  pe->tms[BURST_TIME].s = pe->tms[BURST_TIME].ns = 0;
  pe->tms[READY_FROM] = mem->clock;


  const int rv = mlfq_enq(0, pi);
  if(rv < 0){
    fprintf(stderr, "[%i: %i] Error: Queueing process with PID %d failed\n", mem->clock.s, mem->clock.ns, pe->pid);
  }else{
    printf("[%i:%i] OSS: Unblocked process with PID %d to queue 0\n", mem->clock.s, mem->clock.ns, pe->id);
  }

  return rv;
}

static int forktime(struct clock *forktimer){

  struct clock inc;

  //advance time
  inc.s = 1;
  inc.ns = rand() % CLOCK_NS;
  add_clocks(&mem->clock, &inc);

  if(started < USERS_GENERATED){  //if we can fork more

    // if its time to fork
    if(cmp_clocks(&mem->clock, forktimer)){

      //next fork time
      forktimer->s = mem->clock.s + 1;
      forktimer->ns = 0;

      return 1;
    }
  }
  return 0; //not time to fokk
}

//Send mesage to users to quit
static void final_msg(const int msg){
  int i;
  struct msgbuf mb;
  mb.msg = msg;

  for(i=0; i < USERS_COUNT; i++){
    if(mem->procs[i].pid > 0){
      mb.mtype = mem->procs[i].pid;
      if(msq_enq(&mb) == -1){
        perror("msgsnd");
        break;
      }
    }
  }
}

static void signal_handler(const int sig){
  printf("[%i:%i] OSS: Signaled with %d\n", mem->clock.s, mem->clock.ns, sig);
  interrupted = 1;
}

static void print_results(){

  printf("Quantum: %d\n", QUANTUM );
  printf("Runtime: %u:%u\n", mem->clock.s, mem->clock.ns);

  printf("Dispatched : %i\n", scheduled_count);
  printf("IO count: %i\n", started);
  printf("Started: %i\n", started);
  printf("Exited: %i\n", exited);

  div_clock(&stat_tms[TURN_TIME], started);
  div_clock(&stat_tms[WAIT_TIME], started);
  div_clock(&stat_tms[SLEEP_TIME], started);

  printf("Turnaround ave. : %u:%u\n", stat_tms[TURN_TIME].s, stat_tms[TURN_TIME].ns);
  printf("Wait ave. : %u:%u\n", stat_tms[WAIT_TIME].s, stat_tms[WAIT_TIME].ns);
  printf("Sleep ave.: %u:%u\n", stat_tms[SLEEP_TIME].s, stat_tms[SLEEP_TIME].ns);
  printf("Idle ave.: %u:%u\n", stat_tms[IDLE_TIME].s, stat_tms[IDLE_TIME].ns);
}

int main(void){

  signal(SIGINT,  signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGALRM, signal_handler);
  signal(SIGCHLD, SIG_IGN);

  mem = shm_attach(0600 | IPC_CREAT);
  if(mem == NULL){
    return -1;
  }
  bzero(mem,   sizeof(struct memory));

  if(msq_attach(0600 | IPC_CREAT) == -1){
    return -1;
  }

  srand(getpid());

  stat_tms[IDLE_TIME].s = stat_tms[IDLE_TIME].ns = 0;
  stat_tms[TURN_TIME].s = stat_tms[TURN_TIME].ns = 0;
  stat_tms[WAIT_TIME].s = stat_tms[WAIT_TIME].ns = 0;
  stat_tms[SLEEP_TIME].s = stat_tms[SLEEP_TIME].ns = 0;

  ioq_init();
  mlfq_init();

  alarm(3);

  stdout = freopen("output.txt", "w", stdout);
  if(stdout == NULL){
		perror("freopen");
		return -1;
	}

  int idle_mode = 0;
  struct clock idle_start;
  struct clock forktimer = {0,0};

  while((exited < USERS_GENERATED) &&
        (!interrupted)){

    if(forktime(&forktimer) && (exec_user() < 0)){
      fprintf(stderr, "exec_user failed\n");
      break;
    }

    unblocking();

    const int rq = mlfq_ready(mem->procs);
    if(rq >= 0){  /* if we find a ready user*/

      if(idle_mode){  /* we have a process and idle flag is set */
        //how much time we were idle
        struct clock temp;
        sub_clock(&temp, &mem->clock, &idle_start);
        add_clocks(&stat_tms[IDLE_TIME], &temp);

        idle_start.s  = 0;
        idle_start.ns = 0;
        idle_mode = 0;
      }

      if(dispatching(rq) < 0){
        fprintf(stderr, "dispatching failed\n");
        break;
      }

      mlfq_age(&mem->clock, mem->procs);

    }else{    /* nobody to dispatching now */

      if(idle_mode == 0){
        printf("[%i:%i] OSS: No ready process for dispatching\n", mem->clock.s, mem->clock.ns);
        idle_start = mem->clock;
        idle_mode = 1;
      }

      if(ioq_size() > 0){
        struct process * pe = &mem->procs[ioq_top()];
        printf("[%i:%i] OSS: No ready process. Time jump to %i:%i\n",
          mem->clock.s, mem->clock.ns, pe->tms[BLOCKED_UNTIL].s, pe->tms[BLOCKED_UNTIL].ns);
        mem->clock = pe->tms[BLOCKED_UNTIL];
      }
    }
  }

  print_results();

  final_msg(-1);
  shm_detach(1);
  msq_detach(1);
  return 0;
}
