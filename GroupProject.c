// Name: Kayla Wilson, Vincenzo Mikelic
// Date: 11-25-2022
// Assignment: Project 3 - Group Project
// Problem description: Learn how parallel programming can be used to take advantage of multicore processing
//                      resources, and how to evaluate performance.
// Input: Program uses command line arguments to specify attributes:
//          -t # specifies a number of computational threads E.G) "./GroupProject -t 4" runs the program with 4 threads.
//          -s # specifies the size of the matrix E.G) "./GroupProject -s 60" creates a 60 x 60 matrix.
//          -v # specifies an initialization value for the matrix E.G) "./GroupProject -v 1" initializes all entries in the matrix to 1
//          -c #,#,#... specifies a list of affinities for threads E.G) "./GroupProject -t 2 -c 3,4" runs the program with two threads, thread 0 has affinity 3, thread 1 has affinity 4 
// Output: Ouputs various program information using printf():
//              The sum of the matrix, CPU time each thread took to compute, wall time it took for the program to execute after matrix initialization.
//              Program attribute information (thread count, matrix size, init value, affinity assignments).
//              Value checks for matrix sum using partition sums and row sums.

// Resources:
//      https://stackoverflow.com/questions/30141229/sum-of-elements-in-a-matrix-using-threads-in-c
//      https://people.cs.rutgers.edu/~pxk/416/notes/c-tutorials/gettime.html
//      https://stackoverflow.com/questions/23550907/no-speedup-for-vector-sums-with-threading

#define _GNU_SOURCE // meeded for affinity

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <math.h>
#include <sched.h>

#define BILLION 1000000000L // used for timing to compute nanoseconds in seconds
#define EPSILON 2.22e-16 // used to check equivalency between floats and doubles

int n;
int c;
int r;

float** data;
double* rowSums;
double* partitionSums;
int numOfaffinity = 0;
int* affinity;
int numCores;

struct timespec* threadTimings;

sem_t* rowSemaphores;

void printMat() // function to print matrix assigned to each thread and print entire matrix
{
    printf("_______________________\n");
    int threadNumber = 0;
    for(int h = 0;h<r;++h)
        for(int k = 0;k<c;++k)
        {
            printf("Matrix assigned to thread %d: \n",threadNumber);
            for(int i = 0;i<n/r;++i)
            {
                for(int j = 0;j<n/c;++j)
                {
                    printf("%7.2f+",data[i+(h*(n/r))][j+(k*(n/c))]);
                }        
                printf("\n");
            }
            printf("_______________________\n");
            ++threadNumber;
        }
    printf("Full matrix:\n");
    for(int i = 0;i<n;++i)
    {
        for(int j = 0;j<n;++j)
        {
            printf("%7.2f+",data[i][j]);
        }        
        printf("\n");
    }
    printf("_______________________\n");
}

// compute sum of matrix partitioned to thread
void* sum(void* p){
    int threadNumber = *((int*) p ); // thread number is passed through thread argument

    cpu_set_t cpuset; // set affinity for thread
    if(numOfaffinity)
    {
        CPU_ZERO(&cpuset);
        CPU_SET(affinity[threadNumber%numOfaffinity], &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    }

    int rowPartition = threadNumber / c; // calculate indices of sub array partition
    int columnPartition = threadNumber % c;

    int rowNumber = rowPartition*(n/r); // calculate index within matrix
    int columnNumber = columnPartition*(n/c);

    float* rowPointer = &data[rowNumber][columnNumber]; // pointer at the start of a row
    float* valPointer; // pointer that goes across row

    for(int i = 0;i<n/r;++i)
    {
        valPointer = rowPointer; // reset to start of row
        sem_wait(&rowSemaphores[rowNumber]); // lock row sum for current row
        for(int j = 0;j<n/c;++j)
        {
            partitionSums[threadNumber] += *valPointer; // calculate partition sum and row sum
            rowSums[rowNumber] += *valPointer;
            ++valPointer;

            int q = 0; // empty operations to exaggerate thread scaling, without this threads provide no benefit (memory bottleneck)
            int howSlow = 100; // increase for more operations and more linear scaling
            for(int z = 0;z<howSlow;++z)
            {
                q *= 1;
            }
        }
        sem_post(&rowSemaphores[rowNumber]); // unlock row sum for current row

        ++rowNumber; // go to next row
        rowPointer = &data[rowNumber][columnNumber];
    }

    clock_gettime(CLOCK_THREAD_CPUTIME_ID,&threadTimings[threadNumber]); // get CPU time spent by thread

    if(numOfaffinity) // confirm affinity with print
    {
        sched_getaffinity(pthread_self(), sizeof(cpu_set_t), &cpuset);
        for(int i = 0;i<numCores;++i)
            if(CPU_ISSET(i, &cpuset) != 0)
                printf("Thread %d has affinity %d set.\n",threadNumber,i);
    }

    pthread_exit(NULL);
}

// calculate most alike factors of thread count
void mostAlikeFactors(int t)
{
    // inefficient algorithm to compute most alike integer factors of thread count
    r = t;
    c = t;
    int mostAlike = t;

    for(int i = t;i>0;--i)
        for(int j = i;j>0;--j)
            if(j*i == t)
                if(i-j < mostAlike)
                {
                    r = i;
                    c = j;
                    mostAlike = i-j;
                }
    if(n % c != 0) // n must be divisible by c with no remainder
    {
        printf("Specified size of array is not divisible evenly with number of column partitions. Exiting.\n");
        exit(EXIT_FAILURE);
    }
    if(n % r != 0) // n must be divisible by r with no remainder
    {
        printf("Specified size of array is not divisible evenly with number of row partitions. Exiting.\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]){
    srand(time(0)); // set seed for different random numbers
    
    char* number;
    int t = 1; // init default arguments
    n = 100;
    float v = -1.0;
    int v_arg_provided = 0;

    int i = 0; // search args for number of threads
    for(;i<argc;++i)
        if((number = strstr(argv[i], "-t")) != NULL)
            break;
    if(i+1 < argc)
    {
        number = argv[i+1];
        if((t = atoi(number)) <= 0)
            t = 1;
    }

    i = 0; // search args for size of array
    for(;i<argc;++i)
        if((number = strstr(argv[i], "-s")) != NULL)
            break;
    if(i+1 < argc)
    {
        number = argv[i+1];
        if((n = atoi(number)) <= 0)
            n = 100;
    }

    i = 0; // search args for init value
    for(;i<argc;++i)
        if((number = strstr(argv[i], "-v")) != NULL)
            break;
    if(i+1 < argc)
    {
        number = argv[i+1];
        if((v = atof(number)) != 0.0)
            v_arg_provided = 1;
        else if(strstr(argv[i+1], "0") != NULL)
            v_arg_provided = 1;
    }

    if(t>n) // thread count shouldnt exceed array size
    {
        printf("Number of computation threads should not exceed array size. Exiting.\n");
        exit(EXIT_FAILURE);        
    }

    affinity = (int*)malloc(t * sizeof(int));

    i = 0; // search args for affinities
    for(;i<argc;++i)
        if((number = strstr(argv[i], "-c")) != NULL)
            break;
    if(i+1 < argc)
    {
        char* affinityStart = argv[i+1];
        char* affinityEnd;
        do
        {
            affinityEnd = strstr(affinityStart, ",");

            if(affinityEnd != NULL)
                *affinityEnd = 0;

            if((affinity[numOfaffinity] = atoi(affinityStart)) > 0)
                ++numOfaffinity;
            else if(strstr(affinityStart, "0") != NULL)
            {
                affinity[numOfaffinity] = 0;
                ++numOfaffinity;
            }
            if(numOfaffinity > t)
            {
                printf("Number of affinities should not exceed thread count. Exiting.\n");
                exit(EXIT_FAILURE);
            }
            if(affinityEnd != NULL)
                ++affinityEnd;
            affinityStart = affinityEnd;
        } while(affinityEnd != NULL);
    }

    numCores = sysconf(_SC_NPROCESSORS_ONLN); // get number of processors for affinity
    for(int i = 0;i<numOfaffinity;++i)
        if (affinity[i]<0 || affinity[i]>=numCores)
        {
            printf("Specified affinity value %d is not valid. Minimum value: 0, Maximum value: %d . Exiting.\n",affinity[i],numCores-1);
            exit(EXIT_FAILURE);                    
        }

    if(numOfaffinity)
        if(t % numOfaffinity != 0)
        {
            printf("Number of affinities is not a multiple of specified thread count. Exiting.\n");
            exit(EXIT_FAILURE);                            
        }

    if(numOfaffinity)
    {
        printf("Affinities set: ");
        for(int i = 0;i<numOfaffinity-1;++i)
            printf("%d, ",affinity[i]);
        printf("%d",affinity[numOfaffinity-1]);
        printf("\n");
    }

    mostAlikeFactors(t);

    printf("Number of computation threads: %d\n",t);
    printf("Size of the array: %d\n",n);
    if(v_arg_provided)
        printf("Array initialization value: %f\n",v);
    else
        printf("Array initialized to random values between 0 and 100.\n");

    pthread_t threadArr[t]; //array of threads to use
    pthread_attr_t attr[t]; // attribute pointer array
    int *p;

    // malloc 2D data array nxn
    data = (float **)malloc(n * sizeof(float*));
    for( int i = 0; i < n; i++){
        data[i] = (float*)malloc(n * sizeof(float));
    }

    // malloc arrays for row sum and sub array sum
    rowSums = (double*)malloc(n * sizeof(double));
    partitionSums = (double*)malloc(t * sizeof(double));

    // malloc array of semaphores for each row
    rowSemaphores = (sem_t*)malloc(n * sizeof(sem_t));

    // malloc arrays for thread timing and main timing
    threadTimings = (struct timespec*)malloc(t * sizeof(struct timespec));
    struct timespec* mainTimings = (struct timespec*)malloc(2 * sizeof(struct timespec));

    for(int i = 0;i<n;++i)
        sem_init(&rowSemaphores[i],0,1); //init and lock row semaphores

    // initializing values to array
    if(v_arg_provided)
        for( int i = 0; i < n; i++)
            for( int j = 0; j < n; j++)
                data[i][j] = v;
    else
        for( int i = 0; i < n; i++)
            for( int j = 0; j < n; j++)
                data[i][j] = (float)rand()/(float)RAND_MAX * 100;

    //printMat();

    clock_gettime(CLOCK_REALTIME, &mainTimings[0]);	/* mark start time */

    fflush(stdout); // Required to schedule thread independently
    for(int i = 0;i<t;++i)
    {
        pthread_attr_init(&attr[i]);
        pthread_attr_setscope(&attr[i], PTHREAD_SCOPE_SYSTEM);
    }

    //creates t threads
    for( int i = 0; i < t; i++){
        p = malloc(sizeof(int));
        *p = i;
        pthread_create(&threadArr[i], &attr[i], sum, p);
    }

    // wait for each thread to complete
    for( int i = 0; i < t; i++)
        pthread_join(threadArr[i], NULL);

    printf("\n");

    double totalRowSum = 0; // compute matrix sum using rows
    for(int i = 0;i<n;++i)
        totalRowSum += rowSums[i];

    double threadPartitionSum = 0; // compute matrix sum using sub arrays
    for(int i = 0;i<n;++i)
        threadPartitionSum += partitionSums[i];

    double slowSum = 0; // compute sum iterating over each element in matrix
    for( int i = 0; i < n; i++)
            for( int j = 0; j < n; j++)
                slowSum += data[i][j];
    
    if(abs(slowSum-totalRowSum) < EPSILON)
        printf("Sum of rows match the overall array sum.\n");
    else
    {
        printf("Sum of rows does not match the overall array sum. An error occured.\n");
        exit(EXIT_FAILURE);
    }

    if(abs(slowSum-threadPartitionSum) < EPSILON)
        printf("Sum of thread partition sums match the overall array sum.\n");
    else
    {
        printf("Sum of thread partition sums does not match the overall array sum. An error occured.\n");
        exit(EXIT_FAILURE);
    }

    printf("Sum of array: %f\n",slowSum);
    printf("\n");

    // thread timing
    unsigned long timing; 
    for(int i = 0;i<t;++i)
    {
        timing = BILLION * (threadTimings[i].tv_sec) + threadTimings[i].tv_nsec;
        printf("CPU time spent in thread %d: %f milliseconds\n",i,((long long unsigned int) timing / 1000000.0));
    }
    printf("\n");

    // clean semaphores
    for(int i = 0;i<n;++i)
        sem_destroy(&rowSemaphores[i]);

    // main timing
	clock_gettime(CLOCK_REALTIME, &mainTimings[1]);	/* mark the end time */
	timing = BILLION * (mainTimings[1].tv_sec - mainTimings[0].tv_sec) + mainTimings[1].tv_nsec - mainTimings[0].tv_nsec;
	printf("Program exiting. Wall clock time spent after array initialization: %f milliseconds\n\n", ((long long unsigned int) timing / 1000000.0));

    exit(EXIT_SUCCESS);
}

