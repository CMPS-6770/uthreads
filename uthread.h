/*
 *   FILE: uthread.h
 * AUTHOR: Peter Demoreuille
 *  DESCR: userland threads
 *   DATE: Sat Sep  8 10:56:08 2001
 *
 * Modified considerably by Tom Doeppner
 *   DATE: Sun Jan 10, 2016 and Jan 2023
 *
 */

#ifndef __uthread_h__
#define __uthread_h__

#include <pthread.h>
#include <signal.h>
#include <sys/types.h>

#include "list.h"
#include "uthread_ctx.h"
#include "uthread_queue.h"

/* -------------- defs -- */

#define UTH_MAXPRIO 7             /* max thread prio */
#define UTH_MAX_UTHREADS 512      /* max threads */
#define UTH_STACK_SIZE 128 * 1024 /* stack size */

#define NOT_YET_IMPLEMENTED(msg)                                          \
    do                                                                    \
    {                                                                     \
        fprintf(stderr, "Not yet implemented at %s:%i -- %s\n", __FILE__, \
                __LINE__, (msg));                                         \
    } while (0);

#define PANIC(err)                                                          \
    do                                                                      \
    {                                                                       \
        fprintf(stderr, "PANIC at %s:%i -- %s\n", __FILE__, __LINE__, err); \
        abort();                                                            \
    } while (0);

typedef int uthread_id_t;
typedef void *(*uthread_func_t)(long, char *argv[]);

typedef enum
{
    UT_NO_STATE,   /* invalid thread state */
    UT_ON_CPU,     /* thread is running */
    UT_RUNNABLE,   /* thread is runnable */
    UT_WAIT,       /* thread is blocked */
    UT_ZOMBIE,     /* zombie threads eat your brains! */
    UT_TRANSITION, /* not yet a thread, but will be soon */

    UT_NUM_THREAD_STATES
} uthread_state_t;

/* --------------- thread structure -- */

typedef struct uthread
{
    list_link_t ut_link; /* link on waitqueue / scheduler */

    uthread_ctx_t ut_ctx;     /* context */
    char *ut_stack;           /* user stack */
    uthread_id_t ut_id;       /* thread's id */
    uthread_state_t ut_state; /* thread state */
    int ut_prio;              /* thread's priority */
    void *ut_exit;            /* thread's exit value */
    int ut_detached;          /* thread is detached? */
    utqueue_t ut_waiter;      /* thread waiting to join with me */
    int ut_no_preempt_count;  /* used for nested calls to turn off preemption */
} uthread_t;

extern uthread_t uthreads[UTH_MAX_UTHREADS];
extern int masked;
extern uthread_t *ut_curthr;
extern sigset_t VTALRMmask;

/* --------------- prototypes -- */

void uthread_start(uthread_func_t firstFunc, long argc, char *argv[]);

int uthread_create(uthread_id_t *id, uthread_func_t func, long arg1, void *arg2,
                   int prio);
void uthread_exit(void *status);
uthread_id_t uthread_self(void);

int uthread_join(uthread_id_t id, void **exit_value);
int uthread_detach(uthread_id_t id);

extern void uthread_nopreempt_on();
extern void uthread_nopreempt_off();

#define MTprintf(fmt, args...) \
    uthread_nopreempt_on();    \
    printf(fmt, args);         \
    uthread_nopreempt_off();

#endif /* __uthread_h__ */
