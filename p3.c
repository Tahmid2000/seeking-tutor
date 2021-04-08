#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>

//locks
pthread_mutex_t helpStudent;

//semaphores
sem_t coordinatorReady;
sem_t tutorReady;
sem_t studentReady;
sem_t modifyChairs;
sem_t modifyTutors;
sem_t modifyQueue;
sem_t modifyPriorities;
sem_t modifyStudentPriority;

//inputs
int students;
int tutors;
int totalChairs;
int help;

//others
int availableChairs;
int availableTutors;
int totalHelp;
int currentlyHelped;
int studentPriority;
typedef struct student
{
    int tid;
    int priority;
} student;
student *queue;
int *priorities;

void *coordinatorThread()
{
    int counter = 0;
    //while all students haven't been helped the designated number of helps
    while (counter < totalHelp)
    {
        sem_wait(&studentReady); //wait for notification from student
        //check if tutor available
        sem_wait(&modifyTutors);
        if (availableTutors == 0)
        { //if no tutors
            sem_post(&modifyTutors);
            //sleep?
            //wait?
        }
        else
        { //if tutor
            //decrease available tutors
            availableTutors--;
            sem_post(&modifyTutors);
            //increase available seats
            sem_wait(&modifyChairs);
            availableChairs++;
            sem_post(&modifyChairs);
            //update priorities data structure with students' priorities
            sem_wait(&modifyPriorities);
            int i;
            int j;
            int startingIndex;
            for (i = 0; i < students; i++)
            {
                if ((queue + i) == NULL)
                    break;
                startingIndex = (queue + i)->priority * students;
                for (j = startingIndex; j < students * ((queue + i)->priority + 1); j++)
                {
                    if ((priorities + j) == NULL)
                    {
                        *(priorities + j) = (queue + i)->tid;
                        break;
                    }
                    if (*(priorities + j) == -1)
                        continue;
                }
            }
            sem_post(&modifyPriorities);
            sem_post(&coordinatorReady); //notify tutor
            counter++;
        }
    }
    pthread_exit(NULL);
}

void *tutorThread()
{
    //while all students haven't been helped the designated number of helps
    while (1)
    {
        if (currentlyHelped == totalHelp)
            break;
        sem_wait(&coordinatorReady); //wait for notification from coordinator
        //pick student from priorities structure
        sem_wait(&modifyPriorities);
        int i = 0;
        while (*(priorities + i) == -1)
            i++;
        int tid = *(priorities + i);
        *(priorities + i) = -1;
        sem_post(&modifyPriorities);
        sem_post(&tutorReady);            //notify student with that id
        pthread_mutex_lock(&helpStudent); //lock teachStudent
        usleep(200);                      //teachStudent function
        printf("Student %d received help from Tutor y.\n", tid);
        pthread_mutex_unlock(&helpStudent); //lock teachStudent//unlock teachStudent
        //increase tutorsAvailable
        sem_wait(&modifyTutors);
        availableTutors++;
        sem_post(&modifyTutors);
        currentlyHelped++;
    }
    pthread_exit(NULL);
}

void *studentThread(void *threadID)
{
    int tid = *((int *)threadID); //get threadID
    //check if there is a chair
    sem_wait(&modifyChairs);
    if (availableChairs == 0)
    { //if no chair
        printf("Student %d found no empty chair. Will try again later", tid);
        sem_post(&modifyChairs);
        usleep(2000); // go back to programming
    }
    else
    {                      //if chair
        availableChairs--; //take chair
        printf("Student %d takes a seat. Empty chairs = %d\n", tid, availableChairs);
        sem_post(&modifyChairs);
        //add/modify student in queue
        sem_wait(&modifyQueue);
        int i;
        for (i = 0; i < students; i++)
        {
            if ((queue + i) == NULL)
            {
                student s = {.tid = tid, .priority = 0};
                *(queue + i) = s;
                break;
            }
            else if ((queue + i)->tid == tid)
            {
                (queue + i)->priority++;
                break;
            }
        }
        sem_post(&modifyQueue);
        sem_post(&studentReady); //alert coordinator
        sem_wait(&tutorReady);   //wait for tutor
    }
    pthread_exit(NULL);
}

void *tutorThreadMaker()
{
    int counter = 0;
    while (counter < tutors)
    {
        pthread_t tutorThreadInitializer;
        if (pthread_create(&tutorThreadInitializer, NULL, (void *)tutorThread, NULL))
        {
            char error_message[30] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
        counter++;
        usleep(100000);
    }
    return 0;
}

void *studentThreadMaker()
{
    int counter = 0;
    while (counter < students)
    {
        pthread_t studentThreadInitializer;
        int *threadID = malloc(sizeof(*threadID));
        *threadID = counter + 1;
        if (pthread_create(&studentThreadInitializer, NULL, (void *)studentThread, threadID))
        {
            char error_message[30] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
        counter++;
        usleep(100000);
    }
    return 0;
}

void csmc()
{
    pthread_t coordinator;
    pthread_t tutorMaker;
    pthread_t studentMaker;
    availableChairs = totalChairs;
    availableTutors = tutors;
    totalHelp = students * help;
    sem_init(&coordinatorReady, 0, 0);
    sem_init(&tutorReady, 0, 0);
    sem_init(&studentReady, 0, 0);
    sem_init(&modifyChairs, 0, 0);
    sem_init(&modifyTutors, 0, 0);
    sem_init(&modifyQueue, 0, 0);
    sem_init(&modifyPriorities, 0, 0);
    pthread_mutex_init(&helpStudent, NULL);
    if (pthread_create(&coordinator, NULL, (void *)coordinatorThread, NULL))
    {
        char error_message[30] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }
    if (pthread_create(&tutorMaker, NULL, (void *)tutorThreadMaker, NULL))
    {
        char error_message[30] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }
    if (pthread_create(&studentMaker, NULL, (void *)studentThreadMaker, NULL))
    {
        char error_message[30] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }
    pthread_join(coordinator, NULL);
    pthread_join(tutorMaker, NULL);
    pthread_join(studentMaker, NULL);
}

int main(int argc, char *argv[])
{
    /* if (argc != 4)
    {
        char error_message[30] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    } */
    students = 10;
    tutors = 3;
    totalChairs = 4;
    help = 5;
    queue = malloc(students * sizeof(*queue));
    priorities = (int *)malloc(students * help * sizeof(int));
    csmc();
}