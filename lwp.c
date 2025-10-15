// Partner Names: 
// Sreerenjini Surendran (snamboot)
// Juan E Cisneros (jcisne23)
// Katie Slobodsky (kslobods)

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include "lwp.h"

/* ====== Scheduler state ====== */
thread head    = NULL;  /* RR ready queue head
                         * (circular, uses sched_one/sched_two)
                         */

thread current = NULL;  /* currently running LWP */
int    qlen    = 0;     /* number of runnable LWPs */

/* ====== Book-keeping ====== */
static tid_t  next_tid = 1;
static thread all_list = NULL;                  /* singly linked via lib_one */
static thread morgue_h = NULL, morgue_t = NULL; /* terminated threads FIFO   */
static thread wait_h   = NULL, wait_t   = NULL; /* waiters (blocked in wait) */

static scheduler CurrSched;                     /* active scheduler vtable   */

#define QNEXT(t) ((t)->sched_one)
#define QPREV(t) ((t)->sched_two)

/* ====== tiny list helpers ====== */
static void push_all(thread t){
  if(!t) return;
  t->lib_one = all_list;
  all_list = t;
}

static void wait_push(thread t){
  t->exited = NULL;
  if(!wait_t){ wait_h = wait_t = t; }
  else { wait_t->exited = t; wait_t = t; }
}

static thread wait_pop(void){
  if(!wait_h) return NULL;
  thread t = wait_h;
  wait_h = t->exited;
  if(!wait_h) wait_t = NULL;
  t->exited = NULL;
  return t;
}

static void morgue_push(thread t){
  t->exited = NULL;
  if(!morgue_t){ morgue_h = morgue_t = t; }
  else { morgue_t->exited = t; morgue_t = t; }
}

static thread morgue_pop(void){
  if(!morgue_h) return NULL;
  thread t = morgue_h;
  morgue_h = t->exited;
  if(!morgue_h) morgue_t = NULL;
  t->exited = NULL;
  return t;
}

/* ====== Stack helpers ====== */
static size_t page_align(size_t n){
  long p = sysconf(_SC_PAGESIZE);
  return (n + p - 1)/p * p;
}

static void *alloc_stack(size_t *out_sz){
  struct rlimit rl; size_t want;
  if (getrlimit(RLIMIT_STACK, &rl) == 0 &&
    rl.rlim_cur > 0 &&
    rl.rlim_cur != RLIM_INFINITY)
    want = (size_t)rl.rlim_cur;
  else
    want = 8ul<<20; /* 8 MB default */

  want = page_align(want);
  void *base = mmap(NULL, want, PROT_READ|PROT_WRITE,
                    MAP_PRIVATE|MAP_ANONYMOUS
#ifdef MAP_STACK
                    | MAP_STACK
#endif
                    , -1, 0);
  if(base == MAP_FAILED) return NULL;
  *out_sz = want;
  return base;
}

static void free_stack(void *base, size_t sz){
  if(base && sz) munmap(base, sz);
}

/* ====== Round-Robin scheduler ====== */
void rr_init(void){ head=NULL; current=NULL; qlen=0; }
void rr_shutdown(void){ head=NULL; current=NULL; qlen=0; }

void rr_admit(thread t){
  if(!t) return;
  if(!head){
    head = t;
    QNEXT(t) = QPREV(t) = t;
    qlen = 1;
    return;
  }
  thread tail = QPREV(head);
  QNEXT(t) = head;  QPREV(t) = tail;
  QNEXT(tail) = t;  QPREV(head) = t;
  qlen++;
}

void rr_remove(thread t){
  if(!t || !head) return;
  if(QNEXT(t)==t && QPREV(t)==t){
    head = NULL; qlen = 0; return;
  }
  QNEXT(QPREV(t)) = QNEXT(t);
  QPREV(QNEXT(t)) = QPREV(t);
  if(head == t) head = QNEXT(t);
  qlen--;
  QNEXT(t) = QPREV(t) = NULL;
}

thread rr_next(void){
  if(!head) return NULL;
  thread pick = head;
  head = QNEXT(head); /* rotate */
  return pick;
}

int rr_qlen(void){ return qlen; }

/* publish a scheduler table */
struct scheduler rr_publish = {
  rr_init,
  rr_shutdown,
  rr_admit,
  rr_remove,
  rr_next,
  rr_qlen
};
scheduler RoundRobin = &rr_publish;

/* ====== Context switch + launch shim ====== */
extern void swap_rfiles(rfile *old, rfile *new);

static void lwp_wrap(lwpfun fun, void *arg){
  int rv = fun(arg);
  lwp_exit(rv); /* never returns */
}

/* first time we "return" here after swap_rfiles loads the new state */
static void lwp_stub(void){
  lwpfun f = (lwpfun)current->state.rdi;
  void  *a = (void*) current->state.rsi;
  lwp_wrap(f,a);
}

static void jump_to(thread next){
  thread prev = current;
  current = next;
  if(prev == next) return;
  if(prev) swap_rfiles(&prev->state, &next->state);
  else     swap_rfiles(NULL, &next->state);
}

/* ====== API ====== */
void lwp_set_scheduler(scheduler s){
  if(!s) s = RoundRobin;

  /* first-time init */
  if(CurrSched == NULL){
    CurrSched = s;
    if(CurrSched->init) CurrSched->init();
    return;
  }

  if(CurrSched == s) return;

  scheduler old = CurrSched;
  CurrSched = s;
  if(CurrSched->init) CurrSched->init();

  /* migrate all runnable threads old -> new in old's queue order */
  int n = old->qlen ? old->qlen() : 0;
  for(int i=0; i<n; i++){
    thread t = old->next();
    if(!t) break;
    old->remove(t);
    CurrSched->admit(t);
  }

  if(old->shutdown) old->shutdown();
}

scheduler lwp_get_scheduler(void){
  return CurrSched ? CurrSched : RoundRobin;
}

thread tid2thread(tid_t tid){
  if(tid <= NO_THREAD) return NULL;
  for(thread t = all_list; t; t = t->lib_one)
    if(t->tid == tid) return t;
  return NULL;
}

tid_t lwp_create(lwpfun func, void *argument){
  thread t = calloc(1, sizeof(*t));
  if(!t) return NO_THREAD;

  t->tid = next_tid++;
  t->status = MKTERMSTAT(LWP_LIVE, 0);

  /* stack + initial context */
  size_t sz = 0;
  t->stack = (unsigned long*)alloc_stack(&sz);
  if(!t->stack){ free(t); return NO_THREAD; }
  t->stacksize = sz;

  memset(&t->state, 0, sizeof t->state);
  t->state.fxsave = FPU_INIT;

  /* Correct initial frame for swap_rfiles' `leave; ret`
     Layout on the new stack:
       [frame + 0] : fake saved %rbp
       [frame + 8] : return address (lwp_stub)
     `leave` sets rsp=rbp and pops fake rbp, then `ret` jumps to lwp_stub. */
  uintptr_t top   = (uintptr_t)t->stack + t->stacksize;
  top &= ~((uintptr_t)0xF);           /* 16-byte align */
  uintptr_t frame = top - 16;         /* space for rbp + ret */
  *(void**)(frame + 0) = (void*)0;          /* fake saved rbp */
  *(void**)(frame + 8) = (void*)lwp_stub;   /* return address */
  t->state.rbp = frame;
  t->state.rsp = frame;               /* not used by `leave`, harmless */

  /* pass function & argument via registers for stub */
  t->state.rdi = (unsigned long)func;
  t->state.rsi = (unsigned long)argument;

  push_all(t);
  if(!CurrSched) lwp_set_scheduler(NULL);
  CurrSched->admit(t);
  return t->tid;
}

void lwp_start(void){
  if(current) return; /* already in LWP world */
  thread me = calloc(1, sizeof(*me));
  if(!me) return;
  me->tid = next_tid++;
  me->status = MKTERMSTAT(LWP_LIVE, 0);
  /* NOTE: me->stack==NULL so we never unmap the real process stack */
  push_all(me);
  if(!CurrSched) lwp_set_scheduler(NULL);
  CurrSched->admit(me);
  current = me;
  lwp_yield();
}

void lwp_yield(void){
  if(!CurrSched) lwp_set_scheduler(NULL);
  thread next = CurrSched->next();
  if(!next) _exit(LWPTERMSTAT(current ? current->status : 0));
  jump_to(next);
}

void lwp_exit(int exitval){
  if(!current) _exit(exitval & 0xFF);
  current->status = MKTERMSTAT(LWP_TERM, exitval & 0xFF);

  CurrSched->remove(current);

  /* hand off corpse to a waiter if any; otherwise park in morgue */
  thread waiter = wait_pop();
  if(waiter){
    waiter->exited = current;
    CurrSched->admit(waiter);
  }else{
    morgue_push(current);
  }

  thread next = CurrSched->next();
  if(!next) _exit(LWPTERMSTAT(current->status));
  jump_to(next);
}

tid_t lwp_wait(int *status){
  thread corpse = morgue_pop();
  if(!corpse){
    /* no corpse yet: only block if someone else can run */
    if(!CurrSched || CurrSched->qlen() <= 1) return NO_THREAD;
    thread self = current;
    CurrSched->remove(self);
    wait_push(self);
    lwp_yield();                /* resumes when paired with a corpse */
    corpse = self->exited;
  }

  if(status) *status = (int)corpse->status;
  tid_t id = corpse->tid;
  if(corpse->stack){
  free_stack(corpse->stack, corpse->stacksize);  /* never free main */
}
  free(corpse);
  return id;
}

tid_t lwp_gettid(void){
  return current ? current->tid : NO_THREAD;
}
