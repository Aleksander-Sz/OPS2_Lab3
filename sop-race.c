#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define LEADERBOARD_FILENAME "./leaderboard"
#define LEADERBOARD_ENTRY_LEN 32

#define MIN_TRACK_LEN 16
#define MAX_TRACK_LEN 256
#define MIN_DOG_COUNT 2
#define MAX_DOG_COUNT 6

#define MIN_MOVEMENT 1
#define MAX_MOVEMENT 6
#define MIN_SLEEP 250
#define MAX_SLEEP 1500

#define ERR(source)                                     \
    do                                                  \
    {                                                   \
        fprintf(stderr, "%s:%d\n", __FILE__, __LINE__); \
        perror(source);                                 \
        kill(0, SIGKILL);                               \
        exit(EXIT_FAILURE);                             \
    } while (0)

typedef struct sync_data_t
{
    pthread_barrier_t start_barrier;
    pthread_barrierattr_t start_barrier_attr;
    pid_t* racetrack;
    int racetrack_lenght;
    pthread_mutex_t* racetrack_mutex;
    int* direction;
} sync_data;

void usage(char* program_name)
{
    fprintf(stderr, "Usage: \n");
    fprintf(stderr, "  %s L N\n", program_name);
    fprintf(stderr, "    L - race track length, %d <= L <= %d\n", MIN_TRACK_LEN, MAX_TRACK_LEN);
    fprintf(stderr, "    N - number of dogs, %d <= N <= %d\n", MIN_DOG_COUNT, MAX_DOG_COUNT);
    exit(EXIT_FAILURE);
}

void msleep(unsigned int milli)
{
    time_t sec = (int)(milli / 1000);
    milli = milli - (sec * 1000);
    struct timespec ts = {0};
    ts.tv_sec = sec;
    ts.tv_nsec = milli * 1000000L;
    if (nanosleep(&ts, &ts))
        ERR("nanosleep");
}

void child_work(sync_data* shared_1)
{
    // printf("I am the child %d\n", getpid());
    srand(getpid());
    pthread_barrier_wait(&shared_1->start_barrier);
    printf("%d waf waf (started race)\n", getpid());

    // race
    int movement;
    int position = -1;
    int direction = 1;  // 1 forward, -1 backward
    int my_pid = getpid();
    int L = shared_1->racetrack_lenght;
    pid_t* racetrack = shared_1->racetrack;
    pthread_mutex_t* racetrack_mutex = shared_1->racetrack_mutex;
    while (1)
    {
        msleep(rand() % 1001 + 250);
        movement = rand() % 6 + 1;
        int new_pos = position + (direction * movement);
        if (new_pos >= L)
        {
            // change direction
            // pthread_mutex_lock(&racetrack_mutex[position]);
            // racetrack[position] = 0;
            direction = -1;
            // pthread_mutex_unlock(&racetrack_mutex[position]);
            printf("%d waf waf (changed direction))\n", my_pid);
            return;
        }
        else if (new_pos < 0)
        {
            pthread_mutex_lock(&racetrack_mutex[position]);
            racetrack[position] = 0;
            pthread_mutex_unlock(&racetrack_mutex[position]);
            printf("%d waf waf (finished race))\n", my_pid);
            return;
        }
        else if (racetrack[new_pos] == 0)
        {
            if (position > 0)
            {
                pthread_mutex_lock(&racetrack_mutex[position]);
                racetrack[position] = 0;
                pthread_mutex_unlock(&racetrack_mutex[position]);
            }
            // pthread_mutex_lock(&racetrack_mutex[220]);
            // pthread_mutex_unlock(&racetrack_mutex[220]);
            position = new_pos;
            pthread_mutex_lock(&racetrack_mutex[position]);
            racetrack[position] = my_pid;
            shared_1->direction[position] = direction;
            pthread_mutex_unlock(&racetrack_mutex[position]);
            printf("%d waf waf (new position = %d)\n", my_pid, position);
        }
        else
        {
            printf("%d waf waf (the field is occupied)\n", my_pid);
        }
    }
}

void commentator(sync_data* shared_1)
{
    printf("I am the commentator %d\n", getpid());
    pid_t* racetrack = shared_1->racetrack;
    pthread_mutex_t* racetrack_mutex = shared_1->racetrack_mutex;
    int* direction = shared_1->direction;
    for (int i = 0; i < shared_1->racetrack_lenght; i++)
    {
        pthread_mutex_lock(&racetrack_mutex[i]);
        if (racetrack[i] != 0)
        {
            printf("%d", racetrack[i]);
            if (direction[i] == 1)
            {
                printf("> ");
            }
            else
            {
                printf("< ");
            }
        }
        pthread_mutex_unlock(&racetrack_mutex[i]);
    }
}

void parent_work() { printf("I am the parent %d\n", getpid()); }

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        usage(argv[0]);
        return 0;
    }
    int L = atoi(argv[1]);
    int N = atoi(argv[2]);
    if (L < 16 || L > 256 || N < 2 || N > 6)
    {
        usage(argv[0]);
        return 0;
    }

    // creating the shared memory region:
    sync_data* shared_1 =
        (sync_data*)mmap(NULL, sizeof(sync_data), PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_SHARED, -1, 0);

    pthread_barrierattr_init(&shared_1->start_barrier_attr);
    pthread_barrierattr_setpshared(&shared_1->start_barrier_attr, PTHREAD_PROCESS_SHARED);
    pthread_barrier_init(&shared_1->start_barrier, &shared_1->start_barrier_attr, N);

    shared_1->racetrack = mmap(NULL, sizeof(pid_t) * L, PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    shared_1->racetrack_mutex =
        mmap(NULL, sizeof(pthread_mutex_t) * L, PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    shared_1->direction = mmap(NULL, sizeof(int) * L, PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    for (int i = 0; i < L; i++)
    {
        shared_1->racetrack[i] = 0;
        pthread_mutex_init(&shared_1->racetrack_mutex[i], &attr);
        shared_1->direction[i] = 0;
    }
    pthread_mutexattr_destroy(&attr);
    // pthread_mutex_lock(&shared_1->racetrack_mutex[5]);
    // pthread_mutex_unlock(&shared_1->racetrack_mutex[5]);
    shared_1->racetrack_lenght = L;
    // creating processes:

    int res;
    for (int i = 0; i < N; i++)
    {
        res = fork();
        if (res == -1)
            ERR("Error forking.\n");
        if (res == 0)
        {
            // I am a child
            child_work(shared_1);
            // exit(EXIT_SUCCESS);
            break;
        }
        else
        {
            // I am a parent
            // parent_work();
        }
    }
    if (res > 0)
    {
        // I am a parent
        if (fork() == 0)
        {
            // I am a child
            commentator(shared_1);
        }
        else
        {
            // I am a parent
            ;
            for (int i = 0; i < N + 1; i++)
            {
                wait(NULL);
            }
            // disposing of structures
            printf("Children closed\n");
        }
    }
    for (int i = 0; i < L; i++)
    {
        pthread_mutex_destroy(&shared_1->racetrack_mutex[i]);
    }
    pthread_barrier_destroy(&shared_1->start_barrier);
    pthread_barrierattr_destroy(&shared_1->start_barrier_attr);
    munmap(shared_1->racetrack, sizeof(pid_t) * L);
    munmap(shared_1->racetrack_mutex, sizeof(pthread_mutex_t) * L);
    munmap(shared_1->direction, sizeof(int) * L);
    munmap(shared_1, sizeof(sync_data));
    // msleep(1000);
}
