#include <stdint.h> // uint32
#include <stdlib.h> // exit, perror
#include <unistd.h> // usleep
#include <stdio.h> // printf
#include <string.h>
#include <pthread.h> // pthread_*
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>

#include "queue.c"



int simulationTime = 120;    // simulation time
int seed = 10;               // seed for randomness
int emergencyFrequency = 40; // frequency of emergency
float p = 0.2;               // probability of a ground job (launch & assembly)

void* LandingJob(void *arg); 
void* LaunchJob(void *arg);
void* EmergencyJob(void *arg); 
void* AssemblyJob(void *arg); 
void* ControlTower(void *arg); 

void* PadA(void *arg); 
void* PadB(void *arg); 

//******************************************************
//Globals
struct Queue *landing_queue;
struct Queue *launching_queue;
struct Queue *assembly_queue;
struct Queue *emergency_queue;

#define MAX_CONSEC_LAUNCH  2
#define MAX_CONSEC_ASSEMBLY  2
#define MAX_WAITING_TIME 5
int n = 10;  // time at which waiting queues are printed (keeping logs part).

int next_id=1;
int t = 2;

FILE *f;
time_t start_time;

pthread_t tid[4003] = {0};
int thread_count=0;

//For control tower to check whether pads are available.
int padA_available=1;
int padB_available=1;

// each queue has a lock to prevent race conditions on that queue
pthread_mutex_t mutex_landing;
pthread_mutex_t mutex_launching;
pthread_mutex_t mutex_assembly;
pthread_mutex_t mutex_emergency;

// mutex, cond. variable pair for the first launches to signal the ATC.
pthread_mutex_t landing_available_mutex;
pthread_mutex_t launching_available_mutex;

pthread_cond_t landing_available;
pthread_cond_t launching_available;

pthread_mutex_t landing_queue_mutex;
pthread_mutex_t launching_queue_mutex;
pthread_mutex_t assembly_queue_mutex;
pthread_mutex_t emergency_queue_mutex;

pthread_mutex_t padA_busy_mutex;
pthread_mutex_t padB_busy_mutex;

pthread_cond_t padA_assigned;
pthread_cond_t padB_assigned;

pthread_mutex_t id_mutex;



//******************************************************

// pthread sleeper function
int pthread_sleep (int seconds)
{
    pthread_mutex_t mutex;
    pthread_cond_t conditionvar;
    struct timespec timetoexpire;
    if(pthread_mutex_init(&mutex,NULL))
    {
        return -1;
    }
    if(pthread_cond_init(&conditionvar,NULL))
    {
        return -1;
    }
    struct timeval tp;
    //When to expire is an absolute time, so get the current time and add it to our delay time
    gettimeofday(&tp, NULL);
    timetoexpire.tv_sec = tp.tv_sec + seconds; timetoexpire.tv_nsec = tp.tv_usec * 1000;
    
    pthread_mutex_lock (&mutex);
    int res =  pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
    pthread_mutex_unlock (&mutex);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&conditionvar);
    
    //Upon successful completion, a value of zero shall be returned
    return res;
}


int main(int argc,char **argv){
    // -p (float) => sets p
    // -t (int) => simulation time in seconds
    // -s (int) => change the random seed
    for(int i=1; i<argc; i++){
        if(!strcmp(argv[i], "-p")) {p = atof(argv[++i]);}
        else if(!strcmp(argv[i], "-t")) {simulationTime = atoi(argv[++i]);}
        else if(!strcmp(argv[i], "-s"))  {seed = atoi(argv[++i]);}
        else if(!strcmp(argv[i], "-n"))  {n = atoi(argv[++i]);}
    }
    
    srand(seed); // feed the seed
    
    /* Queue usage example
        Queue *myQ = ConstructQueue(1000);
        Job j;
        j.ID = myID;
        j.type = 2;
        Enqueue(myQ, j);
        Job ret = Dequeue(myQ);
        DestructQueue(myQ);
    */

    // your code goes here
    
    f=fopen("events.log", "a+");
    fprintf(f, "%-14s%-14s%-22s%-18s%-25s%-13s\n", "EventID", "Status", "Request Time", "End Time", "Turnaround Time", "Pad");
    fprintf(f, "__________________________________________________________________________________________________\n");
    
    //create queues.
    landing_queue = ConstructQueue(1000);
    launching_queue = ConstructQueue(1000);
    assembly_queue = ConstructQueue(1000);
    emergency_queue = ConstructQueue(1000);
    
    start_time = time(NULL);
    
    // Create Control Tower thread
    
    pthread_create(&tid[thread_count++], NULL, ControlTower, NULL);

    // Create initial Launching thread
    
    pthread_create(&tid[thread_count++], NULL, LaunchJob, NULL);
    printf("\nINITIAL LAUNCH JOB ADDED.\n");
    
    // Create Pad A and Pad B threads
    pthread_create(&tid[thread_count++], NULL, PadA, NULL);
    
    pthread_create(&tid[thread_count++], NULL, PadB, NULL);
    
    
    
    while ((time(NULL) % start_time) <= simulationTime) {
    
	    printf("\n\nTime: %ld\n\n", time(NULL) % start_time);
	  
	  
	    //Each t second, create jobs with assigned probabilities.
	    //For each job, create a thread.
	    double randomLanding = (rand() / (double) RAND_MAX);
	    double randomLaunch = (rand() / (double) RAND_MAX);
	    double randomAssembly = (rand() / (double) RAND_MAX);
	    
	    if((time(NULL) % start_time) != 0 && (time(NULL) % start_time) % (40*t) == 0){
	      pthread_create(&tid[thread_count++], NULL, EmergencyJob, NULL);
	      printf("EMERGENCY JOB ADDED.\n");
	    }
	    if (randomLanding <= p) {  	      // landing rocket arrives
	      pthread_create(&tid[thread_count++], NULL, LandingJob, NULL); 
	      printf("LANDING JOB ADDED.\n");
	    }
	    if (randomLaunch <= 1 - p) {	      // launching rocket arrives
	      pthread_create(&tid[thread_count++], NULL, LaunchJob, NULL);
	      printf("LAUNCH JOB ADDED.\n");
	    }
	    if (randomAssembly <= p/2.0) {	      // assembly 
	      pthread_create(&tid[thread_count++], NULL, AssemblyJob, NULL);
	      printf("ASSEMBLY JOB ADDED.\n");
	    }
	    pthread_sleep(t);
	    
	    
	    //Below lines print each waiting job's ID and waiting times in each queue at every iteration for debugging.
	    printf("\nLanding jobs (id, waiting time): ");
	    for(int i=0; i<landing_queue->size; i++){
		    	NODE *node=landing_queue->head;
		    	for(int j=0; j<i; j++){
		    		node=node->prev;
		    	}
		    	Job data=node->data;
		    	printf("(%d, %ld) ", data.ID, (time(NULL) % start_time) - data.request_time);
	    }
	    
	    printf("\nLaunching jobs (id, waiting time): ");
	    for(int i=0; i<launching_queue->size; i++){
		    	NODE *node=launching_queue->head;
		    	for(int j=0; j<i; j++){
		    		node=node->prev;
		    	}
		    	Job data=node->data;
		    	printf("(%d, %ld) ", data.ID, (time(NULL) % start_time) - data.request_time);
		   }
	   
	    printf("\nAssembly jobs (id, waiting time): ");
	    for(int i=0; i<assembly_queue->size; i++){
		    	NODE *node=assembly_queue->head;
		    	for(int j=0; j<i; j++){
		    		node=node->prev;
		    	}
		    	Job data=node->data;
		    	printf("(%d, %ld) ", data.ID, (time(NULL) % start_time) - data.request_time);
	    }
	    
	    printf("\nEmergency jobs (id, waiting time): ");
	    for(int i=0; i<emergency_queue->size; i++){
		    	NODE *node=emergency_queue->head;
		    	for(int j=0; j<i; j++){
		    		node=node->prev;
		    	}
		    	Job data=node->data;
		    	printf("(%d, %ld) ", data.ID, (time(NULL) % start_time) - data.request_time);
	    }
		   
	    
	    //Keeping Logs part
	    if(time(NULL) % start_time == n){
		       printf("\nAt %d sec landing: ", n);
		       for(int i=0; i<landing_queue->size; i++){
		           NODE *node=landing_queue->head;
		           for(int j=0; j<i; j++){
		    		node=node->prev;
		    	    }
		    	    Job data=node->data;
		    	    printf("%d ", data.ID);
		        }
		        printf("\nAt %d sec launch: ", n);
		        for(int i=0; i<launching_queue->size; i++){
		    	    NODE *node=launching_queue->head;
		    	    for(int j=0; j<i; j++){
		    		node=node->prev;
		    	    }
		    	    Job data=node->data;
		    	    printf("%d ", data.ID);
		        }
		    
		        printf("\nAt %d sec assembly: ", n);
		        for(int i=0; i<assembly_queue->size; i++){
		    	    NODE *node=assembly_queue->head;
		    	    for(int j=0; j<i; j++){
		    		node=node->prev;
		    	    }
		    	    Job data=node->data;
		    	    printf("%d ", data.ID);
		        }
		        printf("\n\n");
	    }	    
    	   
    }
    
    //Below lines print waiting ID's at the end of the simulation. 
    printf("\nAt the end, landing: ");
    for(int i=0; i<landing_queue->size; i++){
	NODE *node=landing_queue->head;
	for(int j=0; j<i; j++){
		node=node->prev;
	}
	Job data=node->data;
	printf("%d ", data.ID);
     }
     
     printf("\nAt the end, launch: ");
     for(int i=0; i<launching_queue->size; i++){
	 NODE *node=launching_queue->head;
	 for(int j=0; j<i; j++){
		node=node->prev;
	 }
	 Job data=node->data;
	 printf("%d ", data.ID);
     }
		  
    printf("\nAt the end, assembly: ");
    for(int i=0; i<assembly_queue->size; i++){
    	 NODE *node=assembly_queue->head;
	 for(int j=0; j<i; j++){
		node=node->prev;
	 }
         Job data=node->data;
         printf("%d ", data.ID);
    }
    printf("\n\n");
	
    // no need to wait for threads to finish.
    /*for(int i=0; i<thread_count; ++i){
	 pthread_join(tid[i], NULL);
    }*/
  
 

    return 0;
}

// the function that creates plane threads for landing
void* LandingJob(void *arg){
	
	Job *job = malloc(sizeof(*job));
	
  	pthread_mutex_lock(&id_mutex); //to protect next_id variable.
  	
  	
  	pthread_mutex_lock(&mutex_landing); //to protect queue.
	job->ID = next_id;
	next_id += 1;
	pthread_mutex_unlock(&id_mutex);
	
	job->request_time = time(NULL) % start_time; //keep request time for starvation and keeping log parts.
  	
	Enqueue(landing_queue, *job);
	
	//After enqueue, unlock mutex
	pthread_mutex_unlock(&mutex_landing);

	pthread_exit(0);
}

// the function that creates plane threads for departure
void* LaunchJob(void *arg){
	
	
	Job *job = malloc(sizeof(*job));               
  	
  	pthread_mutex_lock(&id_mutex);
  	
	
	pthread_mutex_lock(&mutex_launching);
	job->ID = next_id;
	next_id += 1;
	pthread_mutex_unlock(&id_mutex);
	
	job->request_time = time(NULL) % start_time;
	
	Enqueue(launching_queue, *job);

	pthread_mutex_unlock(&mutex_launching);
	
	pthread_exit(0);
}

// the function that creates plane threads for emergency landing
void* EmergencyJob(void *arg){
	
	Job *job = malloc(sizeof(*job));
  	pthread_mutex_lock(&id_mutex);
  	
  	
  	pthread_mutex_lock(&mutex_emergency);
	job->ID = next_id;
	next_id += 1;
	pthread_mutex_unlock(&id_mutex);
	
	job->request_time = time(NULL) % start_time;
  	
	Enqueue(emergency_queue, *job);
	
	
	pthread_mutex_unlock(&mutex_emergency);

	pthread_exit(0);
}

// the function that creates plane threads for assembly
void* AssemblyJob(void *arg){
	
	Job *job = malloc(sizeof(*job));
  	pthread_mutex_lock(&id_mutex);
  	
  	pthread_mutex_lock(&mutex_assembly);
	job->ID = next_id;
	next_id += 1;
	pthread_mutex_unlock(&id_mutex);
	
	job->request_time = time(NULL) % start_time;
  	
	Enqueue(assembly_queue, *job);
	
	pthread_mutex_unlock(&mutex_assembly);

	
	pthread_exit(0);
}

// the function that controls the air traffic
void* ControlTower(void *arg){
	
	//constantly check whether a waiting job exists.
	//if there is waiting job, check if pads are available, if available, tell them to work.
	while ((time(NULL) % start_time) < simulationTime) {
		
		if (landing_queue->size > 0 || launching_queue->size > 0 || assembly_queue->size > 0 || emergency_queue->size > 0) {
		
		    	if (padA_available==1)
		    		pthread_cond_signal(&padA_assigned);

			if (padB_available==1) 	
				pthread_cond_signal(&padB_assigned);
	  	} 
	  	
	}

  	pthread_exit(0);
}

void* PadA(void *arg){
	
	
	int launch_consec_count = 0; //for last part of part 2.

	while ((time(NULL) % start_time) < simulationTime) {
		
		
		//wait until Control Tower tells the pad to work.
		pthread_cond_wait(&padA_assigned, &padA_busy_mutex);
		
		//these three locks are for protecting queues (since they are shared variables, and might be used simultaneously) for  if statements and enqueues.
		pthread_mutex_lock(&emergency_queue_mutex);
		pthread_mutex_lock(&landing_queue_mutex);
		pthread_mutex_lock(&launching_queue_mutex);
		
		//firstly check emergency queue, since it has priority.
		if (emergency_queue->size > 0) {
			//unlock unused queue's mutexs.
			pthread_mutex_unlock(&landing_queue_mutex);
			pthread_mutex_unlock(&launching_queue_mutex);
			
			padA_available=0;
		    	
		    	launch_consec_count = 0;
		 	
		 	printf("Pad A -> Emergency started.\n");
		 	
		 	
		    	pthread_sleep(t);
		    	//if job ended before simulation end, dequeue the queue and add data to events.log file.
		    	if((time(NULL) % start_time) <= simulationTime) {
		    		printf("Pad A -> Emergency finished.\n");
		    		Job job = Dequeue(emergency_queue);
		    		fprintf(f, "%-14d%-14c%-22d%-18ld%-25ld%-13c\n", job.ID, 'E', job.request_time, time(NULL) % start_time, time(NULL) % start_time-job.request_time, 'A');
		    	} else {
		    	        printf("\nLanding job with ID %d was ongoing on Pad A when the simulation time is over.\n", emergency_queue->head->data.ID);
		    	}
		    	//after job completed, unlock the mutex.
		    	pthread_mutex_unlock(&emergency_queue_mutex);
	  	} 
		
		//check landing queue, also check whether there are at least 3 launching jobs waiting, and first of them has been waiting for more than max_waiting_time to prevent starvation.
		//after preventing starvation for launching, landing jobs may encounter starvation. To prevent that, give priority to launching in above situation at most MAX_CONSEC_LAUNCH times consecutively, then give priority back to landing.
		else if (landing_queue->size > 0 && 
			 ((!(launching_queue->size >= 3 && (time(NULL) % start_time) - launching_queue->head->data.request_time > MAX_WAITING_TIME))
			 || launch_consec_count > MAX_CONSEC_LAUNCH)  
	           ){
	           
				if (landing_queue->size > 0 && 
	           		(launching_queue->size >= 3 && (time(NULL) % start_time) - launching_queue->head->data.request_time > MAX_WAITING_TIME)
			 ) {
			 	printf("\nEven though there are at least 3 launching job waiting, and first of them is more than %d seconds, since there has been %d consecutive launchings, landing is again given priority on Pad A.", MAX_WAITING_TIME, MAX_CONSEC_LAUNCH);
			 }
				
				pthread_mutex_unlock(&emergency_queue_mutex);
				pthread_mutex_unlock(&launching_queue_mutex);
				
				
				padA_available=0;
			    	
			    	launch_consec_count = 0;
			 	
			 	printf("Pad A -> Landing started.\n");			 	
			 	
			    	pthread_sleep(t);
			    	if((time(NULL) % start_time) <= simulationTime) {
			    		printf("Pad A -> Landing finished.\n");
			    		Job job = Dequeue(landing_queue);
			    		fprintf(f, "%-14d%-14c%-22d%-18ld%-25ld%-13c\n", job.ID, 'L', job.request_time, time(NULL) % start_time, time(NULL) % start_time-job.request_time, 'A');
			    	} else {
		          		printf("\nLanding job with ID %d was ongoing on Pad A when the simulation time is over.\n", landing_queue->head->data.ID);
		    		}
			    	pthread_mutex_unlock(&landing_queue_mutex);
		    	
		    	
	  	} 
	  	else if (launching_queue->size > 0) {
	  		
	  		if (landing_queue->size > 0 && 
			 (launching_queue->size >= 3 && (time(NULL) % start_time) - launching_queue->head->data.request_time > MAX_WAITING_TIME)){
			    printf("\nPad A skipped Landing Job and will do Launching Job since there are at least 3 launch jobs waiting, and first of them has been waiting for more than %d seconds.\n", MAX_WAITING_TIME);
			 }
	  		pthread_mutex_unlock(&landing_queue_mutex);
			pthread_mutex_unlock(&emergency_queue_mutex);
	  		
	  		padA_available=0;
		    	
		    	printf("Pad A -> Launching started.\n");
		    	
		    	
		    	launch_consec_count++;
		    	
		    	pthread_sleep(2*t);
		    	if((time(NULL) % start_time) <= simulationTime) {
		    		printf("Pad A -> Launching finished.\n");
		    		Job job = Dequeue(launching_queue);
		    		fprintf(f, "%-14d%-14c%-22d%-18ld%-25ld%-13c\n", job.ID, 'D', job.request_time, time(NULL) % start_time, time(NULL) % start_time-job.request_time, 'A');
		    	} else {
		          	printf("\nLaunching job with ID %d was ongoing on Pad A when the simulation time is over.\n", launching_queue->head->data.ID);
		    	}
		    	pthread_mutex_unlock(&launching_queue_mutex);
	  	} else {
	  		//if there is no job waiting, unlock all mutexs.
	  		pthread_mutex_unlock(&landing_queue_mutex);
			pthread_mutex_unlock(&emergency_queue_mutex);
	  		pthread_mutex_unlock(&launching_queue_mutex);
	  	}
	  	
	  	padA_available=1;
	  	pthread_mutex_unlock(&padA_busy_mutex);
	  	
	}
  	

  	pthread_exit(0);
}

//same logic as pad A is also valid for pad B, where launching is replaced by assembly.
void* PadB(void *arg){
	
	

	int assembly_consec_count = 0;

	while ((time(NULL) % start_time) < simulationTime) {
		
		
		pthread_cond_wait(&padB_assigned, &padB_busy_mutex);
		pthread_mutex_lock(&emergency_queue_mutex);
		pthread_mutex_lock(&landing_queue_mutex);
		pthread_mutex_lock(&assembly_queue_mutex);
		
		if (emergency_queue->size > 0) {
		
			pthread_mutex_unlock(&landing_queue_mutex);
			pthread_mutex_unlock(&assembly_queue_mutex);
			
			padB_available=0;
		    	
		    	assembly_consec_count = 0;
		 	
		 	printf("Pad B -> Emergency started.\n");
		 	
		 	
		    	pthread_sleep(t);
		    	if((time(NULL) % start_time) <= simulationTime) {
		    		printf("Pad B -> Emergency finished.\n");
		    		Job job = Dequeue(emergency_queue);
		    		fprintf(f, "%-14d%-14c%-22d%-18ld%-25ld%-13c\n", job.ID, 'E', job.request_time, time(NULL) % start_time, time(NULL) % start_time-job.request_time, 'B');
		    	} else {
		          	printf("\nEmergency job with ID %d was ongoing on Pad B when the simulation time is over.\n", emergency_queue->head->data.ID);
		          }
		    	pthread_mutex_unlock(&emergency_queue_mutex);
	  	} 
		
		else if (landing_queue->size > 0 && 
	           ((!(assembly_queue->size >= 3 && (time(NULL) % start_time) - assembly_queue->head->data.request_time > MAX_WAITING_TIME)) 
	           || assembly_consec_count > MAX_CONSEC_ASSEMBLY) 
			 ) {
		
				if (landing_queue->size > 0 && 
	           		(assembly_queue->size >= 3 && (time(NULL) % start_time) - assembly_queue->head->data.request_time > MAX_WAITING_TIME)
			 ) {
			 	printf("\nEven though there are at least 3 assembly job waiting, and first of them is more than %d seconds, since there has been %d consecutive assemblys, landing is again given priority on Pad B.\n",MAX_WAITING_TIME, MAX_CONSEC_ASSEMBLY);
			 }
				padB_available=0;
				
				pthread_mutex_unlock(&emergency_queue_mutex);
				pthread_mutex_unlock(&assembly_queue_mutex);
				
			    	
			    	assembly_consec_count = 0;
			 	
			 	printf("Pad B -> Landing started.\n");
			 	
			 	
			    	pthread_sleep(t);
			    	if((time(NULL) % start_time) <= simulationTime) {
			    		printf("Pad B -> Landing finished.\n");
			    		Job job = Dequeue(landing_queue);
			    		fprintf(f, "%-14d%-14c%-22d%-18ld%-25ld%-13c\n", job.ID, 'L', job.request_time, time(NULL) % start_time, time(NULL) % start_time-job.request_time, 'B');
			    	} else {
		          		printf("\nLanding job with ID %d was ongoing on Pad B when the simulation time is over.\n", landing_queue->head->data.ID);
		    		}
			    	pthread_mutex_unlock(&landing_queue_mutex);
		    	
		    	
	  	} 
	  	else if (assembly_queue->size > 0) {
	  		
	  		if (landing_queue->size > 0 && 
			 (assembly_queue->size >= 3 && (time(NULL) % start_time) - assembly_queue->head->data.request_time > MAX_WAITING_TIME)){
			    printf("\nPad B skipped Landing Job and will do Assembly Job since there are at least 3 assembly jobs waiting, and first of them has been waiting for more than %d seconds.\n", MAX_WAITING_TIME);
			 }
	  		padB_available=0;
	  		
	  		pthread_mutex_unlock(&landing_queue_mutex);
			pthread_mutex_unlock(&emergency_queue_mutex);
	  		
		    	
		    	printf("Pad B -> Assembly started.\n");
		    	
		    	
		    	assembly_consec_count++;
		    	
		    	pthread_sleep(6*t);
		    	if((time(NULL) % start_time) <= simulationTime) {
		    		printf("Pad B -> Assembly finished.\n");
		    		Job job = Dequeue(assembly_queue);
		    		fprintf(f, "%-14d%-14c%-22d%-18ld%-25ld%-13c\n", job.ID, 'A', job.request_time, time(NULL) % start_time, time(NULL) % start_time-job.request_time, 'B');
		    	} else {
		          	printf("\nAssembly job with ID %d was ongoing on Pad B when the simulation time is over.\n", assembly_queue->head->data.ID);
		          }
		    	pthread_mutex_unlock(&assembly_queue_mutex);
	  	} else {
	  		pthread_mutex_unlock(&landing_queue_mutex);
			pthread_mutex_unlock(&emergency_queue_mutex);
	  		pthread_mutex_unlock(&assembly_queue_mutex);
	  	}
	  	
	  	
	  	padB_available=1;
	  	pthread_mutex_unlock(&padB_busy_mutex);
	}
  	

  	pthread_exit(0);
}
