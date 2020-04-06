
#include "proc.h"

#define USERS_COUNT 20

struct memory {
	struct clock clock;
	struct process procs[USERS_COUNT];
};

struct memory * shm_attach(const int flags);
						int shm_detach(const int clear);
