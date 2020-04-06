#ifndef PBTL_H
#define PBTL_H

#include "clock.h"
#include "oss.h"

enum process_state { READY=1, IOBLK, TERMINATE};
enum process_times { CPU_TOTAL=0, SYS_TOTAL, BURST_TIME, START_TIME, BLOCKED_UNTIL, READY_FROM};

// entry in the process control table
struct process {
	int	pid;
	int id;
	int realtime;
	int state;
	struct clock	tms[6];
};

struct process * process_new(struct process * procs, const int id);
void 						 process_free(struct process * procs, const unsigned int i);

#endif
