#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"
#include "string.h"
#include "queue.h"
#include "process.h"


#define NUM_PROCESSES   128  /* Maximum number of concurrent processes */
#define SWITCH_FREQ    1000  /* Frequence (in Hz) of the switching */

typedef struct scheduler_state {
  pid      curr_pid;

  process_t processes[NUM_PROCESSES];
  queue_t  *runqueues[MAX_PRIORITY + 1];  /* Set of process ids ordered by priority */
} scheduler_state_t;


/**
 * @name scheduler_install - Initializes a new scheduler
 * @param shell_on         - If set, installs the shell as a program
 */
void scheduler_install(bool shell_on);

/**
 * @name select_new_process - Searches the runqueues for a runnable process with highest priority
 */
void select_new_process();

/**
 * @name run_program - Runs the given program
 * @param name       - The name of the program, /progs/name.elf must exist
 */
void run_program(string name);

void switch_to_process(pid pid);


#endif /* SCHEDULER_H */
