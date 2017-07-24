cmake_minimum_required(VERSION 3.2)
project(visuomp)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_VERBOSE_MAKEFILE ON)

# Set the correct folder for include and lib path (-I and -L) and also export LD_LIBRARY_PATH so the ompt openmp
# runtime lib are the first in the search path in order for the program to load the library we need
# sudo is tricky and you have to change to root and change LD_LIBRARY_PATH for root
#
# on yan's vm setup
set(OMP_LIB_PATH /home/yanyh/tools/llvm-openmp/BUILD/runtime/src)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g ")

set(SOURCE_FILES
    callback.h
    lttng_tracepoint.h
    visuomp.c
)

add_library(visuomp SHARED ${SOURCE_FILES})
include_directories(. ${OMP_LIB_PATH})
link_directories(${OMP_LIB_PATH} ${PAPI_INSTALL}/lib)
target_link_libraries(visuomp dl lttng-ust ${OMP_LIB_PATH}/libomp.so m)

