/*
 *   FILE: uthread_sched.c
 * AUTHOR: Peter Demoreuille
 *  DESCR: scheduling wack for uthreads
 *   DATE: Mon Oct  1 00:19:51 2001
 *
 * Modified considerably by Tom Doeppner in support of two-level threading
 *   DATE: Sun Jan 10, 2016
 * Futher modified in January 2020 and Jan 2023
 */

#include "uthread_sched.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/time.h>

#include "uthread.h"
#include "uthread_bool.h"
#include "uthread_ctx.h"
#include "uthread_private.h"
#include "uthread_queue.h"

void uthread_runq_enqueue(uthread_t *thr);
static void uthread_runq_requeue(uthread_t *thr, int oldpri);

/* ---------- globals -- */

static utqueue_t runq_table[UTH_MAXPRIO + 1]; /* priority runqueues */

int masked = 1;

/* ----------- public code -- */

/*
 * uthread_yield
 *
 * Causes the currently running thread to yield use of the processor to
 * another thread. When this function returns, the thread should be executing again. 
 * A bit more clearly, when this function is called, the current thread 
 * stops executing for some period of time (allowing another thread to execute). 
 * Then, when the time is right (ie when a call to uthread_switch() results in 
 * this thread being swapped in), the function returns.
 */
void uthread_yield() {
    uthread_nopreempt_on(); 
    assert(masked);
    assert(ut_curthr->ut_link.l_next == NULL);

    NOT_YET_IMPLEMENTED("UTHREADS: uthread_yield");
    
    assert(masked);
    uthread_nopreempt_off(); 
}

/*
 * uthread_wake 
 *
 * If the thread is in UT_WAIT state, wake it up and put it on the runq. If it's not waiting,
 * do nothing.
 */
void uthread_wake(uthread_t *uthr) {
    assert(uthr->ut_state != UT_NO_STATE);
    NOT_YET_IMPLEMENTED("UTHREADS: uthread_wake");
}

/*
 * uthread_setprio
 *
 * Changes the priority of the indicated thread. Note that if the thread
 * is in the UT_RUNNABLE state (it's runnable but not on cpu) you should
 * change the list it's waiting on so the effect of this call is
 * immediate. Also note that if the newly set priority is greater
 * than the priority of the current thread, the current thread should yield.
 * uthread_runq_requeue may be useful here.
 * 
 * Returns 0 on success or error if the thread is in an invalid state, doesn't exist, 
 * or is being set to an invalid priority. For reference, please see the 
 * pthread_setschedprio() man page.
 */
int uthread_setprio(uthread_id_t id, int prio) {
    if ((prio > UTH_MAXPRIO) || (prio < 0))
        return EINVAL;
    if ((id < 0) || (id >= UTH_MAX_UTHREADS))
        return ESRCH;
    uthread_t *thr = &uthreads[id];
    
    NOT_YET_IMPLEMENTED("UTHREADS: uthread_setprio");
    
    return 0;
}

/* ----------- (mostly) private code -- */

/*
 * Called by uthread_create to put the newly created thread on the run queue
 */
void uthread_startonrunq(uthread_id_t id, int prio) {
    uthread_t *thr = &uthreads[id];
    uthread_nopreempt_on();
    thr->ut_prio = prio;
    if (thr->ut_state == UT_TRANSITION) {
        // newly created thread
        thr->ut_state = UT_RUNNABLE;
        uthread_runq_enqueue(thr);
    } else {
        PANIC("new thread not in UT_TRANSITION state");
    }
    uthread_nopreempt_off();
}

static int runq_size; /* number of threads on the runq (Used only for debugging) */

/*
 * uthread_switch()
 *
 * This is where all the magic begins. Save the uthread's context (using
 * getcontext). This is where the uthread will resume, so use a volatile local
 * variable to distinguish between first and second returns from getcontext.
 *
 * Find the highest-priority runnable thread and then switch to it, using setcontext.
 *
 * Parameters are:
 *     q: if non-null, a queue that the calling thread should be put on (should
 *        not be runq)
 *     saveonrunq: whether the calling thread should be put on the runq
 * 
 * Only one of <q> and <saveonrq> can be non-NULL.
 */
void uthread_switch(utqueue_t *q, int saveonrq) {
    // thread preeemption should be off
    assert(masked);

    NOT_YET_IMPLEMENTED("UTHREADS: uthread_switch");

    PANIC("No runnable threads");
}

static void uthread_start_timer(void);

/*
 * uthread_sched_init
 *
 * Setup the scheduler. This is called once from uthread_start().
 */
void uthread_sched_init(void) {
    for (int i = 0; i <= UTH_MAXPRIO; i++) {
        utqueue_init(&runq_table[i]);
    }
    uthread_start_timer();
}

static void clock_interrupt(int);

static void uthread_start_timer() {
    // start the time-slice timer.
    struct timeval interval = {0, 1};  // every 10 microseconds
    struct itimerval timerval;
    timerval.it_value = interval;
    timerval.it_interval = interval;
    signal(SIGVTALRM, clock_interrupt);
    masked = 1;
    setitimer(ITIMER_VIRTUAL, &timerval, 0);     // off we go!
}

/*
 * clock_interrupt
 *
 * At each clock interrupt (SIGVTALRM), call uthread_yield if preemption is not masked.
 * 
 * Make sure to unmask SIGVTALRM before calling uthread_yield.
 */
static void clock_interrupt(int sig) {
    // handler for SIGVTALRM
    NOT_YET_IMPLEMENTED("UTHREADS: clock_interrupt");
}

void uthread_nopreempt_on() {
    // mask clock interrupts for current thread; calls may be nested
    assert(!masked || ut_curthr->ut_no_preempt_count);
    masked = 1;
    ut_curthr->ut_no_preempt_count++;
    assert(ut_curthr->ut_no_preempt_count > 0);
}

void uthread_nopreempt_off() {
    // unmask clock interrupts for current thread; since calls may be nested,
    // must keep track of whether this is the "last" call, which should turn on clock
    // interrupts
    assert(ut_curthr != NULL);
    assert(masked);
    assert(ut_curthr->ut_no_preempt_count > 0);
    if (--ut_curthr->ut_no_preempt_count == 0) {
        masked = 0;
    }
}

void uthread_runq_enqueue(uthread_t *thr) {
    // assumes preemption is off
    utqueue_t *q = &runq_table[thr->ut_prio];
    utqueue_enqueue(q, thr);
    runq_size++;
}

static void uthread_runq_requeue(uthread_t *thr, int oldprio) {
    // thread's priority has been changed, pull it from old priority q and put
    // it in new one; assumes preemption is off
    utqueue_t *q = &runq_table[oldprio];
    utqueue_remove(q, thr);
    utqueue_enqueue(&runq_table[thr->ut_prio], thr);
}
