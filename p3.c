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
pthread_mutex_t helpStudent;
pthread_mutex_t modifyChairs;
pthread_mutex_t modifyQueue;
pthread_mutex_t modifyPriorities;
pthread_mutex_t modifyRequests;
pthread_mutex_t modifyHelped;
pthread_mutex_t modifyStudentAlertCoordinator;
pthread_mutex_t modifyTutorAvailable;
pthread_mutex_t modifyTerminated;

//semaphores
sem_t coordinatorReady;
sem_t tutorReady;
sem_t *studentReady;

//others
int availableChairs;
int totalHelp;
int currentlyHelped;
int requests;
int terminated;
int *studentPriorities;
int *priorities;
double *studentAlertCoordinator;
int *tutorAvailable;
int *tutorAlertStudent;

double now()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void *tutorThread(void *threadID)
{
    int tid = *((int *)threadID); //get threadID
    while (1)
    {
        sem_wait(&tutorReady);
        if (currentlyHelped == totalHelp)
            break;
        int i;
        pthread_mutex_lock(&modifyChairs);
        availableChairs++;
        pthread_mutex_unlock(&modifyChairs);
        pthread_mutex_lock(&modifyPriorities);
        int chosenStudent = -1;
        for (i = 0; i < totalHelp; i++)
        {
            if (priorities[i] >= 0)
            {
                chosenStudent = priorities[i];
                priorities[i] = -2;
                break;
            }
        }
        pthread_mutex_unlock(&modifyPriorities);
        tutorAlertStudent[chosenStudent] = tid;
        sem_post(&studentReady[chosenStudent]);
        usleep(200);
        pthread_mutex_lock(&modifyHelped);
        currentlyHelped++;
        printf("T: Student %d tutored by Tutor %d. Students tutored now = %d. Total sessions tutored = %d\n", chosenStudent, tid, 0, currentlyHelped);
        pthread_mutex_unlock(&modifyHelped);
    }
    pthread_exit(NULL);
}

void *coordinatorThread()
{
    while (1)
    {
        sem_wait(&coordinatorReady);
        int i;
        if (currentlyHelped == totalHelp)
        {
            for (i = 0; i < tutors; i++)
                sem_post(&tutorReady);
            break;
        }
        pthread_mutex_lock(&modifyStudentAlertCoordinator);
        int minTid = 0;
        double minTime = studentAlertCoordinator[0];
        for (i = 1; i < students; i++)
        {
            if (studentAlertCoordinator[i] == -1.0)
                continue;
            if (minTime == -1.0)
            {
                minTid = i;
                minTime = studentAlertCoordinator[i];
            }
            if (studentAlertCoordinator[i] < minTime)
            {
                minTime = studentAlertCoordinator[i];
                minTid = i;
            }
        }
        studentAlertCoordinator[minTid] = -1.0;
        pthread_mutex_unlock(&modifyStudentAlertCoordinator);
        pthread_mutex_lock(&modifyRequests);
        pthread_mutex_lock(&modifyPriorities);
        pthread_mutex_lock(&modifyChairs);
        for (i = studentPriorities[minTid] * students; i < (studentPriorities[minTid] + 1) * students; i++)
        {
            if (priorities[i] == -1)
            {
                priorities[i] = minTid;
                break;
            }
        }
        printf("C: Student %d with priority %d in the queue. Waiting students now = %d. Total requests = %d\n", minTid, studentPriorities[minTid], totalChairs - availableChairs, requests);
        pthread_mutex_unlock(&modifyChairs);
        pthread_mutex_unlock(&modifyPriorities);
        pthread_mutex_unlock(&modifyRequests);
        sem_post(&tutorReady);
    }
    pthread_exit(NULL);
}

void *studentThread(void *threadID)
{
    int tid = *((int *)threadID);
    while (1)
    {
        if (studentPriorities[tid] == help)
        {
            pthread_mutex_lock(&modifyTerminated);
            terminated++;
            if (terminated == students)
            {
                pthread_mutex_unlock(&modifyTerminated);
                sem_post(&coordinatorReady);
                break;
            }
            pthread_mutex_unlock(&modifyTerminated);
            break;
        }
        usleep((float)(rand() % 2001));
        pthread_mutex_lock(&modifyChairs);
        if (availableChairs == 0)
        {
            pthread_mutex_unlock(&modifyChairs);
            printf("S: Student %d found no empty chair. Will try again later.\n", tid);
            continue;
        }
        else
        {
            availableChairs--;
            printf("S: Student %d takes a seat. Empty chairs = %d. Priority = %d.\n", tid, availableChairs, studentPriorities[tid]);
            pthread_mutex_unlock(&modifyChairs);
            pthread_mutex_lock(&modifyStudentAlertCoordinator);
            studentAlertCoordinator[tid] = now();
            pthread_mutex_unlock(&modifyStudentAlertCoordinator);
            pthread_mutex_lock(&modifyRequests);
            requests++;
            pthread_mutex_unlock(&modifyRequests);
            sem_post(&coordinatorReady);
            sem_wait(&studentReady[tid]);
            usleep(200);
            printf("S: Student %d received help from Tutor %d.\n", tid, tutorAlertStudent[tid]);
            studentPriorities[tid]++;
        }
    }
    pthread_exit(NULL);
}

void csmc()
{
    pthread_t coordinator;
    pthread_t tutorThreads[tutors];
    pthread_t studentThreads[students];
    pthread_mutex_init(&helpStudent, NULL);
    pthread_mutex_init(&modifyChairs, NULL);
    pthread_mutex_init(&modifyQueue, NULL);
    pthread_mutex_init(&modifyPriorities, NULL);
    pthread_mutex_init(&modifyRequests, NULL);
    pthread_mutex_init(&modifyHelped, NULL);
    pthread_mutex_init(&modifyStudentAlertCoordinator, NULL);
    pthread_mutex_init(&modifyTerminated, NULL);
    int i;
    for (i = 0; i < students; i++)
        sem_init(&studentReady[i], 0, 0);
    sem_init(&tutorReady, 0, 0);
    sem_init(&coordinatorReady, 0, 0);
    if (pthread_create(&coordinator, NULL, (void *)coordinatorThread, NULL) != 0)
    {
        char error_message[30] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }

    for (i = 0; i < tutors; i++)
    {
        int *tutorID = malloc(sizeof(*tutorID));
        *tutorID = i;
        if (pthread_create(&tutorThreads[i], NULL, (void *)tutorThread, tutorID) != 0)
        {
            char error_message[30] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
    }
    for (i = 0; i < students; i++)
    {
        int *studentID = malloc(sizeof(*studentID));
        *studentID = i;
        if (pthread_create(&studentThreads[i], NULL, (void *)studentThread, studentID) != 0)
        {
            char error_message[30] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
    }
    pthread_join(coordinator, NULL);
    for (i = 0; i < tutors; i++)
        pthread_join(tutorThreads[i], NULL);
    for (i = 0; i < students; i++)
        pthread_join(studentThreads[i], NULL);
    return;
}

int main(int argc, char *argv[])
{
    /* if (argc != 5)
    {
        char error_message[30] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    } */
    students = 5;
    tutors = 1;
    totalChairs = 2;
    help = 2;
    availableChairs = totalChairs;
    totalHelp = students * help;
    currentlyHelped = 0;
    requests = 0;
    terminated = 0;
    studentPriorities = (int *)malloc(students * sizeof(int));
    int i;
    for (i = 0; i < students; i++)
        *(studentPriorities + i) = 0;
    priorities = (int *)malloc(totalHelp * sizeof(int));
    for (i = 0; i < totalHelp; i++)
        priorities[i] = -1;
    studentAlertCoordinator = (double *)malloc(students * sizeof(double));
    for (i = 0; i < students; i++)
        studentAlertCoordinator[i] = -1.0;
    tutorAlertStudent = (int *)malloc(students * sizeof(int));
    for (i = 0; i < students; i++)
        tutorAlertStudent[i] = -1;
    tutorAvailable = (int *)malloc(tutors * sizeof(int));
    studentReady = (sem_t *)malloc(students * sizeof(sem_t));
    time_t t;
    srand((unsigned)time(&t));
    csmc();
    return 0;
}
