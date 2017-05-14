cmake_minimum_required(VERSION 3.2)
project(omptool)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_VERBOSE_MAKEFILE ON)

# Set the correct folder for include and lib path (-I and -L) and also export LD_LIBRARY_PATH so the ompt openmp
# runtime lib are the first in the search path in order for the program to load the library we need
# sudo is tricky and you have to change to root and change LD_LIBRARY_PATH for root
#
# Use -DPE_MEASUREMENT_SUPPORT=1 as compiler flag to turn on/off power/energy msr-based measurement, see ompt_callback.c

# set up path
set(OMP_STANDALONE_INSTALL /home/yan/tools/llvm-openmp/BUILD/runtime/src)
set(OMP_LIB_PATH ${OMP_STANDALONE_INSTALL})
set(OMP_INCLUDE ${OMP_STANDALONE_INSTALL})
set(PAPI_INSTALL /home/yan/tools/papi-5.5.1-install)

# For tracing
#define OMPT_TRACING_SUPPORT 1
#define OMPT_ONLINE_TRACING_PRINT 1

# For additional measurement
#define OMPT_MEASUREMENT_SUPPORT 1
#define PAPI_MEASUREMENT_SUPPORT 1
#define PE_MEASUREMENT_SUPPORT 1

# For optimization
#define PE_OPTIMIZATION_SUPPORT 1
#define PE_OPTIMIZATION_DVFS 1

set(FEATURES "")
set(FEATURES "${FEATURES} -DOMPT_TRACING_SUPPORT=1")
set(FEATURES "${FEATURES} -DOMPT_ONLINE_TRACING_PRINT=1")
set(FEATURES "${FEATURES} -DOMPT_MEASUREMENT_SUPPORT=1")
set(FEATURES "${FEATURES} -DPAPI_MEASUREMENT_SUPPORT=1")
set(FEATURES "${FEATURES} -DPE_MEASUREMENT_SUPPORT=1")
# set(FEATURES "${FEATURES} -DPE_OPTIMIZATION_SUPPORT=1")
# set(FEATURES "${FEATURES} -DPE_OPTIMIZATION_DVFS=1")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FEATURES}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FEATURES}")

set(SOURCE_FILES
    omptool.h
    omptool.c
    cpufreq.h
    cpufreq.c
    ompt_callback.c
    pe_measurement.c
)

add_library(omptool SHARED ${SOURCE_FILES})
include_directories(${PAPI_INSTALL}/include)
include_directories(${OMP_INCLUDE})
link_directories(${OMP_LIB_PATH} ${PAPI_INSTALL}/lib)
# target_link_libraries(omptool unwind m ${PAPI_INSTALL}/lib/libpapi.so)
target_link_libraries(omptool m ${PAPI_INSTALL}/lib/libpapi.so)
