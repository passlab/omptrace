# Set the correct folder name for -I and -L and also export LD_LIBRARY_PATH so the ompt openmp 
# runtime lib are the first in the search path in order for the program to load the library we need
# sudo is tricky and you have to change to root and change LD_LIBRARY_PATH for root
#

OMP_REX_INCLUDE=/home/yan/tools/llvm-openmp/BUILD/runtime/src
OMP_LIB_PATH=/home/yan/tools/llvm-openmp/BUILD/runtime/src

axpyclang:
	clang axpy.c ../lib/cpufreq.c ../lib/pe_measurement.c ../../omptool.c -o axpyclang -fopenmp -L${OMP_LIB_PATH} -I${OMP_REX_INCLUDE} -lomp -lm

matmul: 
	icc matmul.c ../lib/cpufreq.c ../../omptool.c -o matmul -openmp -L/home/liu/LLVM-openmp/BUILD/runtime/src  -I/home/liu/LLVM-openmp/BUILD/runtime/src -lomp -lm -par-affinity=warnings,granularity=thread,proclist=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71],explicit
axpy: 
	icc axpy.c ../lib/cpufreq.c ../../omptool.c -o axpy -openmp -L/home/liu/LLVM-openmp/BUILD/runtime/src  -I/home/liu/LLVM-openmp/BUILD/runtime/src -lomp -lm -par-affinity=warnings,granularity=thread,proclist=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71],explicit

clean:
	rm axpy matmul axpyclang

#matmul: 
#	icc matmul.c ../lib/cpufreq.c -o matmul -openmp -L/home/liu/LLVM-openmp/BUILD/runtime/src  -I/home/liu/LLVM-openmp/BUILD/runtime/src -lomp -lm -par-affinity=warnings,granularity=thread,proclist=[0,36,1,37,2,38,3,39,4,40,5,41,6,42,7,43,8,44,9,45,10,46,11,47,12,48,13,49,14,50,15,51,16,52,17,53,18,54,19,55,20,56,21,57,22,58,23,59,24,60,25,61,26,62,27,63,28,64,29,65,30,66,31,67,32,68,33,69,34,70,35,71],explicit
#axpy: 
#	icc axpy.c ../lib/cpufreq.c -o axpy -openmp -L/home/liu/LLVM-openmp/BUILD/runtime/src  -I/home/liu/LLVM-openmp/BUILD/runtime/src -lomp -lm -par-affinity=warnings,granularity=thread,proclist=[0,36,1,37,2,38,3,39,4,40,5,41,6,42,7,43,8,44,9,45,10,46,11,47,12,48,13,49,14,50,15,51,16,52,17,53,18,54,19,55,20,56,21,57,22,58,23,59,24,60,25,61,26,62,27,63,28,64,29,65,30,66,31,67,32,68,33,69,34,70,35,71],explicit
