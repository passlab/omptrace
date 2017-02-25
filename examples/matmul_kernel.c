/*
 *  * Square matrix multiplication
 *   * A[N][N] * B[N][N] = C[N][N]
 *    *  * gcc matmul.c cpu_power.c cpu_freq.c -o matmul -lm -fopenmp
 *    	   icc matmul.c cpu_power.c cpu_freq.c -o matmul -lm -openmp
 *     */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/timeb.h>
#include <string.h>
#include "omp.h"

//#define _GNU_SOURCE
#include <utmpx.h>
#include <sched.h>

/* read timer in second */
double read_timer() {
    struct timeb tm;
    ftime(&tm);
    return (double) tm.time + (double) tm.millitm / 1000.0;
}

/* read timer in ms */
double read_timer_ms() {
    struct timeb tm;
    ftime(&tm);
    return (double) tm.time * 1000.0 + (double) tm.millitm;
}


#define REAL float

void init(int N, REAL *A) {
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            A[i*N+j] = (REAL) drand48();
        }
    }
}


void matmul_omp_for(int N, REAL *A, REAL *B, REAL *C) {
    int i,j,k;
    REAL temp;
#pragma omp parallel for shared(N,A,B,C) private(i,j,k,temp)
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            temp = 0;
            for (k = 0; k < N; k++) {
                temp += (A[i * N + k] * B[k * N + j]);
            }
            C[i * N + j] = temp;
        }
    }
}


int main(int argc, char *argv[]) {
    int N;

    int num_threads = 4; /* 4 is default number of threads */
    if (argc < 2) {
        fprintf(stderr, "Usage: axpy <n> (default %d) [<num_threads>] (default %d)\n", N, num_threads);
        exit(1);
    }
    N = atoi(argv[1]);
    if (argc >=3) num_threads = atoi(argv[2]);
    omp_set_num_threads(num_threads);

    double elapsed_omp;

    REAL *A = malloc(sizeof(REAL)*N*N);
    REAL *B = malloc(sizeof(REAL)*N*N);
    REAL *C_omp = malloc(sizeof(REAL)*N*N);

    srand48((1 << 12));
    init(N, A);
    init(N, B);

    int i=0;
    int num_runs = 1;

    float high_power_freq = 1.8;
    float low_power_freq = 1.2;
    //power_capping(200,200);
    //set_cpu_freq_range(0,71,&high_power_freq);
    energy_measure_before();

    elapsed_omp = read_timer();
//    for (i=0; i<num_runs; i++)
//        matmul_omp_for(N, A, B, C_omp);

   // double iterations = log(num_threads)/log(2);
    int id;
    num_threads=4;
#pragma omp parallel num_threads(num_threads) shared(low_power_freq,i) private(id)
{
	    id = sched_getcpu();
    	printf("core id:%d\n",id);
	    matmul_omp_for(N, A, B, C_omp);
}
/*
    num_threads=18;
#pragma omp parallel num_threads(num_threads) shared(low_power_freq,i) private(id)
{
	    matmul_omp_for(N, A, B, C_omp);
}
    num_threads=36;
#pragma omp parallel num_threads(num_threads) shared(low_power_freq,i) private(id)
{
	    matmul_omp_for(N, A, B, C_omp);
}
    num_threads=72;
#pragma omp parallel num_threads(num_threads) shared(low_power_freq,i) private(id)
{
	    matmul_omp_for(N, A, B, C_omp);
}
*/

    elapsed_omp = (read_timer() - elapsed_omp)/num_runs;

    energy_measure_after();


/*    
    double iterations = log(num_threads)/log(2);
    int id_arrays[72];
    int id_index=0;

#pragma omp parallel shared(id_index,id_arrays)
{
    for(i=1;i<iterations;i++)
    {
	omp_set_num_threads(num_threads/i);
	int id = sched_getcpu();
	id_arrays[id_index++] = id;
	#pragma omp barrier
	for(j = 0;j < id_index;j++)
	if(id == id_arrays[j])
	{
	    matmul_omp_for(N, A, B, C_omp);
	}
	else
	{
	    set_cpu_freq(id,&low_power_freq);
	}		
	#pragma omp barrier
    }
}
*/

    /* you should add the call to each function and time the execution */

    printf("======================================================================================================\n");
    printf("\tMatrix Multiplication: A[N][N] * B[N][N] = C[N][N], N=%d\n", N);
    printf("------------------------------------------------------------------------------------------------------\n");
    printf("Performance:\t\tRuntime (ms)\t MFLOPS\n");
    printf("------------------------------------------------------------------------------------------------------\n");
    printf("matmul_omp:\t\t%4f\t%4f\n", elapsed_omp * 1.0e3, ((((2.0 * N) * N) * N) / (1.0e6 * elapsed_omp)));
    return 0;
}



