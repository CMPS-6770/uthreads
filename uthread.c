/*
 *   FILE: uthread.c
 * AUTHOR: peter demoreuille
 *  DESCR: userland threads
 *   DATE: Sun Sep 30 23:45:00 EDT 2001
 *
 * Modified considerably by Tom Doeppner
 *   DATE: Sun Jan 10, 2016 and Jan 2023
 */
#include "uthread.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <unistd.h>

#include "uthread_bool.h"
#include "uthread_cond.h"
#include "uthread_mtx.h"
#include "uthread_private.h"
#include "uthread_queue.h"
#include "uthread_sched.h"

/* ---------- globals -- */

uthread_t *ut_curthr = 0;             /* current running thread */
uthread_t uthreads[UTH_MAX_UTHREADS]; /* threads on the system */
sigset_t VTALRMmask;

static utqueue_t reap_queue;       /* dead threads */
static uthread_id_t reaper_thr_id; /* reference to reaper thread */
static uthread_mtx_t reap_mtx;

/* ---------- prototypes -- */

static void create_first_thr(uthread_func_t firstFunc, long argc, char *argv[]);

static uthread_id_t uthread_alloc(void);
static void uthread_destroy(uthread_t *thread);

static char *alloc_stack(void);
static void free_stack(char *stack);

static void reaper_init(void);
static void *reaper(long a0, char *a1[]);
static void make_reapable(uthread_t *uth);

/* ---------- public code -- */

/*
 * uthread_start
 *
 * This initializes the global array of uthreads, then becomes the first uthread and invokes its
 * first function. It does not return -- when all uthreads are done, the reaper
 * calls exit.
 */
void uthread_start(uthread_func_t firstFunc, long argc, char *argv[])
{
    // initialize VTALRMmask for use later
    sigemptyset(&VTALRMmask);
    sigaddset(&VTALRMmask, SIGVTALRM);

    NOT_YET_IMPLEMENTED("UTHREADS: uthread_start");

    /* XXX: don't touch anything in this function below here */

    /* these should go last, and in this order */
    uthread_sched_init(); // masks clock interrupts
    reaper_init();
    create_first_thr(firstFunc, argc, argv); /* creates first user thread and
                                                reaper thread */
    uthread_switch(0, 0);
    PANIC("Returned to initial context");
}

/*
 * uthread_create
 *
 * Create a uthread to execute the specified function <func> with arguments
 * <arg1> and <arg2> and initial priority <prio>. To do this, you should -- 
 * 
 * 1. Find a valid (unused) id for the thread using uthread_alloc() (failing this,
 * return an error). 
 * 
 * 2. Create a stack for the thread using alloc_stack() (failing this, return an error).
 * 
 * 3. Set up the uthread_t struct corresponding to the newly-found id. 
 *
 * 4. Create a context for the thread to execute on using uthread_makecontext(). 
 * 
 * 5. Make the thread runnable by calling uthread_startonrunq, and return the 
 *    aforementioned thread id in <uidp>. 
 * 
 * Return 0 on success, or an error code on failure. Refer to the man page of 
 * pthread_create for the exact error codes.
 */
int uthread_create(uthread_id_t *uidp, uthread_func_t func, long arg1,
                   void *arg2, int prio)
{
    NOT_YET_IMPLEMENTED("UTHREADS: uthread_create");
    return 0;
}

/*
 * uthread_exit
 *
 * Terminate the current thread. Should set all the related flags and
 * such in uthread_t. The status value will be returned to the user through 
 * uthread_join. 
 *
 * If this is not a detached thread, and there is someone
 * waiting to join with it, you should wake up that thread.
 *
 * If the thread is detached, it should be put onto the reaper's dead
 * thread queue and wakeup the reaper thread by calling make_reapable().
 */
void uthread_exit(void *status)
{
    NOT_YET_IMPLEMENTED("UTHREADS: uthread_exit");
    PANIC("returned to a dead thread");
}

/*
 * uthread_join
 *
 * Wait for the given thread to finish executing. If the thread has not
 * finished executing, the calling thread needs to block until this event
 * happens. If the thread has a ut_exit value specified, return it by using
 * <return_value>. 
 *
 * Error conditions include (but are not limited to):
 *   o the thread described by <uid> does not exist
 *   o two threads attempting to join the same thread, etc..
 * Return an appropriate error code (found in manpage for pthread_join) in
 * these situations (and more).
 * 
 * Note that you do not need to handle the EDEADLK error case.
 *
 * Note that if a thread finishes executing and is never uthread_join()'d
 * (or uthread_detach()'d) it remains in the state UT_ZOMBIE and is never
 * cleaned up.
 *
 * When you have successfully joined with the thread, wake the reaper so 
 * it can cleanup the thread by calling make_reapable. 
 */
int uthread_join(uthread_id_t uid, void **return_value)
{
    NOT_YET_IMPLEMENTED("UTHREADS: uthread_join");
    return 0;
}

/*
 * uthread_detach
 *
 * Detach the given thread. Thus, when this thread's function has finished
 * executing, no other thread need (or should) call uthread_join() to perform
 * the necessary cleanup.
 *
 * There is also the special case if the thread has already exited and then
 * is detached (i.e. was already in the state UT_ZOMBIE when uthread_deatch()
 * is called). In this case it is necessary to call make_reapable on the
 * appropriate thread.
 *
 * There are also some errors to check for, see the man page for
 * pthread_detach (basically just invalid threads, etc).
 */
int uthread_detach(uthread_id_t uid)
{
    NOT_YET_IMPLEMENTED("UTHREADS: uthread_detach");
    return 0;
}

/*
 * uthread_self
 *
 * Returns the id of the currently running thread.
 */
uthread_id_t uthread_self(void)
{
    assert(ut_curthr != NULL);
    return ut_curthr->ut_id;
}

/* ------------- private code -- */

/*
 * uthread_alloc
 *
 * Find a free uthread_t, returns the id.
 * Mask preemption to ensure atomicity.
 */
static uthread_id_t uthread_alloc(void)
{
    NOT_YET_IMPLEMENTED("UTHREADS: uthread_alloc");
    return 0; 
}

/*
 * uthread_destroy
 *
 * Cleans up resources associated with a thread (since it's now finished
 * executing). This is called implicitly whenever a detached thread finishes
 * executing or whenever non-detached thread is uthread_join()'d.
 */
static void uthread_destroy(uthread_t *uth)
{
    assert(uth->ut_state == UT_ZOMBIE);
    NOT_YET_IMPLEMENTED("UTHREADS: uthread_destroy");
}

static uthread_cond_t reap_cond;

/*
 * reaper_init
 *
 * Initializes the reap queue, the reap mutex, and the reap condition variable
 */
static void reaper_init(void)
{
    NOT_YET_IMPLEMENTED("UTHREADS: reaper_init");
}

/*
 * reaper
 *
 * This is responsible for going through all the threads on the dead
 * threads list (which should all be in the ZOMBIE state) and then
 * cleaning up all the threads that have been detached/joined with
 * already.
 *
 * In addition, when there are no more runnable threads (besides the
 * reaper itself) it will call exit() to stop the program.
 */
static void *reaper(long a0, char *a1[])
{
    uthread_nopreempt_on();
    uthread_mtx_lock(&reap_mtx); // get exclusive access to reap queue

    while (1)
    {
        uthread_t *thread;
        int th;

        while (utqueue_empty(&reap_queue))
        {
            // wait for a thread to join the reap queue
            uthread_cond_wait(&reap_cond, &reap_mtx);
        }

        while (!utqueue_empty(&reap_queue))
        {
            // deal with all threads on reap queue
            thread = utqueue_dequeue(&reap_queue);
            assert(thread->ut_state == UT_ZOMBIE);
            uthread_destroy(thread);
        }

        /* check and see if there are still runnable threads */
        for (th = 0; th < UTH_MAX_UTHREADS; th++)
        {
            if (th != reaper_thr_id && uthreads[th].ut_state != UT_NO_STATE)
            {
                break;
            }
        }

        if (th == UTH_MAX_UTHREADS)
        {
            /* we leak the reaper's stack */
            fprintf(stderr, "uthreads: no more threads.\n");
            fprintf(stderr, "uthreads: bye!\n");
            exit(0);
        }
    }
    return 0;
}

void uthread_runq_enqueue(uthread_t *thr);

/*
 * Turns the main context (the 'main' routine that initialized
 * this process) into a regular uthread that can be switched
 * into and out of. Must be called from the main context (i.e.,
 * by uthread_start()). Also creates the reaper thread.
 */
static void create_first_thr(uthread_func_t firstFunc, long argc,
                             char *argv[])
{
    // clock interrupts are masked
    masked = 1;

    // Set up uthread context for main thread
    // This does what uhtread_alloc does, but without a valid ut_curthr
    uthread_id_t tid = 0;
    uthreads[0].ut_state = UT_TRANSITION;
    ut_curthr = &uthreads[tid];
    memset(&ut_curthr->ut_link, 0, sizeof(list_link_t));
    ut_curthr->ut_prio = UTH_MAXPRIO;
    ut_curthr->ut_exit = NULL;
    ut_curthr->ut_detached = 1;
    utqueue_init(&ut_curthr->ut_waiter);
    ut_curthr->ut_state = UT_RUNNABLE;
    // Thread must start with preemption disabled: this is assumed in
    // uthread_switch
    ut_curthr->ut_no_preempt_count =
        1; // thread created with clock interrupts masked; this is assumed in uthread_switch
    ut_curthr->ut_stack = alloc_stack();
    if (ut_curthr->ut_stack == NULL)
    {
        PANIC("Could not create stack for first thread.");
    }

    uthread_makecontext(&ut_curthr->ut_ctx, ut_curthr->ut_stack, UTH_STACK_SIZE,
                        firstFunc, argc, argv);

    uthread_runq_enqueue(ut_curthr);
    
    // first thread is now on run q; next step is to set up the reaper thread
    reaper_thr_id = uthread_alloc();
    ut_curthr = &uthreads[reaper_thr_id];
    memset(&ut_curthr->ut_link, 0, sizeof(list_link_t));
    ut_curthr->ut_prio = UTH_MAXPRIO;
    ut_curthr->ut_exit = NULL;
    ut_curthr->ut_detached = 1;
    utqueue_init(&ut_curthr->ut_waiter);
    ut_curthr->ut_state = UT_RUNNABLE;
    ut_curthr->ut_no_preempt_count =
        1; 
    ut_curthr->ut_stack = alloc_stack();
    if (ut_curthr->ut_stack == NULL)
    {
        PANIC("Could not create stack for reaper thread.");
    }

    uthread_makecontext(&ut_curthr->ut_ctx, ut_curthr->ut_stack, UTH_STACK_SIZE,
                        reaper, 0, 0);

    uthread_runq_enqueue(ut_curthr);
    // reaper thread is now on run q
    ut_curthr = NULL; // at the moment we don't have a current uthread --
                      // they're both on the run q.
}

/*
 * Adds the given thread to the reaper's queue, and wakes up the reaper.
 * Called when a thread is completely dead.
 * 
 *  1. This function should be called with reap_mtx locked
 *     to synchronize access to the reap_queue with the reaper uthread.
 *     You should figure out where to unlock reap_mtx in this function.
 *   
 *  2. You need to make sure that thread resources aren't destroyed
 *     too early.
 *   
 *  3. Make sure the reaper is alerted when a thread is added to
 *     the reap queue. 
 *   
 * Hint: Consider ALL possible uthread inputs to this function!
 */
static void make_reapable(uthread_t *uth)
{
    // reap_mtx is locked by caller, but is unlocked
    // here. Clock interrupts are masked. Can't lock
    // reap_mtx here: caller might be a zombie.
    assert(uth->ut_state == UT_ZOMBIE);
    assert(masked);

    NOT_YET_IMPLEMENTED("UTHREADS: make_reapable");
}

static char *alloc_stack(void)
{
    // malloc/free are pthread-safe since we're building on top
    // of pthreads, but we must protect from clock interrupts to
    // make them uthreads-safe
    uthread_nopreempt_on();
    char *stack = (char *)malloc(UTH_STACK_SIZE);
    uthread_nopreempt_off();
    return stack;
}

static void free_stack(char *stack)
{
    uthread_nopreempt_on();
    free(stack);
    uthread_nopreempt_off();
}
