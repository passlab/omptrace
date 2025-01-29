OMP_INSTALL=/usr/lib/llvm-18
OMP_LIB_PATH=${OMP_INSTALL}/lib
OMPTRACE_LIB=../build/libomptrace.so

default:run

axpy: axpy.c
	clang-18 -g -fopenmp axpy.c -o axpyclang
	objdump -d axpyclang >axpyclang.objdump

jacobi: jacobi.c
	clang-18 -g -fopenmp jacobi.c -o jacobi -lm
	objdump -d jacobi >jacobi.objdump

fib: fib-task.c
	clang-18 -g -fopenmp fib-task.c -o fib
	objdump -d fib >fib.objdump

runaxpy: axpy
	LD_PRELOAD=${OMP_LIB_PATH}/libomp.so:${OMPTRACE_LIB} ./axpyclang 1024	

runjacobi: jacobi
	LD_PRELOAD=${OMP_LIB_PATH}/libomp.so:${OMPTRACE_LIB} ./jacobi

runfib: fib
	LD_PRELOAD=${OMP_LIB_PATH}/libomp.so:${OMPTRACE_LIB} ./fib

clean:
	rm axpyclang axpyclang.objdump core jacobi jacobi.objdump fib fib.objdump
