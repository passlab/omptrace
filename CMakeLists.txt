cmake_minimum_required(VERSION 3.2)
project(omptool)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_VERBOSE_MAKEFILE ON)

# set up path
set(OMP_INSTALL /home/yanyh/tools/llvm-openmp-install)
set(OMP_LIB_PATH ${OMP_INSTALL}/lib)
set(OMP_INCLUDE ${OMP_INSTALL}/include)

set(FEATURES "")
set(FEATURES "${FEATURES} -DOMPT_MEASUREMENT_SUPPORT=1")
# For additional measurement with PAPI
#set(FEATURES "${FEATURES} -DPAPI_MEASUREMENT_SUPPORT=1")
#set(PAPI_INSTALL /usr/local)
#set(PAPI_INSTALL /home/yanyh/tools/papi-5.5.1-install)

# For tracing and output
set(FEATURES "${FEATURES} -DOMPT_TRACING_SUPPORT=1")
set(FEATURES "${FEATURES} -DOMPT_TRACING_GRAPHML_DUMP=1")
#set(FEATURES "${FEATURES} -DOMPT_ONLINE_TRACING_PRINT=1")
#set(FEATURES "${FEATURES} -DOMPT_CSV_OUTPUT=1")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FEATURES} -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FEATURES} -g")

set(SOURCE_FILES
    omptool.h
    omptool.c
    ompt_callback.c
)

add_library(omptool SHARED ${SOURCE_FILES})
include_directories(${OMP_INCLUDE})

# If PAPI measurement is set, uncomment the following and specify the correct PAPI path
#include_directories(${PAPI_INSTALL}/include)
#link_directories(${OMP_LIB_PATH} ${PAPI_INSTALL}/lib)
#target_link_libraries(omptool m ${PAPI_INSTALL}/lib/libpapi.so)

#target_link_libraries(omptool unwind m)
