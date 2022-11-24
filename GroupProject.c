//referenced:
//  https://stackoverflow.com/questions/30141229/sum-of-elements-in-a-matrix-using-threads-in-c

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <string.h>

int n;
int c;
int r;
float** data;
float* rowSums;

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

    double partialArraySum = 0;
    for(int i = 0;i<n/r;++i)
    {
        valPointer = rowPointer;
        for(int j = 0;j<n/c;++j)
        {
            partialArraySum += *valPointer;
            ++valPointer;
        }
        ++rowNumber;
        rowPointer = &data[rowNumber][columnNumber];
    }

    printf("partialArraySum for thread %d: %f\n",threadNumber,partialArraySum);

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

    rowSums = (float*)malloc(n * sizeof(float));
    float *threadTimes = (float*)malloc(n * sizeof(float));
    float totalSum = 0;

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

    printMat();

    //start real time
    struct timespec startTime, endTime, delta;
    clock_gettime(CLOCK_REALTIME, &startTime);

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

    //computing final sum of matrix
    //for( int i = 0; i < n; i++)
        //totalSum += rowSums[i];

    //computing final time
    clock_gettime(CLOCK_REALTIME, &endTime);
    float totalTime = (endTime.tv_sec - startTime.tv_sec) + (endTime.tv_nsec - startTime.tv_nsec);
    printf("Total time: %f\n", totalTime);
}

