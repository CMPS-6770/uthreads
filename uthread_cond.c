/*
 *   FILE: uthread_cond.c
 * AUTHOR: Peter Demoreuille
 *  DESCR: uthreads condition variables
 *   DATE: Mon Oct  1 01:59:37 2001
 *
 *
 * Modified to handle time slicing by Tom Doeppner
 *   DATE: Sun Jan 10, 2016 and Jan 2023
 */

#include "uthread_cond.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "uthread.h"
#include "uthread_mtx.h"
#include "uthread_private.h"
#include "uthread_queue.h"
#include "uthread_sched.h"

/*
 * uthread_cond_init
 *
 * Initializes the given condition variable. 
 */
void uthread_cond_init(uthread_cond_t *cond)
{
    utqueue_init(&cond->uc_waiters);
}

/*
 * uthread_cond_wait
 *
 * Should behave just like a stripped-down version of pthread_cond_wait.
 * Block on the given condition variable with the mutex unlocked. The
 * caller should lock the mutex and it should be locked again after the
 * broadcast or signal.
 * 
 * Mask preemption to ensure atomicity.
 */
void uthread_cond_wait(uthread_cond_t *cond, uthread_mtx_t *mtx)
{
    NOT_YET_IMPLEMENTED("UTHREADS: uthread_cond_wait");
}

/*
 * uthread_cond_broadcast
 *
 * Wake up all the threads waiting on this condition variable.
 * Note there may be no threads waiting.
 * 
 * Mask preemption to protect wait queue.
 */
void uthread_cond_broadcast(uthread_cond_t *cond)
{
    NOT_YET_IMPLEMENTED("UTHREADS: uthread_cond_broadcast");
}

/*
 * uthread_cond_signal
 *
 * Wakeup just one thread waiting on the condition variable.
 * Note there may be no threads waiting.
 * 
 * Mask preemption to protect wait queue.
 */
void uthread_cond_signal(uthread_cond_t *cond)
{
    NOT_YET_IMPLEMENTED("UTHREADS: uthread_cond_signal");
}
