COMP 304 - Project 2 Report


Part I:

After parsing the arguments in main method, the code part that we have implemented initializes the simulation time, creates the initial threads and queues that will stores the jobs via categorizing.
Thereafter a loop that generates jobs executes till the simulation time ends. Which kind of job is going to be created is decided randomly with respect to corresponding probabilities.

We have 1 thread for control tower, 1 thread for pad A, 1 thread for pad B, and 1 thread for each job created.
Job threads enqueues the job to the queues, control tower checks whether there is any job waiting and if there is any, it checks if pads are available, if available tells the available pad(s) to start a job.
Pad threads check the queues according to priority (first landing), then completes the first job in the corresponding queue, and dequeue that queue.

Since we have some variables that are accessed by multiple threads (like queues, job id etc.), we have implemented mutexes to prevent race conditions.


Part II:

Since landing jobs are given priority, launching and assembly jobs may face starvation if landing jobs never ends (news arrive constantly).
In order to solve starvation, we have added a maximum wait-time and counter for the jobs on the ground to avoid this situation as stated in pdf.
If there are 3 or launching jobs are waiting, and first of them has been waiting for more than a predefined time, priority is given to that job in pad A.
Same applies for assembly jobs on pad B.

Above solution may cause starvation for landing jobs if there are a lot of jobs waiting on the launching and assembly queues. Giving them priority might make landing jobs face starvation.
We have solved the this starvation by limiting the consecutive same jobs. If launching jobs are given priority more than 2 times consecutively in above situation, we give the priority back to landing.


Part III:

Similar to solution of part I, for emergency situations new emergency queue is implemented and jobs that are in that queue have priority to jobs that are in other queues.
On the pad threads, we checked queue of emergency jobs firstly to provide the priority.


Keeping Logs:

We keep the request time of the jobs on the struct, then used for recording request and turnaround times.
After each job completed in pads, we recorded its info on events.log file.

Note: We added several print statements to see what is happening during the simulation, so you might need to scroll up to see the waiting jobs on time n (last part of Keeping Logs part).


We believe that each part works properly.

Group Members:

İsmail Ozan Kayacan - 69103
Lütfi Furkan Karagöz - 69069