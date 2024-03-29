/*
 *   FILE: uthread_private.h
 * AUTHOR: Peter Demoreuille
 *  DESCR: uthreads private stuff.
 *   DATE: Mon Oct  1 10:56:31 2001
 *
 */

#ifndef __uthread_private_h__
#define __uthread_private_h__

#include "uthread_mtx.h"

/*
 * initialize the scheduler.
 * called from uthread_init()
 */
void uthread_sched_init(void);

/*
 * swap out the currently running thread and wait
 * until there is another runnable thread and start
 * running it
 */
void uthread_switch(utqueue_t *, int);

/*
 * Put newly created thread on runq
 */
void uthread_startonrunq(uthread_id_t, int);

#endif /* __uthread_private_h__ */
