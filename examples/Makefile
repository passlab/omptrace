OMP_INSTALL=/home/yanyh/tools/llvm-openmp-install
OMP_LIB_PATH=${OMP_INSTALL}/lib
OMPTRACE_LIB=../build/libomptrace.so

default:run

axpyclang: axpy.c
	clang -g -fopenmp axpy.c -o axpyclang
	objdump -d axpyclang >axpyclang.objdump

jacobi: jacobi.c
	clang -g -fopenmp jacobi.c -o jacobi -lm
	objdump -d jacobi >jacobi.objdump

runaxpy: axpyclang
	LD_PRELOAD=${OMP_LIB_PATH}/libomp.so:${OMPTRACE_LIB} ./axpyclang 1024	

runjacobi: jacobi
	LD_PRELOAD=${OMP_LIB_PATH}/libomp.so:${OMPTRACE_LIB} ./jacobi

clean:
	rm axpyclang axpyclang.objdump core jacobi jacobi.objdump
