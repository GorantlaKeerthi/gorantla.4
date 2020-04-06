#include <stdio.h>
#include <sys/msg.h>
#include <unistd.h>

#include "clock.h"
#include "msq.h"

static int msgid = -1;

#define MSG_SIZE (sizeof(int) + sizeof(struct clock))

int msq_attach(const int flags){
	msgid = msgget(ftok("shm.c", 2), flags);
	if(msgid == -1){
		perror("msgget");
		return -1;
	}
	return 0;
}

void msq_detach(const int clear){
	if(clear){
		msgctl(msgid, IPC_RMID, NULL);
	}
}

int msq_enq(const struct msgbuf *mb){

  if(msgsnd(msgid, (void*)mb, MSG_SIZE, 0) == -1){
    perror("msgsnd");
    return -1;
  }

  return 0;
}

int msq_deq(struct msgbuf *mb){

  if(msgrcv(msgid, (void*)mb, MSG_SIZE, getpid(), 0) == -1){
    perror("msgrcv");
    return -1;
  }

  return 0;
}
