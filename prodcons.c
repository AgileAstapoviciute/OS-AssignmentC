/* 
 * Operating Systems  (2INC0)  Practical Assignment.
 * Condition Variables Application.
 *
 * Cunyuan Liu (2330512)
 * Christina Tsakloglou (2413094)
 * Agilė Astapovičiūtė (1962965)
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

#include "prodcons.h"

static ITEM buffer[BUFFER_SIZE];
static int buffer_count = 0;
static ITEM expected_item = 0;
static pthread_cond_t cv_load = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cv_unload = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void rsleep (int t);	    // already implemented (see below)
static ITEM get_next_item (void);   // already implemented (see below)


/* producer thread */
static void * 
producer (void * arg)
{
	ITEM item;
	int producer_id = *((int *) arg);

	// printf("Producer %d started\n", producer_id);
    while (true /* TODO: not all items produced */)
    {
        // * get the new item
		item = get_next_item();
		if (item == NROF_ITEMS) {
			break;
		}
		
        rsleep (100);	// simulating all kind of activities...
		
	      // * put the item into buffer[]
	//
        // follow this pseudocode (according to the ConditionSynchronization lecture):
        //      mutex-lock;
		pthread_mutex_lock(&mutex);
        //      while not condition-for-this-producer
		while (buffer_count >= BUFFER_SIZE || item != expected_item) {
		//          wait-cv;
			pthread_cond_wait(&cv_load, &mutex);
		}
        //      critical-section;
		buffer[buffer_count++] = item;
		expected_item++;
        //      possible-cv-signals;
		pthread_cond_signal(&cv_unload);
		pthread_cond_broadcast(&cv_load);
        //      mutex-unlock;
		pthread_mutex_unlock(&mutex);
    }
	return (NULL);
}

/* consumer thread */
static void * 
consumer (void * arg)
{
	int i = 0;
	int items_consumed = 0;

    while (items_consumed < NROF_ITEMS /* not all items retrieved from buffer[] */)
    {
	      // * get the next item from buffer[]
	      // * print the number to stdout
        //
        // follow this pseudocode (according to the ConditionSynchronization lecture):
        //      mutex-lock;
		pthread_mutex_lock(&mutex);
        //      while not condition-for-this-consumer
		while (buffer_count <= 0) {
		//          wait-cv;
			pthread_cond_wait(&cv_unload, &mutex);
		}
        //      critical-section;
		for (i = 0; i < buffer_count; i++) {
			printf("%d\n", buffer[i]);
		}
		items_consumed += buffer_count;
		buffer_count = 0;
        //      possible-cv-signals;
		pthread_cond_broadcast(&cv_load);
        //      mutex-unlock;
		pthread_mutex_unlock(&mutex);
        rsleep (100);		// simulating all kind of activities...
    }
	return (NULL);
}

int main (void) 
{
	pthread_t producer_threads[NROF_PRODUCERS];
	pthread_t consumer_thread;
	int producer_ids[NROF_PRODUCERS];

	// * startup the producer threads and the consumer thread
	for (int i = 0; i < NROF_PRODUCERS; i++){
		producer_ids[i] = i;
		pthread_create(&producer_threads[i], NULL, producer, &producer_ids[i]);
	}
	pthread_create(&consumer_thread, NULL, consumer, NULL);

    // * wait until all threads are finished  
    pthread_join(consumer_thread, NULL);
	for (int i = 0; i < NROF_PRODUCERS; i++){
		pthread_join(producer_threads[i], NULL);
	}

    return (0);
}

/*
 * rsleep(int t)
 *
 * The calling thread will be suspended for a random amount of time between 0 and t microseconds
 * At the first call, the random generator is seeded with the current time
 */
static void 
rsleep (int t)
{
    static bool first_call = true;
    
    if (first_call == true)
    {
        srandom (time(NULL));
        first_call = false;
    }
    usleep (random () % t);
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
    static pthread_mutex_t  job_mutex   = PTHREAD_MUTEX_INITIALIZER;
    static bool    jobs[NROF_ITEMS+1] = { false }; // keep track of issued jobs
    static int     counter = 0;    // seq.nr. of job to be handled
    ITEM           found;          // item to be returned
	
	/* avoid deadlock: when all producers are busy but none has the next expected item for the consumer 
	 * so requirement for get_next_item: when giving the (i+n)'th item, make sure that item (i) is going to be handled (with n=nrof-producers)
	 */
	pthread_mutex_lock (&job_mutex);

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
	        found = (random() % (2*NROF_PRODUCERS)) % NROF_ITEMS;
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
			
	pthread_mutex_unlock (&job_mutex);
	return (found);
}



