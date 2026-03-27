/*
 * Operating Systems  (2INC0)  Practical Assignment.
 * Condition Variables Application.
 *
 * STUDENT_NAME_1 (STUDENT_NR_1)
 * STUDENT_NAME_2 (STUDENT_NR_2)
 *
 * Grading:
 * Students who hand in clean code that fully satisfies the minimum requirements will get an 8.
 * Extra steps can lead to higher marks because we want students to take the initiative.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>

#include "prodcons.h"

static ITEM buffer[BUFFER_SIZE];
static int head = 0;
static int tail = 0;
static int b_count = 0;
static int next_to_produce = 0;
static int next_to_consume = 0;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_producers = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cond_consumer = PTHREAD_COND_INITIALIZER;
static void rsleep(int t);		 // already implemented (see below)
static ITEM get_next_item(void); // already implemented (see below)

/* producer thread */
static void *
producer(void *arg)
{
    (void)arg; 
    ITEM item;
    while ((item = get_next_item()) != NROF_ITEMS)
    {
        rsleep(100); 

        pthread_mutex_lock(&mutex);

        // Wait until it's my turn
        while (item != next_to_produce)
        {
            pthread_cond_wait(&cond_producers, &mutex);
        }

        // Wait for space
        while (b_count == BUFFER_SIZE)
        {
            pthread_cond_wait(&cond_producers, &mutex);
        }

        // Critical Section
        buffer[tail] = item;
        tail = (tail + 1) % BUFFER_SIZE;
        b_count++;
        next_to_produce++;

        // Signal and Unlock
        pthread_cond_signal(&cond_consumer);
        pthread_cond_broadcast(&cond_producers);
        pthread_mutex_unlock(&mutex);
    }
    return (NULL);
}

/* consumer thread */
static void *
consumer(void *arg)
{
	while (next_to_consume < NROF_ITEMS /*not all items retrieved from buffer[] */)
	{
		// * get the next item from buffer[]
		// * print the number to stdout
		//
		// follow this pseudocode (according to the ConditionSynchronization lecture):
		//      mutex-lock;
		pthread_mutex_lock(&mutex);

		//      while not condition-for-this-consumer
		//          wait-cv;
		while (b_count == 0)
		{
			pthread_cond_wait(&cond_consumer, &mutex);
		}

		//      critical-section;
		ITEM item = buffer[head];
		head = (head + 1) % BUFFER_SIZE;
		b_count--;
		next_to_consume++;

		printf("%d\n", item);

		//      signal producers that a slot is free
		pthread_cond_broadcast(&cond_producers);

		//      mutex-unlock;
		pthread_mutex_unlock(&mutex);

		rsleep(100); // simulating all kind of activities...
	}
	return (NULL);
}

int main(void)
{
	pthread_t prod_threads[NROF_PRODUCERS];
	pthread_t cons_thread;

	// start the consumer
	pthread_create(&cons_thread, NULL, consumer, NULL);

	// start producers
	int producer_ids[NROF_PRODUCERS];
	for (long i = 0; i < NROF_PRODUCERS; i++)
	{
		producer_ids[i] = i;
		pthread_create(&prod_threads[i], NULL, producer, &producer_ids[i]);
	}

	// wait until all threads are finished
	for (int i = 0; i < NROF_PRODUCERS; i++)
	{
		pthread_join(prod_threads[i], NULL);
	}

	pthread_join(cons_thread, NULL);

	return (0);
}

/*
 * rsleep(int t)
 *
 * The calling thread will be suspended for a random amount of time between 0 and t microseconds
 * At the first call, the random generator is seeded with the current time
 */
static void
rsleep(int t)
{
	static bool first_call = true;

	if (first_call == true)
	{
		srandom(time(NULL));
		first_call = false;
	}
	usleep(random() % t);
}

/*
 * get_next_item()
 *
 * description:
 *	thread-safe function to get a next job to be executed
 *	subsequent calls of get_next_item() yields the values 0..NROF_ITEMS-1
 *	in arbitrary order
 *	return value NROF_ITEMS indicates that all jobs have already been given
 *
 * parameters:
 *	none
 *
 * return value:
 *	0..NROF_ITEMS-1: job number to be executed
 *	NROF_ITEMS:	 ready
 */
static ITEM
get_next_item(void)
{
	static pthread_mutex_t job_mutex = PTHREAD_MUTEX_INITIALIZER;
	static bool jobs[NROF_ITEMS + 1] = {false}; // keep track of issued jobs
	static int counter = 0;						// seq.nr. of job to be handled
	ITEM found;									// item to be returned

	/* avoid deadlock: when all producers are busy but none has the next expected item for the consumer
	 * so requirement for get_next_item: when giving the (i+n)'th item, make sure that item (i) is going to be handled (with n=nrof-producers)
	 */
	pthread_mutex_lock(&job_mutex);

	counter++;
	if (counter > NROF_ITEMS)
	{
		// we're ready
		found = NROF_ITEMS;
	}
	else
	{
		if (counter < NROF_PRODUCERS)
		{
			// for the first n-1 items: any job can be given
			// e.g. "random() % NROF_ITEMS", but here we bias the lower items
			found = (random() % (2 * NROF_PRODUCERS)) % NROF_ITEMS;
		}
		else
		{
			// deadlock-avoidance: item 'counter - NROF_PRODUCERS' must be given now
			found = counter - NROF_PRODUCERS;
			if (jobs[found] == true)
			{
				// already handled, find a random one, with a bias for lower items
				found = (counter + (random() % NROF_PRODUCERS)) % NROF_ITEMS;
			}
		}

		// check if 'found' is really an unhandled item;
		// if not: find another one
		if (jobs[found] == true)
		{
			// already handled, do linear search for the oldest
			found = 0;
			while (jobs[found] == true)
			{
				found++;
			}
		}
	}
	jobs[found] = true;

	pthread_mutex_unlock(&job_mutex);
	return (found);
}
