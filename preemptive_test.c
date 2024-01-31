/*
 * Simple preemption test for uthreads. Ensures that preemption happens
 * WITHOUT explicit calls to uthread_yield()
 * 
 * There are some ugly things in these tests. We often make threads spin
 * instead of having them wait on a condition variable or barrier or something.
 * We do this because we're trying to test that preemption happens
 * automatically, without blocking calls (like waiting on a condition variable)
 */
#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/errno.h>

#include "uthread.h"
#include "uthread_mtx.h"
#include "uthread_cond.h"
#include "uthread_sched.h"


#define HALF_UTHREADS (UTH_MAX_UTHREADS / 2)

char buffer[1024];
#define DEBUG(fmt, args...) sprintf(buffer, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, ## args); \
    write(STDOUT_FILENO, buffer, strlen(buffer));

#define test_assert(x, args...) \
    do { \
        if (!(x)) { \
            DEBUG("FAILED: %s, %s\n", #x, #args); \
            return_code = 134; \
        } \
    } while (0)

#define test_error(x, err) \
    test_assert(x == err)

//
// There are cases here where we want to use this for waiting rather than sleep(),
// because sleep will cause the underlying process to block.
// We also want to test that preemption is happening automatically every
// millisec, so we don't want to explicitly yield.
//
// This could probably better be done using alarm() and a signal handler,
// but we're also using SIGVTALRM within uthreads, and if that were changed
// to SIGALRM, it would break everything
//
void spin_for_n_seconds(int n_seconds) {
    time_t end_time = time(NULL) + n_seconds;

    while (time(NULL) < end_time) {
        // Do nothing
    }
}


static void *yield_and_exit(long a0, char* a1[]);
static void *do_nothing_and_exit(long exit_val, char* a1[]);
static void *detach_self_and_exit(long exit_val, char* a1[]);

enum {
    TEST_PREEMPTION_HAPPENS,
    TEST_MUTEXES,
    TEST_DIFFERENT_THREAD_ID,
    TEST_PRIORITY,
    TEST_DETACHED,
    TEST_JOIN,
    TEST_CREATE_STRESS,

    TEST_NUM_TESTS,
};


volatile int test_counter;
uthread_mtx_t test_mtx;
int return_code;

void test_init() {
    test_counter = 0;
    return_code = 0;
    
    uthread_mtx_init(&test_mtx);
}


/*
  Start of test_preemption_happens
 */
volatile int a_counter;
volatile int b_counter;
#define LAST_VALUE 500

void *threada(long a0, char* a1[]) {
    int oldb = 0;
    while (1) {
        if (b_counter > oldb) {
            oldb = b_counter;
            a_counter++;

            if (a_counter == LAST_VALUE - 1) {
                uthread_exit((void *)0);
            }
        }
    }
    return NULL; 
}

void *threadb(long a0, char* a1[]) {
    b_counter = 1;
    int olda = a_counter;
    while (1) {
        if (a_counter > olda) {
            olda = a_counter;
            b_counter++;

            if (b_counter == LAST_VALUE) {
                uthread_exit((void *)0);
            }
        }
    }
    return NULL; 
}

void test_preemption_happens() {
    uthread_id_t a_id, b_id;
    a_counter = 0;
    b_counter = 0;

    DEBUG("Testing if preemption actually happens. "
        "If this hangs for more than ~10 seconds, this test has FAILED\n");
    
    uthread_create(&a_id, threada, 0, NULL, 0);
    uthread_create(&b_id, threadb, 0, NULL, 0);

    DEBUG("Passing in NULL for return_value in uthread_join\n"); 
    uthread_join(a_id, NULL);
    uthread_join(b_id, NULL);

    test_assert(a_counter == LAST_VALUE - 1 && b_counter == LAST_VALUE);
}

void *lock_mutex_and_add_n_times(long n_iters, char* a1[]) {
    // Make sure other threads have a chance to try to get put on the mutex's queue
    uthread_mtx_lock(&test_mtx);
    uthread_yield();
    uthread_mtx_unlock(&test_mtx);
    
    int i;
    for (i = 0; i < n_iters; i++) {
        uthread_mtx_lock(&test_mtx);
        test_counter++;
        uthread_mtx_unlock(&test_mtx);
    }
    
    return NULL; 
}

/**
 * The purpose of this test is to expose mistakes in uthread_mtx_lock
 * by making a mutex and having lots of threads compete for it.
 * If preemption is not correctly disabled in uthread_mtx_lock
 * there is a good chance this test will fail
 */
void test_mutexes() {
    int i;
    const int N_ITERS = 10000;
    test_init();

    DEBUG("Testing mutexes.\n");

    uthread_id_t threads[HALF_UTHREADS];

    uthread_nopreempt_on();
    for (i = 0; i < HALF_UTHREADS; i++) {
        test_assert(0 == uthread_create(&threads[i], lock_mutex_and_add_n_times,
                                        N_ITERS, NULL, 0));
    }
    uthread_nopreempt_off();
    
    DEBUG("Passing in NULL for return value in uthread_join\n"); 
    for (i = 0; i < HALF_UTHREADS; i++) {
        test_assert(0 == uthread_join(threads[i], NULL));
    }

    test_assert(test_counter == N_ITERS * HALF_UTHREADS, "shared counter was not modified correctly");
}


/*
  Start of priority test
 */


static void *high_priority_thread(long a0, char* a1[]) {
    int somevalue = 0;
    while (test_counter == 0) {
        // Do some random work
        somevalue += 22;
        somevalue *= 53;
        somevalue %= 113;
        //
    }

    uthread_exit((void *)0);
    return NULL; 
}

static void *low_priority_thread(long a0, char* a1[]) {
    // Sets the counter to 1. If this runs before the high priority threads
    // then all of the high priority threads will stop (and our test will fail)
    test_counter = 1;
    uthread_exit((void *)0);
    return NULL; 
}


/*
 * fire up a bunch of threads at different priorities, make sure
 * higher priority ones run first
 */
static void test_priority() {
    int i;
    test_counter = 0;
    uthread_id_t low_prio_thread_id;

    DEBUG("Testing priorities\n");
    
    // First make sure we have a high priority
    test_assert(0 == uthread_setprio(uthread_self(), UTH_MAXPRIO));

    // Fire up some high priority threads (with equal priority to us)
    uthread_id_t high_prio_threads[HALF_UTHREADS];
    for (i = 0; i < HALF_UTHREADS; i++) {
        test_assert(0 == uthread_create(&high_prio_threads[i],
                                        high_priority_thread,
                                        0, NULL,
                                        UTH_MAXPRIO));
    }

    // They might preempt us here, but they're spinning, so it shouldn't matter.
    DEBUG("High priority threads are now running\n");

    // Now start up the low priority thread
    test_assert(0 == uthread_create(&low_prio_thread_id,
                                    low_priority_thread,
                                    0, NULL,
                                    0));

    DEBUG("Low priority thread now scheduled (but hopefully not running)\n");

    // Wait a few seconds, the scheduler should preempt us a whole bunch
    // and let those high priority threads run, but never the low priority thread
    spin_for_n_seconds(5);

    // If test_counter == 1, then the low priority thread ran
    test_assert(test_counter == 0);

    // Tell the high priority threads they can execute
    test_counter = 101;
    DEBUG("Passing in NULL for return_value in uthread_join\n")
    for (i = 0; i < HALF_UTHREADS; i++) {
        test_assert(0 == uthread_join(high_prio_threads[i], NULL));
    }

    // Now join the low priority thread
    DEBUG("Passing in NULL for return_value in uthread_join\n")
    test_assert(0 == uthread_join(low_prio_thread_id, NULL));
    test_assert(test_counter == 1);
}

/**
 * make a bunch of threads, detach them all
 * and then make sure they all completed
 */
void test_detached() {
    // Trying joining to a detached thread
    uthread_id_t id;

    DEBUG("Testing detached threads\n");

    //
    // Try to join a detached thread
    //
    test_assert(0 == uthread_create(&id, do_nothing_and_exit,
                                    0, NULL, 0));

    // The thread have already finished by here, but it doesn't matter.
    test_assert(0 == uthread_detach(id));
    test_assert(uthread_join(id, NULL));

    //
    // Try to detach a thread that's been joined already
    //
    // test_assert(0 == uthread_create(&id, do_nothing_and_exit,
    //                                 0, NULL, 0));
    // test_assert(0 == uthread_join(id, NULL));

    // The thread should now be in the detached state OR have completed
    // and been destroyed by the reaper.
    // Calling detach() on an already joined thread is technically undefined
    // behavior, but this implementation of uthreads returns a nice error
    // test_assert(!!uthread_detach(id));

    //
    // Have a thread detach itself, and make sure it still gets to run
    // and set test_counter
    //
    test_counter = 0;
    test_assert(0 == uthread_setprio(uthread_self(), 0));
    test_assert(0 == uthread_create(&id, detach_self_and_exit,
                                    0, NULL, UTH_MAXPRIO));

    // Give this thread plenty of time to run, detach itself
    // and set the counter, but don't explicitly yield.
    spin_for_n_seconds(5);

    test_assert(test_counter == 1);
    test_assert(uthread_detach(id), "Does not catch already detached/terminated thread"); 
    DEBUG("Passing in NULL for return_value in uthread_join\n")
    test_assert(uthread_join(id, NULL));
}

/*
 * Create threads until we fail (test the out of resources error EAGAIN).
 * Then join with one of the threads, wait for the reaper to free its resources,
 * (without explicly yielding to the reaper) and then try creating it again
 */
void alloc_too_many_threads() {
    long i, ret;
    long end = 0;
    const long REAPER_SECONDS = 5;
    uthread_id_t threads[UTH_MAX_UTHREADS];

    DEBUG("Stress testing thread creation\n");
    for (i = 0; i < UTH_MAX_UTHREADS; i++) {
        ret = uthread_create(&threads[i], do_nothing_and_exit,
                             i, NULL, 0);

        if (ret) {
            end = i;
            break;
        }
    }

    test_assert(end > 0 && "Could not make even one thread!");
    if (end == 0) {
        DEBUG("Test failed to complete\n");
        return;
    }

    DEBUG("Created %ld threads, and got EAGAIN. Joining with one.\n", end);
    
    // Now join with the last thread we successfully created, and let the reaper run
    i = end - 1;
    test_assert(0 == uthread_join(threads[i], (void **)&ret));
    test_assert(ret == i);

    DEBUG("Giving time for reaper to run\n");

    // Give the reaper some time to run (but don't explicitly yield)
    // The reaper has a high priority, so this may not even be necessary
    spin_for_n_seconds(REAPER_SECONDS);

    DEBUG("Reaper should have run by now. Creating a new thread.\n");
    // Now try to alloc another thread. The resources should now be free,
    // and it should be created successfully
    ret = uthread_create(&threads[i], do_nothing_and_exit,
                         i, NULL, 0);
    if (ret) {
        DEBUG("Resources for thread were not freed after %ld seconds. "\
              "Something is probably wrong.\n", REAPER_SECONDS);
    } else {
        // There should be no other error with this
        test_assert(ret == 0);
    }

    // Cleanup. join with all the threads we created
    for (i = 0; i < end; i++) {
        test_assert(0 == uthread_join(threads[i], (void **)&ret));
        test_assert(ret == i, "uthread_join is setting return_value incorrectly");
    }
}

/**
 * Test join
 */
void test_join() {
    const long EXPECTED_RET = 44;
    uthread_id_t id;
    long retval;

    DEBUG("Testing join\n");
    
    //
    // Try joining with a thread which has almost definitely already completed
    //
    test_assert(0 == uthread_create(&id, do_nothing_and_exit,
                                    EXPECTED_RET, NULL, UTH_MAXPRIO));

    // Give it plenty of time to complete, but don't explicitly yield
    spin_for_n_seconds(5);

    // Now join with it (should work)
    test_assert(0 == uthread_join(id, (void **)&retval));
    test_assert(retval == EXPECTED_RET, "uthread_join set return_value incorrectly");
}


/**
 * Create a thread, ensure its thread id is differnet from thread_self()
 */
void test_different_thread_id() {
    uthread_id_t id;
    DEBUG("Testing thread ids\n");

    test_assert(0 == uthread_create(&id, yield_and_exit,
                                    0, NULL, 0));
    test_assert(id != uthread_self());
    DEBUG("Passing in NULL for return_value in uthread_join\n"); 
    uthread_join(id, NULL);
}

/**
 * The function that the first thread is going to execute 
*/
void* mainthread(long ac, char *av[]) {
    int ntest; 

    ntest = atoi(av[1]);
    switch (ntest) {
        case TEST_PREEMPTION_HAPPENS:
            test_preemption_happens();
            break;
        case TEST_MUTEXES:
            test_mutexes();
            break;
        case TEST_DIFFERENT_THREAD_ID:
            test_different_thread_id();
            break;
        case TEST_PRIORITY:
            test_priority();
            break;
        case TEST_DETACHED:
            test_detached();
            break;
        case TEST_JOIN:
            test_join();
            break;
        case TEST_CREATE_STRESS:
            alloc_too_many_threads();
            break;
        default:
            fprintf(stderr, "Invalid test number.\n");
    }

    // Each execution of this program runs a single test and
    // each test changes the exit_code if any assertion has failed.
    // return the exit_code so that the runner knows if the test passed
    // or failed.
    exit(return_code);

    uthread_exit((void *)0); // unnecessary, can not execute
    DEBUG("Thread %i got to the end of main, Should have exited THIS IS BAD\n", uthread_self());

    return 0;
}

int
main(int ac, char **av)
{
    if (ac < 2) {
        fprintf(stderr, "Usage: %s <TEST_NO.> (0-%d)\n", av[0],
                TEST_NUM_TESTS - 1);
        return -1;
    }
    
    // initialize the threads package 
    uthread_start(mainthread, ac, av);
}

static void *yield_and_exit(long exit_val, char* a1[]) {
    uthread_yield();
    uthread_exit((void *)exit_val);
    return NULL; 
}

static void *do_nothing_and_exit(long exit_val, char* a1[]) {
    uthread_exit((void *)exit_val);
    return NULL; 
}

static void *detach_self_and_exit(long exit_val, char* a1[]) {
    test_assert(0 == uthread_detach(uthread_self()), "detaching current thread failed.");
    test_counter = 1;
    uthread_exit((void *)exit_val);
    return NULL;
}