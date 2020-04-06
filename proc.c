#include <stdlib.h>
#include <strings.h>
#include "proc.h"
#include "oss.h"

//amount of processes with normal class ( in % )
#define NORMALITY 75

static unsigned int bitmap;

static int bitmap_find_unset_bit(){

	int i;
  for(i=0; i < USERS_COUNT; i++){
  	if(((bitmap & (1 << i)) >> i) == 0){	//if bit is unset

			bitmap ^= (1 << i);	//raise the bit
      return i;
    }
  }
  return -1;
}

void process_free(struct process * procs, const unsigned int i){

    bitmap ^= (1 << i); //switch bit
    bzero(&procs[i], sizeof(struct process));
}

struct process * process_new(struct process * procs, const int id){
	const int i = bitmap_find_unset_bit();
	if(i == -1){
		return NULL;
	}

  procs[i].id	= id;
  procs[i].realtime = ((rand() % 100) > NORMALITY) ? 1 : 0;
  procs[i].state = READY;
	return &procs[i];
}
