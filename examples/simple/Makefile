LLVM_OPENMP_SRC=/home/yanyh/tools/llvm-openmp
LLVM_OPENMP_BUILD=/home/yanyh/tools/llvm-openmp/BUILD

# This is where the testing callback.h file is
CALL_BACK_TEST_HEADER=${LLVM_OPENMP_SRC}/runtime/test/ompt

default:
	clang -shared -fopenmp -fPIC -I${LLVM_OPENMP_BUILD}/runtime/src -I${CALL_BACK_TEST_HEADER} empty.c -o libompt.so
	clang -fopenmp helloomp.c -o helloomp

run: default
	LD_PRELOAD=./libompt.so ./helloomp

clean:
	rm libompt.so helloomp
