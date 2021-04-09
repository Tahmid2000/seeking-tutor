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
pthread_mutex_t modifyTutors;
pthread_mutex_t modifyQueue;
pthread_mutex_t modifyPriorities;
pthread_mutex_t modifyRequests;
pthread_mutex_t modifyHelped;
pthread_mutex_t modifyStudentAlertCoordinator;

//semaphores
sem_t coordinatorReady;
sem_t tutorReady;
sem_t studentReady;

//others
int availableChairs;
int availableTutors;
int totalHelp;
int currentlyHelped;
int requests;
int *studentPriorities;
int *priorities;
double *studentAlertCoordinator;

double now()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void *tutorThread(void *threadID)
{
    int tid = *((int *)threadID); //get threadID
    //printf("Tutor thread %d\n", tid);
    pthread_exit(NULL);
}

void *coordinatorThread()
{
    //printf("Coordinator thread\n");
    while (1)
    {
        if (currentlyHelped == totalHelp)
            break;
        sem_wait(&studentReady);
        pthread_mutex_lock(&modifyChairs);
        availableChairs++;
        pthread_mutex_unlock(&modifyChairs);
        currentlyHelped++;
        pthread_mutex_lock(&modifyStudentAlertCoordinator);
        int minTid = 0;
        double minTime = studentAlertCoordinator[0];
        int i;
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
        pthread_mutex_lock(&modifyRequests);
        printf("C: Student %d with priority %d in the queue. Waiting students now = %d. Total requests = %d\n", minTid, studentPriorities[minTid], totalChairs - availableChairs, requests);
        pthread_mutex_unlock(&modifyRequests);
        pthread_mutex_unlock(&modifyStudentAlertCoordinator);
        sem_post(&coordinatorReady);
        printf("C: Serving. Chairs available: %d. Helped so far %d.\n", availableChairs, currentlyHelped);
        usleep(200);
    }
    pthread_exit(NULL);
}

void *studentThread(void *threadID)
{
    int tid = *((int *)threadID); //get threadID
    //printf("Student thread %d\n", tid);
    float program;
    while (1)
    {
        if (studentPriorities[tid] == help)
            break;
        program = (float)(rand() % 201) / 100;
        usleep(program * 1000); //check if this is in ms or micro
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
            sem_post(&studentReady);
            pthread_mutex_lock(&modifyStudentAlertCoordinator);
            studentAlertCoordinator[tid] = now();
            pthread_mutex_unlock(&modifyStudentAlertCoordinator);
            pthread_mutex_lock(&modifyRequests);
            requests++;
            pthread_mutex_unlock(&modifyRequests);
            sem_wait(&coordinatorReady);
            printf("S: Student %d is being served\n", tid);
            usleep(200);
            pthread_mutex_lock(&modifyPriorities);
            studentPriorities[tid]++;
            pthread_mutex_unlock(&modifyPriorities);
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
    pthread_mutex_init(&modifyTutors, NULL);
    pthread_mutex_init(&modifyQueue, NULL);
    pthread_mutex_init(&modifyPriorities, NULL);
    pthread_mutex_init(&modifyRequests, NULL);
    pthread_mutex_init(&modifyHelped, NULL);
    pthread_mutex_init(&modifyStudentAlertCoordinator, NULL);
    int i;
    /* for (i = 0; i < students; i++)
        sem_init(&studentReady[i], 0, 0);
    for (i = 0; i < tutors; i++)
        sem_init(&tutorReady[i], 0, 0); */
    sem_init(&studentReady, 0, 0);
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
}

int main(int argc, char *argv[])
{
    /* if (argc != 5)
    {
        char error_message[30] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    } */
    students = 10;
    tutors = 3;
    totalChairs = 4;
    help = 5;
    availableChairs = totalChairs;
    availableTutors = tutors;
    totalHelp = students * help;
    currentlyHelped = 0;
    requests = 0;
    studentPriorities = (int *)malloc(students * sizeof(int));
    int i;
    for (i = 0; i < students; i++)
        *(studentPriorities + i) = 0;
    priorities = (int *)malloc(students * help * sizeof(int));
    studentAlertCoordinator = (double *)malloc(students * sizeof(double));
    for (i = 0; i < students; i++)
        studentAlertCoordinator[i] = -1.0;
    /* studentReady = (sem_t *)malloc(students * sizeof(sem_t));
    tutorReady = (sem_t *)malloc(tutors * sizeof(sem_t)); */
    csmc();
}
