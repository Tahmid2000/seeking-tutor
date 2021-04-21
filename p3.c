//Please compile with -pthread tag
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

//inputs
int students;
int tutors;
int totalChairs;
int help;

//locks
pthread_mutex_t modifyChairs;
pthread_mutex_t modifyQueue;
pthread_mutex_t modifyRequests;
pthread_mutex_t modifyHelped;
pthread_mutex_t modifyStudentAlertCoordinator;
pthread_mutex_t modifyTerminated;
pthread_mutex_t modifyTutoringNow;

//semaphores
sem_t coordinatorReady;
sem_t tutorReady;
sem_t *studentReady;

//others
int availableChairs;             //number of available chairs out of total chairs
int tutoringNow;                 //number of students being tutored now
int totalHelp;                   //students * help; total amount of help needed by all students
int currentlyHelped;             //number of students have been helped so far
int requests;                    //number of requests from students to coordinator
int terminated;                  //number of students that have recieved all the help they needed
int *studentPriorities;          //data structure that keeps track of the priority of each student
int *queue;                      //data structure of the queue of students waiting to be tutored
double *studentAlertCoordinator; //data structure that keeps track of the time a student alerted coordinator
int *tutoredBy;                  //data structure that keeps track of which tutor tutored a specific student

//this function gets the current time, which will be used to keep track of the time a student alerted a coordinator
double now()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

//tutor thread with a specific threaID
void *tutorThread(void *threadID)
{
    int tid = *((int *)threadID);
    while (1)
    {
        sem_wait(&tutorReady);            //wait for notification from coordinator
        if (currentlyHelped == totalHelp) //if all students have been helped sufficient amount of times, terminate
            break;
        int i;
        pthread_mutex_lock(&modifyQueue);
        int chosenStudent = -1;
        //loop through queue to find student to tutor
        for (i = 0; i < totalHelp; i++)
        {
            if (queue[i] >= 0) //when student is found
            {
                chosenStudent = queue[i];
                //set this spot in the queue to -2 so coordinator doesn't put another student here in the future
                //this is done so the coordinator skips over taken spots in the queue a specific priority level
                queue[i] = -2;
                break;
            }
        }
        //set which tutor the chosen studen was tutored by
        pthread_mutex_unlock(&modifyQueue);
        tutoredBy[chosenStudent] = tid;
        //open up a chair
        pthread_mutex_lock(&modifyChairs);
        availableChairs++;
        pthread_mutex_unlock(&modifyChairs);
        //increase tutoring now and start tutoring
        pthread_mutex_lock(&modifyTutoringNow);
        tutoringNow++;
        pthread_mutex_unlock(&modifyTutoringNow);
        sem_post(&studentReady[chosenStudent]); //alert chosen student to be tutored
        usleep(200);                            //tutor/sleep
        //increase amount helped and decreasing tutoring now
        pthread_mutex_lock(&modifyHelped);
        pthread_mutex_lock(&modifyTutoringNow);
        currentlyHelped++;
        tutoringNow--;
        printf("T: Student %d tutored by Tutor %d. Students tutored now = %d. Total sessions tutored = %d\n", chosenStudent, tid, tutoringNow, currentlyHelped);
        pthread_mutex_unlock(&modifyTutoringNow);
        pthread_mutex_unlock(&modifyHelped);
    }
    pthread_exit(NULL); //terminate thread
}

void *coordinatorThread()
{
    //infinite loop
    while (1)
    {
        sem_wait(&coordinatorReady); //wait for notification from a student thread
        int i;
        if (currentlyHelped == totalHelp) //if all students have been helped sufficient amount of times, terminate
        {
            for (i = 0; i < tutors; i++) //alert all tutors to terminate
                sem_post(&tutorReady);
            break;
        }
        pthread_mutex_lock(&modifyStudentAlertCoordinator);
        //this figures out which student thread alerted the coordinator thread based on time
        //the student thread which alerted the coordinator first, will be put in the queue first,
        //but not necessarily before someone else that alerted the coordinator slightly later, if
        //the later student has higher priority
        //note: adding this timing feature automatically puts students with same priorities in correct positions
        //based on their time alerting the coordinator
        int minTid = 0;                              //arbitrarily choose student with threadID 0 as the one that notified first
        double minTime = studentAlertCoordinator[0]; //get time the student notified
        for (i = 1; i < students; i++)
        {
            if (studentAlertCoordinator[i] == -1.0) //this student thread didn't alert the coordinator
                continue;
            if (minTime == -1.0) //if the current min is set to a thread that didn't alert the coordinator, set it to the thread
            {
                minTid = i;
                minTime = studentAlertCoordinator[i];
            }
            if (studentAlertCoordinator[i] < minTime) //if this thread's alert time is before the current one, set it to the thread
            {
                minTid = i;
                minTime = studentAlertCoordinator[i];
            }
        }
        studentAlertCoordinator[minTid] = -1.0; //after chosing thread, reset its index in the data structure
        pthread_mutex_unlock(&modifyStudentAlertCoordinator);
        pthread_mutex_lock(&modifyRequests);
        pthread_mutex_lock(&modifyQueue);
        pthread_mutex_lock(&modifyChairs);
        //put the chosen student in the queue based on its priority
        //loops through that row of priorities until it finds the first empty spot in the queue (-1)
        //the data structure has help * student spots, so, for example, if the student's priority is
        //2, the loop starts at level 2 of the queue
        for (i = studentPriorities[minTid] * students; i < (studentPriorities[minTid] + 1) * students; i++)
        {
            if (queue[i] == -1) //found an empty spot at this level that hasn't been occupied before
            {
                queue[i] = minTid;
                break;
            }
        }
        requests++;
        printf("C: Student %d with priority %d in the queue. Waiting students now = %d. Total requests = %d\n", minTid, studentPriorities[minTid], totalChairs - availableChairs, requests);
        pthread_mutex_unlock(&modifyChairs);
        pthread_mutex_unlock(&modifyQueue);
        pthread_mutex_unlock(&modifyRequests);
        sem_post(&tutorReady); //notify tutor
    }
    pthread_exit(NULL); //terminate thread
}

//student thread with a specified threadID
void *studentThread(void *threadID)
{
    int tid = *((int *)threadID);
    //infinitely loop
    while (1)
    {
        if (studentPriorities[tid] == help) //if a student has been sufficiently helped, terminate thread
        {
            pthread_mutex_lock(&modifyTerminated);
            terminated++;
            if (terminated == students)
            { //alert coordinator to exit if all student threads have been terminated
                pthread_mutex_unlock(&modifyTerminated);
                sem_post(&coordinatorReady);
                break;
            }
            pthread_mutex_unlock(&modifyTerminated);
            break;
        }
        usleep((float)(rand() % 2001)); //program/sleep for a random amount of time up to 200 ms
        pthread_mutex_lock(&modifyChairs);
        if (availableChairs == 0) //if there aren't any available chairs continue in the loop and program/sleep
        {
            pthread_mutex_unlock(&modifyChairs);
            printf("S: Student %d found no empty chair. Will try again later.\n", tid);
            continue;
        }
        else //chairs available
        {
            availableChairs--; //occupy chair
            printf("S: Student %d takes a seat. Empty chairs = %d. Priority = %d.\n", tid, availableChairs, studentPriorities[tid]);
            pthread_mutex_unlock(&modifyChairs);
            //set the time this student thread alerted the coordinator
            pthread_mutex_lock(&modifyStudentAlertCoordinator);
            studentAlertCoordinator[tid] = now();
            pthread_mutex_unlock(&modifyStudentAlertCoordinator);
            sem_post(&coordinatorReady);  //notify coordinator
            sem_wait(&studentReady[tid]); //wait for a notification from a tutor
            usleep(200);                  //tutored/sleep for .2 ms
            printf("S: Student %d received help from Tutor %d.\n", tid, tutoredBy[tid]);
            //decrease priority of this student, don't need lock for this because no other thread will change the priority of this thread, only read
            studentPriorities[tid]++;
        }
    }
    pthread_exit(NULL); //terminate thread
}

void csmc()
{
    //threads
    pthread_t coordinator;
    pthread_t tutorThreads[tutors];
    pthread_t studentThreads[students];
    //initialize locks
    pthread_mutex_init(&modifyChairs, NULL);
    pthread_mutex_init(&modifyQueue, NULL);
    pthread_mutex_init(&modifyQueue, NULL);
    pthread_mutex_init(&modifyRequests, NULL);
    pthread_mutex_init(&modifyHelped, NULL);
    pthread_mutex_init(&modifyStudentAlertCoordinator, NULL);
    pthread_mutex_init(&modifyTerminated, NULL);
    pthread_mutex_init(&modifyTutoringNow, NULL);
    //initialize semaphores
    int i;
    for (i = 0; i < students; i++)
        sem_init(&studentReady[i], 0, 0);
    sem_init(&tutorReady, 0, 0);
    sem_init(&coordinatorReady, 0, 0);
    //create coordinator thread
    if (pthread_create(&coordinator, NULL, (void *)coordinatorThread, NULL) != 0)
    {
        char error_message[30] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }
    //create n tutor threads that execute the tutorThread() function
    for (i = 0; i < tutors; i++)
    {
        //pass in loop counter as threadID
        int *tutorID = malloc(sizeof(*tutorID));
        *tutorID = i;
        if (pthread_create(&tutorThreads[i], NULL, (void *)tutorThread, tutorID) != 0)
        {
            char error_message[30] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
    }
    //create n student threads that execute the studentThread() function
    for (i = 0; i < students; i++)
    {
        //pass in loop counter as threadID
        int *studentID = malloc(sizeof(*studentID));
        *studentID = i;
        if (pthread_create(&studentThreads[i], NULL, (void *)studentThread, studentID) != 0)
        {
            char error_message[30] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
    }
    //wait for all threads to finish executing
    pthread_join(coordinator, NULL);
    for (i = 0; i < tutors; i++)
        pthread_join(tutorThreads[i], NULL);
    for (i = 0; i < students; i++)
        pthread_join(studentThreads[i], NULL);
    return;
}

int main(int argc, char *argv[])
{
    //make sure correct number of arguments given
    if (argc != 5)
    {
        char error_message[25] = "Insufficient arguments\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }
    //initialize values from input
    students = atoi(argv[1]);
    tutors = atoi(argv[2]);
    totalChairs = atoi(argv[3]);
    help = atoi(argv[4]);
    //check for approrpiate inputs
    if (students == 0 || help == 0)
        exit(1);
    if (tutors == 0)
    {
        char error_message[22] = "Cannot have 0 tutors\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }
    if (totalChairs == 0)
    {
        char error_message[22] = "Cannot have 0 chairs\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }
    //initialize variables
    availableChairs = totalChairs;
    totalHelp = students * help;
    currentlyHelped = 0;
    requests = 0;
    terminated = 0;
    tutoringNow = 0;
    //dynamically allocate data structures and initialize values
    studentPriorities = (int *)malloc(students * sizeof(int));
    int i;
    for (i = 0; i < students; i++)
        *(studentPriorities + i) = 0; //all students start with highest priority
    queue = (int *)malloc(totalHelp * sizeof(int));
    for (i = 0; i < totalHelp; i++)
        queue[i] = -1; //-1 indicates an open spot in the queue
    studentAlertCoordinator = (double *)malloc(students * sizeof(double));
    for (i = 0; i < students; i++)
        studentAlertCoordinator[i] = -1.0; //-1 indicates that the student hasn't notified the coordinator
    tutoredBy = (int *)malloc(students * sizeof(int));
    for (i = 0; i < students; i++)
        tutoredBy[i] = -1; //-1 indicates that a student hasn't been tutored
    studentReady = (sem_t *)malloc(students * sizeof(sem_t));
    time_t t;
    srand((unsigned)time(&t)); //for random numbers
    csmc();                    //start csmc
    return 0;
}
