
#include <stdio.h>
#include <omp.h>
//#include "callback.h"

int main(int argc, char* argv[])
 {
   #pragma omp parallel  
   {
	int id = omp_get_thread_num();
   	printf("Hello, world %d.\n", id);
   }

/* 
   printf("-------------------------------artifical barrier------------------------\n");   
   sleep(2);
   #pragma omp parallel  
   {
	int id = omp_get_thread_num();
   	printf("Before Barrier: Hello, world %d.\n", id);
   #pragma omp barrier
   	printf("After Barrier: Hello, world %d.\n", id);
   }
   int i;
   int N=100;
   int A[N];
   #pragma omp parallel for
   for (i=0; i<N; i++) {
       A[i] = 8*i;

   }
*/
 }

