#include <stdio.h>
#include <stdlib.h>
#include "lwp.h"
// Making custom LWPs with smiles and tears

// Making a Round Robin Scheduler
thread head = NULL;
thread current = NULL;
int qlen = 0;

// initialize the scheduler
void rr_init(void) {
    head = NULL;
    current = NULL;
    qlen = 0;
}

// cleans up scheduler
void rr_shutdown(void) {
    head = NULL;
    current = NULL;
    qlen = 0;
}

// adds a new thread
void rr_admit(thread new);

// removes a thread
void rr_remove(thread victim);

// returns to the next thread, returns NULL if there isn't one
thread rr_next(void);

// returns number of runnable threads
int rr_qlen(void) {
    return qlen;
}

struct scheduler rr_publish = {NULL, NULL, rr_admit, rr_remove, rr_next, rr_qlen};
scheduler RoundRobin = &rr_publish;

// creates new LWP and adds it to current scheduler
tid_t lwp_create(lwpfun fucnction, void *argument);

// starts the LWP system
void lwp_start(void);

// yields control to another LWP as indicated by scheduler
void lwp_yield(void);

// terminates current LWP and yields to whatever thread the scheduler chooses
void lwp_exit(int exitval);

// waits for thread to be terminated
tid_t lwp_wait(int *status);

// gets tid of the calling thread
tid_t lwp_gettid(void);

// returns thread with corresponding tid
thread tid2thread(tid_t tid);

// install a new scheduler
void lwp_set_scheduler(scheduler sched);

// find out what the current scheduler is
scheduler lwp_get_scheduler(void);

int main () {
    printf("Do Bronx");
    return 0;
}
