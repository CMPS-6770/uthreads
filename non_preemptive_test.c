/*
 *     FILE: test.c 
 * AUTHOR: Sean Andrew Cannella (scannell)
 *    DESCR: a simple test program for uthreads
 *     DATE: Mon Sep 23 14:11:48 2002
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/errno.h>
#include <signal.h>
#include "uthread.h"
#include "uthread_mtx.h"
#include "uthread_cond.h"
#include "uthread_sched.h"

#define TEST_NUM_FORKS 4

#define SBUFSZ 256

#define DEBUG(fmt, args...) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, ## args)

#define test_assert(x, args...) \
    do { \
        if (!(x)) { \
            DEBUG("FAILED: %s, %s\n", #x, #args); \
            exit_code = 134; \
        } \
    } while (0)

#define test_error(x, err) \
    test_assert(x, "Invalid error")

static void get_clock_sigset(sigset_t* set) {
    sigemptyset(set);
    sigaddset(set, SIGVTALRM);
}


// Note that we should disable preemption this way, for tests, and not
// with uthread_no_preempt_on, because that gets reset anytime we call
// uthread_block (or any other functions which calls uthread_switch)
// because after switching, uthread_nopreempt_reset is called.
static void disable_preemption() {
    sigset_t toblock;
    get_clock_sigset(&toblock);
    sigprocmask(SIG_BLOCK, &toblock, NULL);
}

//                 
/* XXX: we're using sprintf and write to emulate printf - but, we're not 
 * really being as judicious as we should be about guarding write. */

static void *mutex_fifo_test_A(long a0, char* a1[]);
static void *mutex_fifo_test_B(long a0, char* a1[]);
static void *mutex_exclusion_test_A(long a0, char* a1[]);
static void *mutex_exclusion_test_B(long a0, char* a1[]);
static void *cond_lock_test_A(long a0, char* a1[]);
static void *cond_lock_test_B(long a0, char* a1[]);
static void *cond_lock_test_incr(long a0, char* a1[]);
static void *join_sleeping_test_A(long a0, char* a1[]);
static void *join_sleeping_test_B(long a0, char* a1[]);
static void *setprio_sleeping_thread(long a0, char* a1[]);
static void *thread_dummy_function(long a0, char* a1[]);
static void *thread_add_ten_to_counter(long a0, char* a1[]);
static void *thread_add_ten_yield_mutex(long a0, char* a1[]);
static void *thread_add_ten_to_counter_with_initial_wait(long a0, char* a1[]);
static void *thread_fork_creation(long a0, char* a1[]);
static void *increment_test_counter_by_value(long value, char* a1[]);
static void *increase_priority_of_other_thread(long thread_id, char* a1[]);
static void test_setup();
static void test_reset();
static void test_cleanup();

static void test_queue();
static void test_main_thread();
static void test_single_thread();
static void test_mutexes();
static void test_multiple_threads();
static void test_locks_and_conditions();
static void test_priority();
static void test_detached_threads();
static void test_thread_bomb();
static void test_thread_fork_creation();
static void test_priority_change();

enum {
    TEST_QUEUE,
    TEST_MAIN_THREAD,
    TEST_SINGLE_THREAD,
    TEST_MUTEX,
    TEST_MULTIPLE_THREADS,
    TEST_LOCKS_AND_CONDS,
    TEST_PRIORITY,
    TEST_DETACHED_THREADS,
    TEST_THREAD_BOMB,
    TEST_FORK_CREATION,
    TEST_PRIORITY_CHANGE,
};

long test_counter;
char test_counter_guard;
uthread_mtx_t test_mtx;
uthread_cond_t test_cond;
char test_pause;
uthread_mtx_t test_pause_mtx;
uthread_cond_t test_pause_cond;

int exit_code;

void* mainthread(long ac, char *av[]) {
    int ntest; 

    test_setup();

    // Preemption is off for all of these tests,
    // each test function will call uthread_yield when we want other threads to
    // run
    disable_preemption();

    ntest = atoi(av[1]);
    switch (ntest) {
        case TEST_QUEUE:
            test_queue(); // tests basic functionality of queues, nothing that students implement
            break;
        case TEST_MAIN_THREAD:
            test_main_thread();
            break;
        case TEST_SINGLE_THREAD:
            test_single_thread(); // test joining with thread and ESRCH errors: -1, 
            break;
        case TEST_MUTEX:
            test_mutexes();
            break;
        case TEST_MULTIPLE_THREADS:
            test_multiple_threads();
            break;
        case TEST_LOCKS_AND_CONDS:
            test_locks_and_conditions();
            break;
        case TEST_PRIORITY:
            test_priority();
            break;
        case TEST_DETACHED_THREADS:
            test_detached_threads();
            break;
        case TEST_THREAD_BOMB:
            //test_thread_bomb(); // TODO: this test is currently skipped 
            break;
        case TEST_FORK_CREATION:
            test_thread_fork_creation(); // test thread creation and joining with them + return values
            break;
        case TEST_PRIORITY_CHANGE:
            test_priority_change(); // tests that uthread_yield will be called
            break;
        default:
            fprintf(stderr, "Invalid test number.\n");
    }

    test_cleanup();
    // Each execution of this program runs a single test and
    // each test changes the exit_code if any assertion has failed.
    // return the exit_code so that the runner knows if the test passed
    // or failed.
    exit(exit_code); 

    uthread_exit((void *)0); // unnecessary, can not execute
    DEBUG("Thread %i got to the end of main, Should have exited THIS IS BAD\n", uthread_self());
    return NULL; 
}

int
main(int ac, char **av)
{
    if (ac < 2) {
        fprintf(stderr, "Usage: %s <test_num>\n", av[0]);
        return -1;
    }
    uthread_start(mainthread, 0, av);
    fprintf(stderr, "########### EXIT CODE ##############: %d\n", exit_code);
    exit(35); // Use the exit code to indicate if test passed.
    //
}

/*
 * Initialize mutexes, conditions, counters, guard booleans, etc.
 */
static void test_setup(){
    exit_code = 0;
    uthread_mtx_init(&test_mtx);
    uthread_cond_init(&test_cond);
    uthread_mtx_init(&test_pause_mtx);
    uthread_cond_init(&test_pause_cond);
    test_reset();
}

/*
 * Reset counters and guard booleans
 */ 
static void test_reset(){
    test_counter = 0;
    test_counter_guard = 0;
    test_pause = 0;
}

/*
 * In case anything needs to be cleaned up before exiting
 */
static void test_cleanup(){

}

/*
 * Test the queue. Not written by students, but still worthwhile to test.
 * Threads are created on the spot, they are not actual running threads.
 */
static void test_queue(){
    DEBUG("Running test_queue()\n");
    utqueue_t queue;
    uthread_t threads[10];
    uthread_t* deqthr;
    long num_threads = 10;
    long i;

    utqueue_init(&queue);

    /* Initialize thread link fields since these are dummy threads */
    for (i = 0; i < num_threads; i++){
        threads[i].ut_link.l_next = NULL;
        threads[i].ut_link.l_prev = NULL;
    }

    for (i = 0; i < num_threads; i++){
        threads[i].ut_link.l_next = NULL;
        threads[i].ut_link.l_prev = NULL;
        utqueue_enqueue(&queue, &threads[i]);                
    }
    test_assert(!utqueue_empty(&queue));

    for (i = 0; i < num_threads; i++){
        deqthr = utqueue_dequeue(&queue);
        test_assert(deqthr == &threads[i]);
    }
    test_assert(utqueue_empty(&queue));
    //test_assert(!utqueue_empty(&queue)); // THIS IS WRONG, testing autograder only
    DEBUG("Finished test_queue()\n");
}

/*
 * Test simple functions on the main thread
 */
static void test_main_thread(){
    DEBUG("Running test_main_thread()\n");

    test_assert(uthread_self() == 0, "main thread id is not correct");
    uthread_setprio(uthread_self(), 7);
    uthread_yield();
    uthread_setprio(uthread_self(), 0);
    uthread_yield();

    DEBUG("Finished test_main_thread()\n");
}

// static void *uthread_self_joiner(long a0, char *a1[]) {
//     long s;
//     test_error(uthread_join(uthread_self(), (void **)&s), EDEADLK);
//     return NULL; 
// }

static void *uthread_failed_joiner(long a0, char *a1[]) {
    long status;
    uthread_id_t id = (uthread_id_t)a0;
    test_error(uthread_join(id, (void **)&status), EINVAL);
    return NULL;
}

static void *uthread_joinee(long a0, char *a1[]) {
    uthread_yield();
    return NULL; 
}

/*
 * Test the creation and execution of a single thread.
 * Test various errors associated with joining
 */ 
static void test_single_thread(){
    DEBUG("Running test_single_thread()\n");
    uthread_id_t thr;
    long expected_exit_value = 25;
    long exit_value;

    DEBUG("\tCreating and joining:\n");
    test_reset();
    test_assert(0 == uthread_create(&thr, thread_dummy_function, expected_exit_value, NULL, 0));
    test_assert(0 == uthread_join(thr, (void **)&exit_value));
    test_assert(exit_value == expected_exit_value, "uthread_join not setting return_value correctly");

    DEBUG("\tChecking join ESRCH errors:\n");
    uthread_yield(); /* let the reaper reap thr */
    test_error(uthread_join(thr, (void **)&exit_value), ESRCH); // TODO: not asking students to handle this anymore
    test_error(uthread_join(-1,(void **)&exit_value), ESRCH); 
    test_error(uthread_join(UTH_MAX_UTHREADS+1,(void **)&exit_value), ESRCH);

    DEBUG("Finished test_single_thread()\n");
}

/*
 * Tests that mutexes satisfy FIFO and mutex properties
 */
static void test_mutexes() {
    DEBUG("Running test_mutexes()\n");
    
    uthread_id_t A;
    uthread_id_t B;

    /* exclusion test */
    DEBUG("\tChecking exclusion:\n");
    test_reset();
    test_assert(0 == uthread_create(&A, &mutex_exclusion_test_A, 0, 0, 4));
    test_assert(0 == uthread_create(&B, &mutex_exclusion_test_B, 0, 0, 4));
    DEBUG("\t\tPassing NULL for return_value to uthread_join\n"); 
    uthread_join(A, 0);
    uthread_join(B, 0);

    /* FIFO test */
    DEBUG("\tChecking FIFO:\n");
    test_reset();
    test_assert(0 == uthread_create(&A, &mutex_fifo_test_A, 0, 0, 4));
    test_assert(0 == uthread_create(&B, &mutex_fifo_test_B, 0, 0, 4));
    DEBUG("\t\tPassing NULL for return_value to uthread_join\n"); 
    uthread_join(A, 0);
    uthread_join(B, 0);

    /* larger test. Increments counters, yield with mutex locked, etc */
    DEBUG("\tChecking shared data (general mutex test):\n");
    uthread_id_t thr[8];
    long num_thr = 8;
    long thread_number, exit_value;

    test_reset();
    for(thread_number = 0; thread_number < num_thr; thread_number++){
        test_assert(0 == uthread_create(&thr[thread_number], thread_add_ten_yield_mutex, thread_number, NULL, 0));
    }
    for (thread_number = 0; thread_number < num_thr; thread_number++){
        exit_value = 0;
        test_assert(0 == uthread_join(thr[thread_number], (void **)&exit_value));
        test_assert(exit_value == thread_number, "not setting return_value correctly in uthread_join");
    }
    test_assert(test_counter == 10*num_thr, "shared data result is not as expected");

    /* trylock tests */
    DEBUG("\tTrylock test:\n");
    test_reset();
    test_assert(0 == uthread_create(&A, cond_lock_test_A, 0, 0, UTH_MAXPRIO));
    uthread_yield();
    test_assert(0 == uthread_mtx_trylock(&test_mtx), "Trylock should fail");
    test_assert(0 == uthread_join(A, (void **)&exit_value));

    uthread_mtx_lock(&test_mtx);
    test_assert(0 == uthread_mtx_trylock(&test_mtx), "Trylock should fail");
    uthread_mtx_unlock(&test_mtx);
    // added 2023: attempt to lock the mutex, should acquire now 
    test_assert(1 == uthread_mtx_trylock(&test_mtx), "Trylock should succeed"); 
    uthread_mtx_unlock(&test_mtx); 

    DEBUG("Finished test_mutexes()\n");
}

/*
 * Same as single thread, except now there's more.
 */
static void test_multiple_threads() {
    DEBUG("Running test_multiple_threads()\n");
    // uthread_id_t sthr;
    uthread_id_t thr[8];
    long num_threads = 8;
    long thread_number, exit_value;

    DEBUG("\tGenerally, can run mulitple threads:\n");
    test_reset();
    for(thread_number = 0; thread_number < num_threads; thread_number++){
        test_assert(0 == uthread_create(&thr[thread_number], thread_dummy_function, thread_number, NULL, 0));
    }
    for (thread_number = 0; thread_number < num_threads; thread_number++){
        exit_value = 0;
        test_assert(0 == uthread_join(thr[thread_number], (void **)&exit_value));
        test_assert(exit_value == thread_number, "not setting return_value correctly in uthread_join");
    }

    /* join with a thread that has already finished */
    //DEBUG("\tJoin with a thread that finished already:\n");
    // test_assert(0 == uthread_create(thr, thread_dummy_function, 0, 0, UTH_MAXPRIO));
    //uthread_yield();
    // test_assert(0 == uthread_join(thr[0], (void **)&exit_value));

    /* join makes threads reapable */
    DEBUG("\tJoin makes threads reapable\n");
    long i, j;
    for (i = 0; i < 4; i++) {
        uthread_id_t ids[UTH_MAX_UTHREADS / 2 + 1];
        for (j = 0; j < UTH_MAX_UTHREADS / 2; j++) {
            test_assert(0 == uthread_create(&ids[j], thread_dummy_function, 0, 0, UTH_MAXPRIO));
        }
        for (j = 0; j < UTH_MAX_UTHREADS / 2; j++) {
            test_assert(0 == uthread_join(ids[j], (void **)&exit_value));
        }
        /* yield to let reaper run.  This is technically a bug,
           hopefully fixed after 2015 */
        uthread_yield();
    }

    /* this thread is detached so we create another thread for EDEADLK */
    // DEBUG("\tEDEADLK test for joining with self:\n");
    // test_assert(0 == uthread_create(&sthr, uthread_self_joiner, 0, 0, UTH_MAXPRIO));
    // uthread_yield();
    // uthread_join(sthr, (void **)&exit_value);

    test_reset();
    uthread_id_t A, B;
    /* test that join blocks on ANY non-zombie state */
    DEBUG("\tJoin blocks on all states, not just runnables:\n");
    test_assert(0 == uthread_create(&A, join_sleeping_test_A, 0, 0, UTH_MAXPRIO));
    test_assert(0 == uthread_create(&B, join_sleeping_test_B, 0, 0, UTH_MAXPRIO));
    uthread_yield(); /* yield to A, who yields to B, who then blocks */
    test_assert(0 == uthread_join(B, (void **)&exit_value));
    test_assert(test_counter == 1, "Thread B did not run correctly");
    uthread_join(A, (void **)&exit_value);

    /* test for EINVAL from joining with a thread someone is already joining with */
    DEBUG("\tEINVAL test for mulitple threads joining with the same thread\n");
    uthread_id_t failed_joiner, joinee;
    test_assert(0 == uthread_create(&joinee, uthread_joinee, 0, 0, 3));
    test_assert(0 == uthread_create(&failed_joiner, uthread_failed_joiner, joinee, 0, 3));
    test_assert(uthread_join(joinee, (void **)&exit_value) == 0);
    test_assert(uthread_join(failed_joiner, (void **)&exit_value) == 0);

    DEBUG("Finished test_multiple_threads()\n");
}

/*
 * Uses add_ten_to_counter function to test locks and conditions. Assumes
 * threads are working properly at this point.
 */
static void test_locks_and_conditions(){
    DEBUG("Running test_locks_and_conditions()\n");
    uthread_id_t thr[8];
    long num_thr = 8;
    long thread_number, exit_value;

    test_reset();
    DEBUG("\tGeneral locks and conditions test\n");
    for(thread_number = 0; thread_number < num_thr; thread_number++){
        test_assert(0 == uthread_create(&thr[thread_number], thread_add_ten_to_counter, thread_number, NULL, 0));
    }
    for (thread_number = 0; thread_number < num_thr; thread_number++){
        exit_value = 0;
        test_assert(0 == uthread_join(thr[thread_number], (void **)&exit_value));
        test_assert(exit_value == thread_number, "return_value not set correctly in uthread_join");
    }
    test_assert(test_counter == 10*num_thr);


    test_reset();
    /* test that cond vars get the lock when waking up */
    DEBUG("\tcond vars get the lock when waking up:\n");
    uthread_id_t A;
    uthread_id_t B;
    test_assert(0 == uthread_create(&A, cond_lock_test_A, 0, 0, 3));
    test_assert(0 == uthread_create(&B, cond_lock_test_B, 0, 0, 3));
    test_assert(0 == uthread_join(A, (void **)&exit_value));
    test_assert(0 == uthread_join(B, (void **)&exit_value));

    test_reset();
    /* test that signal wakes up only one thread */
    DEBUG("\tsignal wakes up only one thread\n");
    test_assert(0 == uthread_create(&thr[0], cond_lock_test_incr, 0, 0, UTH_MAXPRIO));
    test_assert(0 == uthread_create(&thr[1], cond_lock_test_incr, 0, 0, UTH_MAXPRIO));
    uthread_yield(); /* yield so both threads waiting on condition */
    test_counter++;
    uthread_cond_signal(&test_cond); /* wake up one */
    uthread_yield(); /* it increments counter */
    test_assert(test_counter == 2, "thread was not woken up from uthread_cond_signal");
    uthread_yield();
    test_assert(test_counter == 2, "only one thread should have been woken up"); /* other hasn't woken, counter hasn't changed */
    uthread_cond_signal(&test_cond);
    uthread_yield();
    test_assert(test_counter == 3, "second thread should have been woken up"); /* second thread has incremented counter */
    uthread_join(thr[0], (void **)&exit_value);
    uthread_join(thr[1], (void **)&exit_value);

    test_reset();
    /* test that broadcast wakes up all threads */
    DEBUG("\tbroadcast wakes up all threads\n");
    test_assert(0 == uthread_create(thr + 0, cond_lock_test_A, 0, 0, 3));
    test_assert(0 == uthread_create(thr + 1, cond_lock_test_B, 0, 0, UTH_MAXPRIO));
    test_assert(0 == uthread_create(thr + 2, cond_lock_test_B, 0, 0, UTH_MAXPRIO));
    test_assert(0 == uthread_join(thr[0], (void **)&exit_value));
    test_assert(0 == uthread_join(thr[1], (void **)&exit_value));
    test_assert(0 == uthread_join(thr[2], (void **)&exit_value));
    test_assert(test_counter == 3, "all threads should have been woken up");

    DEBUG("Finished test_locks_and_conditions()\n");
}

static void
*assert_counter_value(long a0, char *a1[]) {
    test_assert(test_counter == a0, "test_counter is not correct");
    test_counter++;
    return NULL; 
}

/*
 * Same as previous function, now each thread gets a different priority
 */
static void test_priority(){
    DEBUG("Running test_priority()\n");
    uthread_id_t thr[8];
    long num_thr = 8;
    long thread_number, exit_value;

    test_reset();
    DEBUG("\tGeneral scheduling/priority test\n");
    for(thread_number = 0; thread_number < num_thr; thread_number++){
        test_assert(0 == uthread_create(&thr[thread_number],
                    thread_add_ten_to_counter, thread_number, NULL, thread_number%(UTH_MAXPRIO)));
    }
    uthread_setprio(thr[0],UTH_MAXPRIO);
    for (thread_number = 0; thread_number < num_thr; thread_number++){
        exit_value = 0;
        test_assert(0 == uthread_join(thr[thread_number], (void **)&exit_value));
        test_assert(exit_value == thread_number, "return_value not set correctly in uthread_join");
    }
    test_assert(test_counter == 10*num_thr, "all threads created should have modified counter");

    test_reset();
    /* simple test; higher-pri threads should run first, so the higher-pri thread is created second but asserts that the counter hasn't been incremented yet */
    DEBUG("\thigher-pri threads run first\n");
    test_assert(0 == uthread_create(&thr[0], assert_counter_value, 1, 0, 3));
    test_assert(0 == uthread_create(&thr[1], assert_counter_value, 0, 0, 4));
    test_assert(0 == uthread_join(thr[0], (void **)&exit_value));
    test_assert(0 == uthread_join(thr[1], (void **)&exit_value));

    test_reset();
    /* change priority of a thread on the run queue */
    DEBUG("\tchanging priority of thread on the run queue works\n");
    test_assert(0 == uthread_create(&thr[0], assert_counter_value, 1, 0, 3));
    test_assert(0 == uthread_create(&thr[1], assert_counter_value, 0, 0, 2));
    test_assert(0 == uthread_setprio(thr[1], 4));
    test_assert(0 == uthread_join(thr[0], (void **)&exit_value));
    test_assert(0 == uthread_join(thr[1], (void **)&exit_value));

    uthread_yield(); /* let the reaper have some fun */

    test_reset();
    /* change priority of a thread NOT on the run queue */
    DEBUG("\tchanging priority of thread not on the run queue works\n");
    test_assert(0 == uthread_create(&thr[0], setprio_sleeping_thread, 0, 0, UTH_MAXPRIO));
    test_assert(0 == uthread_create(&thr[1], setprio_sleeping_thread, 0, 0, 3));
    test_assert(0 == uthread_setprio(uthread_self(), 4));
    uthread_yield(); /* [0] gets mutex, [1] is blocking for it */
    test_assert(test_counter == 1);
    test_assert(0 == uthread_setprio(thr[1], 5)); /* make [1] go after */
    uthread_yield();
    test_assert(test_counter == 2); /* [1] has changed counter */
    uthread_join(thr[0], (void **)&exit_value);
    uthread_join(thr[1], (void **)&exit_value);
    test_assert(test_counter == 2);

    /* test that setprio(uthread_self()) doesn't put thread back on run queue */
    test_reset();
    DEBUG("\tsetprio(uthread_self()) doesn't put thread on run queue\n");
    test_assert(0 == uthread_setprio(uthread_self(), UTH_MAXPRIO - 1));
    test_assert(0 == uthread_create(thr, assert_counter_value, 1, 0, UTH_MAXPRIO - 1));
    test_counter++;
    uthread_yield();
    test_counter++;
    test_assert(0 == uthread_join(thr[0], (void **)&exit_value));
    test_assert(0 == uthread_setprio(uthread_self(), UTH_MAXPRIO));

    test_reset();
    /* test that setting to the same priority doesn't reschedule */
    DEBUG("\tsetting to same priority doesn't reschedule:\n");
    test_assert(0 == uthread_create(&thr[0], assert_counter_value, 0, 0, UTH_MAXPRIO));
    test_assert(0 == uthread_create(&thr[1], assert_counter_value, 1, 0, UTH_MAXPRIO));
    uthread_setprio(thr[0], UTH_MAXPRIO);
    test_assert(0 == uthread_join(thr[0], (void **)&exit_value));
    test_assert(0 == uthread_join(thr[1], (void **)&exit_value));

    uthread_yield();

    /* thr[0] and thr[1] are now invalid threads, so setprio should fail */
    DEBUG("\tError cases for setprio\n");
    // the thread is in an invalid state
    test_assert(EINVAL == uthread_setprio(thr[0], UTH_MAXPRIO));

    test_assert(ESRCH == uthread_setprio(-1, UTH_MAXPRIO));
    test_assert(ESRCH == uthread_setprio(UTH_MAX_UTHREADS, UTH_MAXPRIO));
    test_assert(EINVAL == uthread_setprio(uthread_self(), -1));
    test_assert(EINVAL == uthread_setprio(uthread_self(), UTH_MAXPRIO + 1));

    DEBUG("Finished test_priority()\n");
}

/*
 * Can't forget about detaching threads. Uses add_ten_to_counter to make
 * sure that they are working since there is no way to wait for them other
 * than a condition or waiting for the counter to increment up to the 
 * expected value.
 * 
 * The main thread has to have a lower priority than the new threads so that 
 * when the main thread yields, other threads will get to go. uthread_switch
 * says its okay to switch from the curthr to curthr if it 
 */
static void test_detached_threads(){
    DEBUG("Running test_detached_threads()\n");
    uthread_id_t thr[8];
    long num_thr = 8;
    long thread_number, exit_value;

    test_reset();
    uthread_setprio(uthread_self(), 0);
    for(thread_number = 0; thread_number < num_thr; thread_number++){
        test_assert(0 == uthread_create(&thr[thread_number],
                    thread_add_ten_to_counter_with_initial_wait, thread_number, NULL, 1));
        uthread_detach(thr[thread_number]);
    }

    /* Tell all the threads to go! */
    test_pause = 0;
    uthread_cond_broadcast(&test_pause_cond);

    for (thread_number = 0; thread_number < num_thr; thread_number++){
        // Either the thread is not joinable (EINVAL), or it has finished
        // executing and has been reaped (ESRCH)
        //test_assert((ESRCH | EINVAL) & uthread_join(thr[thread_number], (void **)&exit_value));
        test_assert(!!uthread_join(thr[thread_number], (void **)&exit_value));
    }

    while(test_counter < 10*num_thr) {
        DEBUG("Waiting for detached threads to finish... (spinning)\n");
        uthread_yield();
    }
    test_assert(test_counter == 10*num_thr);

    /* detach a thread that has already finished */
    DEBUG("\tDetach a thread that has already finished:\n");
    test_assert(0 == uthread_create(thr, thread_dummy_function, 0, 0, UTH_MAXPRIO));
    uthread_yield();
    test_assert(0 == uthread_detach(thr[0]));

    /* detach makes threads reapable */
    DEBUG("\tDetach makes threads reapable:\n");
    long i, j;
    for (i = 0; i < 4; i++) {
        uthread_id_t ids[UTH_MAX_UTHREADS / 2 + 1];
        for (j = 0; j < UTH_MAX_UTHREADS / 2; j++) {
            test_assert(0 == uthread_create(&ids[j], thread_dummy_function, 0, 0, UTH_MAXPRIO));
            test_assert(0 == uthread_detach(ids[j]));
        }
        /* yield to let reaper run.  This is technically a bug,
           hopefully fixed after 2015 */
        uthread_yield();
    }

    uthread_id_t noop;
    /* error cases */
    DEBUG("\tDetach error cases:\n");
    test_error(uthread_detach(-1), ESRCH);
    test_error(uthread_detach(UTH_MAX_UTHREADS + 1), ESRCH);
	/* Create a detached thread */
	test_assert(0 == uthread_create(&noop, &thread_dummy_function, 0, NULL, 0));
	test_assert(0 == uthread_detach(noop));
    /* this should fail because the thread is already detached */
    test_assert(uthread_detach(noop)); 
    test_error(uthread_join(noop, (void **)&exit_value), EINVAL);
    /* test_error(uthread_detach(noop), EINVAL); this is undefined behavior in the man page so it is undefined for us too */

    DEBUG("Finished test_detached_threads()\n");
}

/*
 * Get up, get up, get up, drop the bombshell \
 * Get up, get up, this is outta control \
 * Get up, get up, get up, drop the bombshell \
 * Get up, get up, get gone
 *     - Powerman 5000
 * 
 * This tests everything
 *     - Multiple threads
 *     - Creating more threads than are available
 *     - Joining threads (and erroneous joins)
 *     - Detaching threads
 *     - (Broad)casting signals
 *     - Using mutexes
 *     - Priorties
 */
static void test_thread_bomb(){
    DEBUG("Running test_thread_bomb()\n");
    long i,j, exit_value;
    long num_thr = UTH_MAX_UTHREADS;
    //one init thread, one reaper thread already exist
    long num_thr_avail = UTH_MAX_UTHREADS-2; 
    uthread_id_t thr[UTH_MAX_UTHREADS];
    memset(thr,0,UTH_MAX_UTHREADS * sizeof(uthread_id_t));

    test_reset();
    test_pause = 1;

    /* Create a bunch of threads, detach half of them, check for expected errors */
    for(i = 0; i < num_thr; i++){
        if (i < num_thr_avail) {
            test_assert(0 == uthread_create(&thr[i], thread_add_ten_to_counter_with_initial_wait, i, NULL, 0));
        } else {
            test_error(uthread_create(&thr[i], thread_add_ten_to_counter_with_initial_wait, i, NULL, 0), EAGAIN);
        }

        if (i < num_thr_avail && i % 2 == 0) {
            uthread_detach(thr[i]);
        }
    }

    /* Screw with priorities a lot */
    for (j = 0; j < 10; j++){
        for(i = 0; i < num_thr_avail; i++){
            uthread_setprio(thr[i],(i*j)%(UTH_MAXPRIO+1));
        }
    }
    uthread_setprio(uthread_self(), 0);

    /* Tell all the threads to go! */
    test_pause = 0;
    uthread_cond_broadcast(&test_pause_cond);

    /* Try to join the threads and check for expected return values */
    for (i = 0; i < num_thr; i++){
        exit_value = 0;
        if (i >= num_thr_avail){
            /* If uthread_create failed correctly, it should not have set
             * thr[i], and we will attempt to join with some garbage tid */
            long retval = uthread_join(thr[i], (void **)&exit_value);
            //test_assert((ESRCH | EINVAL) & retval);
            test_assert(!!retval);
            continue;
        }
        if (i % 2 == 1){
            test_assert(0 == uthread_join(thr[i], (void **)&exit_value));
            test_assert(exit_value == i);
        }
        else {
            /* Accept both EINVAL and ESRCH, since it depends on whether the
             * detached thread has exited which errno is appropriate */
            long retval = uthread_join(thr[i], (void **)&exit_value);
            //test_assert((ESRCH | EINVAL) & retval);
            test_assert(!!retval);
        }
    }

    /* Wait for all the threads to finish incrementing */
    while(test_counter < 10*num_thr_avail) {
        DEBUG("Waiting for detached threads to finish... (spinning)\n");
        uthread_yield();
    }
    /* Make sure that the final counter value is as expected (should already have happend) */
    test_assert(test_counter == 10*num_thr_avail);
    DEBUG("Finished test_thread_bomb()\n");
}



/*
 * Test the idea of threads creating their own threads and so on.
 * @see thread_fork_creation()
 */ 
static void test_thread_fork_creation(){
    DEBUG("Running test_thread_fork_creation()\n");
    uthread_id_t thr;
    long exit_value, i;

    test_reset();

    test_assert(0 == uthread_create(&thr, thread_fork_creation, 0, NULL, 0));

    test_assert(0 == uthread_join(thr, (void **)&exit_value));
    test_assert(exit_value == 0, "uthread_join not setting return_value correctly");

    exit_value = 1;
    for (i = 0; i < TEST_NUM_FORKS; i++)
        exit_value = exit_value * 2;

    test_assert(test_counter == exit_value);
    DEBUG("Finished test_thread_fork_creation()\n");
}

static void test_priority_change() {
    DEBUG("Testing priority change\n");

    // Create 2 threads. Thread a will run at a higher priority than thread b.
    // If everything works, thread a will increase thread b's priority and
    // then yield to thread b.
    uthread_id_t a_id, b_id;
    long res;
    const long COUNTER_INCREMENT = 1;

    test_reset();
    test_assert(0 == uthread_create(&b_id, increment_test_counter_by_value,
                                    COUNTER_INCREMENT, NULL, 0));
    test_assert(0 == uthread_create(&a_id, increase_priority_of_other_thread,
                                    b_id, NULL, 1));


    test_assert(0 == uthread_join(a_id, (void **)&res));
    test_assert(res == COUNTER_INCREMENT, "uthread_join not setting return_value correctly");
    test_assert(0 == uthread_join(b_id, (void **)&res));
    test_assert(res == COUNTER_INCREMENT, "uthread_join not setting return_value correctly");
}


/*
 * Dummy function for threads to use. Simply exits. Nothing exciting.
 */
static void
*thread_dummy_function(long a0, char* a1[]){
    (void) a1;
    uthread_exit((void *)a0);
    DEBUG("Thread %i got to end of thread_dummy_function. Should have exited. THIS IS BAD\n", uthread_self());
    return NULL;
}

/*
 * Increments the global counter 10 times by 1. It locks and waits on a 
 * condition for a signal. Simple test program.
 */ 
static void 
*thread_add_ten_to_counter(long a0, char* a1[]){
    (void) a1;
    long i;
    for (i = 0; i < 10; i++){
        uthread_mtx_lock(&test_mtx);
        while(test_counter_guard)
            uthread_cond_wait(&test_cond, &test_mtx);
        test_counter_guard = 1;
        test_counter = test_counter + 1;
        uthread_mtx_unlock(&test_mtx);
        uthread_yield();
        uthread_mtx_lock(&test_mtx);
        test_counter_guard = 0;
        uthread_cond_signal(&test_cond);
        uthread_mtx_unlock(&test_mtx);
        uthread_yield();
    }
    uthread_exit((void *)a0);
    return NULL; 
}

static uthread_id_t me;
static void
*thread_add_ten_yield_mutex(long a0, char *a1[]) {
    (void) a1;
    long i;
    for (i = 0; i < 10; i++) {
        uthread_mtx_lock(&test_mtx);
        volatile long local = test_counter;
        me = uthread_self();
        uthread_yield();
        assert(me == uthread_self());
        assert(test_counter == local);
        test_counter = test_counter + 1;
        uthread_mtx_unlock(&test_mtx);
    }
    uthread_exit((void *)a0);
    return NULL; 
}

/*
   test that mutexes satisfy the FIFO property.
   A should run first, then B.
 */
static void
*mutex_fifo_test_A(long a0, char *a1[]) {
    test_assert(test_counter == 0);
    uthread_mtx_lock(&test_mtx); /* lock then yield so others try to lock */
    uthread_yield();
    uthread_mtx_unlock(&test_mtx); /* unlock so B obtains mutex */
    test_assert(ut_curthr != test_mtx.m_owner, "mutex ownership was not transferred in mtx_unlock");  
    uthread_mtx_lock(&test_mtx); /* should block */
    test_assert(test_counter & 0x2); /* B has incremented already */
    test_counter += 0x1;
    uthread_mtx_unlock(&test_mtx);
    return NULL; 
}

static void
*mutex_fifo_test_B(long a0, char *a1[]) {
    uthread_mtx_lock(&test_mtx);
    test_assert((test_counter & 0x1) == 0); /* A has not incremented yet */
    test_counter += 0x2;
    uthread_mtx_unlock(&test_mtx);
    return NULL; 
}


/*
   test that mutexes satisfy mutex property.
   Implementations satisfying mutex but not FIFO will still pass
   this test.
   A should be put on the run queue before B.
 */
static void
*mutex_exclusion_test_A(long a0, char *a1[]) {
    test_assert(test_counter == 0);
    uthread_mtx_lock(&test_mtx); /* lock then yield so others try to lock */
    uthread_yield();
    uthread_mtx_unlock(&test_mtx); /* unlock so B obtains mutex */
    uthread_mtx_lock(&test_mtx);
    test_counter += 0x1;
    uthread_yield();
    /* either B has done nothing or both operations */
    /* both values satisify exclusion, but only 0xD satisfies FIFO */
    test_assert(test_counter == 0x1 || test_counter == 0xD);
    test_counter += 0x2;
    uthread_mtx_unlock(&test_mtx);
    // added 2023: make sure mutex has transferred ownership
    test_assert(ut_curthr != test_mtx.m_owner, "mutex ownership was not transferred in mtx_unlock"); 
    return NULL; 
}

static void
*mutex_exclusion_test_B(long a0, char *a1[]) {
    uthread_mtx_lock(&test_mtx);
    test_counter += 0x4;
    uthread_yield();
    /* either A has done nothing, or it has done both adds */
    /* both values satisfy exclusion, but only 0x4 satisfies FIFO
       (since B would get the lock first) */
    test_assert(test_counter == 0x4 || test_counter == 0x7);
    test_counter += 0x8;
    uthread_mtx_unlock(&test_mtx);
    // added 2023: make sure mutex has transferred ownership
    test_assert(ut_curthr != test_mtx.m_owner, "mutex ownership was not transferred in mtx_unlock"); 
    return NULL; 
}

static void
*cond_lock_test_A(long a0, char *a1[]) {
    uthread_mtx_lock(&test_mtx);
    test_counter = 1;
    uthread_cond_broadcast(&test_cond);
    uthread_yield();
    test_assert(test_counter == 1);
    uthread_mtx_unlock(&test_mtx);
    return NULL; 
}

static void
*cond_lock_test_B(long a0, char* a1[]) {
    uthread_mtx_lock(&test_mtx);
    while (test_counter == 0) {
        uthread_cond_wait(&test_cond, &test_mtx);
    }
    assert(test_counter >= 1); /* >= depending on test in which it is used */
    test_counter += 1;
    uthread_mtx_unlock(&test_mtx);
    return NULL;
}

static void
*cond_lock_test_incr(long a0, char* a1[]) {
    uthread_mtx_lock(&test_mtx);
    while (test_counter == 0)
        uthread_cond_wait(&test_cond, &test_mtx);
    test_counter += 1;
    uthread_mtx_unlock(&test_mtx);
    return NULL; 
}

/*
 * Increments the global counter 10 times by 1. It locks and waits on a 
 * condition for a signal. Instead of just immediately incrementing
 * like the previous version, this waits for the go-ahead from some other
 * thread. It should be used by test functions so that all threads can be
 * created before any threads finished. Once the threads are created,
 * set the pause guard to 0 and broadcast the pause condition signal
 */ 
static void 
*thread_add_ten_to_counter_with_initial_wait(long a0, char* a1[]){
    (void) a1;
    long i;
    uthread_mtx_lock(&test_pause_mtx);
    while(test_pause)
        uthread_cond_wait(&test_pause_cond, &test_pause_mtx);
    uthread_mtx_unlock(&test_pause_mtx);

    for (i = 0; i < 10; i++){
        uthread_mtx_lock(&test_mtx);
        while(test_counter_guard)
            uthread_cond_wait(&test_cond, &test_mtx);
        test_counter_guard = 1;
        test_counter = test_counter + 1;
        uthread_mtx_unlock(&test_mtx);
        test_counter_guard = 0;
        uthread_cond_signal(&test_cond);
    }
    uthread_exit((void *)a0);
    return NULL; 
}

/*
 * A thread that creates two threads that create two more threads...
 * 
 * Rather than having a single thread create all the threads and wait on all the
 * joins, this function creates two new threads that will use this function and wait
 * for them to return
 * 
 * The maximum depth is TEST_NUM_FORKS (4) (16 total threads)
 * 
 * The leaf threads increment the counter by 1 and then return. The end value
 * of the counter should be TEST_NUM_FORKS^2
 * 
 * a0 is the initial depth (set to 0 for expected results);
 */
static void *thread_fork_creation(long cur_depth, char* a1[]){
    (void) a1;
    uthread_id_t thr1, thr2;
    long exit_value=-1;
    long retval=-1;
    if (cur_depth >= TEST_NUM_FORKS){
        uthread_mtx_lock(&test_mtx);
        test_counter = test_counter + 1;
        uthread_mtx_unlock(&test_mtx);
        uthread_exit((void *)cur_depth);
    }


    retval = uthread_create(&thr1, thread_fork_creation, cur_depth+1, NULL, 0);
    test_assert(retval == 0, "uthread_create failed");
    retval = uthread_create(&thr2, thread_fork_creation, cur_depth+1, NULL, 0);

    retval = uthread_join(thr1, (void **)&exit_value);
    test_assert(exit_value == cur_depth+1, "uthread_join did not set return_value correctly");
    test_assert(retval == 0);

    retval = uthread_join(thr2, (void **)&exit_value);
    test_assert(exit_value == cur_depth+1, "uthread_join did not set return_value correctly");
    test_assert(retval == 0, "uthread_join failed");

    uthread_exit((void *)cur_depth);
    return NULL; 
}


static void
*join_sleeping_test_A(long a0, char* a1[]) {
    uthread_mtx_lock(&test_mtx);
    uthread_yield(); /* let B run */
    uthread_mtx_unlock(&test_mtx);
    return NULL; 
}

static void
*join_sleeping_test_B(long a0, char* a1[]) {
    uthread_mtx_lock(&test_mtx); /* block since A has it */
    test_counter += 1;
    uthread_mtx_unlock(&test_mtx);
    return NULL; 
}

static void
*setprio_sleeping_thread(long a0, char* a1[]) {
    uthread_mtx_lock(&test_mtx);
    test_counter += 1;
    uthread_yield();
    uthread_mtx_unlock(&test_mtx);
    return NULL; 
}

static void
*increment_test_counter_by_value(long value, char* a1[]) {
    test_counter += value;
    uthread_exit((void *)test_counter);
    return NULL; 
}

static void *increase_priority_of_other_thread(long other_thread,
                                              char* a1[]) {
    test_assert(test_counter == 0);
    uthread_setprio(other_thread, UTH_MAXPRIO);

    // We should yield because of this^ call, and b should set the counter
    test_assert(test_counter > 0);
    uthread_exit((void *)test_counter);
    return NULL; 
}
