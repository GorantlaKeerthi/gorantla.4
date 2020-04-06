#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>

#include "shm.h"
#include "msq.h"

//10 % change to terminate
#define TERM_PERCENT 10

//99% quantum to use, when preempted
#define PREEMPTED_QUANT 99

// IO limits r,s
#define IO_QUANT_S 5
#define IO_QUANT_NS 1000

int main(const int argc, char * const argv[]){

	struct memory *mem = shm_attach(0);
	if(mem == NULL){
		return -1;
	}

	if(msq_attach(0) < 0){
		return -1;
	}

	srand(getpid());

	int stop = 0;
	struct msgbuf mb;
  while(!stop){

		if(msq_deq(&mb) == -1){
			break;
		}

		const int quantum = mb.msg;

		const int term = rand() % 100;
		const int opt = (term < TERM_PERCENT) ? 3 : rand() % 3;

		switch(opt){
			case 0:	//use entire quantum
				mb.msg = READY;
				mb.burst.s = 0;
				mb.burst.ns = quantum;
				break;

			case 1:	//block on io
				mb.msg = IOBLK;
				mb.burst.s = rand() % IO_QUANT_S;
				mb.burst.ns = rand() % IO_QUANT_NS;
				break;

			case 2:	//pre-empted
				mb.msg = READY;
				mb.burst.s = 0;
				mb.burst.ns = (unsigned int)((float) quantum / (100.0f / (1.0f + (rand() % PREEMPTED_QUANT))));
				break;

			case 3:
			default:
				mb.msg = TERMINATE;
				mb.burst.s = 0;
				mb.burst.ns = (unsigned int)((float) quantum / (100.0f / (1.0f + (rand() % PREEMPTED_QUANT))));

				stop = 1;
				break;
		}

		mb.mtype = getppid();
		if(msq_enq(&mb) == -1){
			break;
		}
  }


	shm_detach(0);
	msq_detach(0);

  return 0;
}
