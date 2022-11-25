//referenced:
//  https://stackoverflow.com/questions/30141229/sum-of-elements-in-a-matrix-using-threads-in-c

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>

int n;
int c;
int r;
float** data;
double* rowSums;
double* partitionSums;
struct timespec* threadTimings;
sem_t* rowSemaphores;

void printMat() // function to print matrix assigned to each thread
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

//compute sum of matrix partitioned to thread
void* sum(void* p){
    int threadNumber = *((int*) p );
    int rowPartition = threadNumber / c;
    int columnPartition = threadNumber % c;

    int rowNumber = rowPartition*(n/r);
    int columnNumber = columnPartition*(n/c);

    float* rowPointer = &data[rowNumber][columnNumber];
    float* valPointer;

    for(int i = 0;i<n/r;++i)
    {
        valPointer = rowPointer;
        sem_wait(&rowSemaphores[rowNumber]);
        for(int j = 0;j<n/c;++j)
        {
            partitionSums[threadNumber] += *valPointer;
            rowSums[rowNumber] += *valPointer;
            ++valPointer;
        }
        sem_post(&rowSemaphores[rowNumber]);
        ++rowNumber;
        rowPointer = &data[rowNumber][columnNumber];
    }

    printf("partitionSum for thread %d: %f\n",threadNumber,partitionSums[threadNumber]);

    clock_gettime(CLOCK_THREAD_CPUTIME_ID,&threadTimings[threadNumber]);
    printf("timing for thread %d in milliseconds: %f\n",threadNumber,((threadTimings[threadNumber].tv_nsec) / 1000000.0));

    pthread_exit(NULL);
}

//calculate most alike factors of thread count
void mostAlikeFactors(int t)
{
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
    if(n % c != 0)
    {
        printf("Specified size of array is not divisible evenly with number of column partitions. Exiting.\n");
        exit(EXIT_FAILURE);
    }
    if(n % r != 0)
    {
        printf("Specified size of array is not divisible evenly with number of row partitions. Exiting.\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]){
    char* number;
    int t = 1;
    n = 100;
    float v = -1.0;
    int v_arg_provided = 0;

    int testing = 1; // fill matrix with incrementing values for testing

    int i = 0; // search args for number of threads (args must be in quotes for now e.g "-t #")
    for(;i<argc;++i)
        if((number = strstr(argv[i], "-t ")) != NULL)
            break;
    if(i != argc)
    {
        number += 3;
        if((t = atoi(number)) <= 0)
            t = 1;
    }

    i = 0; // search args for size of array (args must be in quotes for now e.g "-s #")
    for(;i<argc;++i)
        if((number = strstr(argv[i], "-s ")) != NULL)
            break;
    if(i != argc)
    {
        number += 3;
        if((n = atoi(number)) <= 0)
            n = 100;
    }

    i = 0; // search args for init value (args must be in quotes for now e.g "-v #")
    for(;i<argc;++i)
        if((number = strstr(argv[i], "-v ")) != NULL)
            break;
    if(i != argc)
    {
        number += 3;
        if((v = atof(number)) != 0.0)
            v_arg_provided = 1;
        else if(strstr(argv[i], "0") != NULL)
            v_arg_provided = 1;
    }

    if(t>n)
    {
        printf("Number of computation threads should not exceed array size. Exiting.\n");
        exit(EXIT_FAILURE);        
    }

    mostAlikeFactors(t);

    pthread_t threadArr[t]; //array of threads to use
    int *p;

    //malloc 2D data array nxn
    data = (float **)malloc(n * sizeof(float*));
    for( int i = 0; i < n; i++){
        data[i] = (float*)malloc(n * sizeof(float));
    }

    rowSums = (double*)malloc(n * sizeof(double));
    partitionSums = (double*)malloc(t * sizeof(double));

    rowSemaphores = (sem_t*)malloc(n * sizeof(sem_t));

    threadTimings = (struct timespec*)malloc(t * sizeof(struct timespec));

    for(int i = 0;i<n;++i)
        sem_init(&rowSemaphores[i],0,1); //init and lock row semaphores

    //initializing values to array
    if(v_arg_provided)
        for( int i = 0; i < n; i++)
            for( int j = 0; j < n; j++)
                data[i][j] = v;
    else
        for( int i = 0; i < n; i++)
            for( int j = 0; j < n; j++)
                data[i][j] = (float)rand()/(float)RAND_MAX * 100;

    if(testing == 1)
    {
        float fill = 1;
        for( int i = 0; i < n; i++)
            for( int j = 0; j < n; j++)
            {
                data[i][j] = fill;        
                ++fill;
            }
    }

    //printMat();

    //creates t threads
    for( int i = 0; i < t; i++){
        p = malloc(sizeof(int));
        *p = i;
        pthread_create(&threadArr[i], NULL, sum, p);
    }

    // wait for each thread to complete
    for( int i = 0; i < t; i++)
        pthread_join(threadArr[i], NULL);

    printf("\n");

    for(int i = 0;i<n;++i)
        printf("row sum for row %d : %f\n",i,rowSums[i]);

    double totalsum = 0;

    for(int i = 0;i<n;++i)
        totalsum += rowSums[i];
    
    printf("total sum: %f\n",totalsum);

    for(int i = 0;i<n;++i)
        sem_destroy(&rowSemaphores[i]);
}

