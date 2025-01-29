
# OMPTrace for tracing and visualizing OpenMP program execution
A simple tool for tracing and visualizing OpenMP program execution, check [SC18 OpenMP booth talk for an overview](https://www.openmp.org/wp-content/uploads/SC18_OpenMPBoothTalk_YonghongYan.pdf). 

## Features
 1. Trace program execution using OMPT callback, 
 1. Time stamp and calcualte elapsed time for parallel regions, worksharing, etc, 
 1. Collect PAPI counters in each traced event
 1. Output the events and measurement data for a parallel region
 1. Output measurement data (elapsed time, and PAPI counters) in CSV file
 1. Dump all the traces to the graphml file for visualization

## Prerequisite
OpenMP implementation that supports OMPT interface, which is the [LLVM OpenMP runtime](https://github.com/llvm-mirror/openmp.git). 
   The LLVM OpenMP runtime can be installed standalone or with llvm/clang compiler. The instructions below are for standalone installation. 
  1. clone the repo and checkout the branch
   
           git clone https://github.com/llvm-mirror/openmp.git llvm-openmp
           cd llvm-openmp
           
  1. cmake to create the makefile with OMPT_SUPPORT abled, build it and install it
    
           mkdir BUILD
           cd BUILD
           cmake -G "Unix Makefiles" -DLIBOMP_OMPT_SUPPORT=on -DCMAKE_INSTALL_PREFIX=../../llvm-openmp-install ..
           make; make install
           
If you use the OpenMP runtime installed with clang/llvm or from Intel compiler, you need to locate the libomp.so and omp.h/ompt.h and provide the path and filename to omptrace build and program execution. 

## Building omptrace library

    git clone https://github.com/passlab/omptrace.git
    cd omptrace; 

 Modify the CMakeList.txt file to enable and disable certain features and set the variables to point to the right location of OMPT-enabled runtime installation and PAPI. Then build with the following instructions:

    mkdir build; cd build
    cmake ..
    make
    
## Experiment
There are three examples so far you can experiment in [examples](examples) folder, axpy, jacobi and fib-task. Check the Makefile to see how they can be compiled and executed with omptrace enabled. 

### To execute axpy
    cd examples
    make axpy
    make runaxpy

### To execute fib-task
    cd examples 
    make fib
    make runfib

## Visualization
Tracing is writen to a graphml file named OMPTRACE.graphml and you need [yEd](https://www.yworks.com/products/yed) to layout and visualize the trace file. 

## Limitation
The current implementation is simple and all traces of OMPT events are written to memory and then dump to the graphml file. Thus there is limitation of the number of events each thread records and the graphml file could be huge. This is particularly true for parallel iterative program that has some OpenMP parallel regions to be executed thousands of times. Jacobi shows such case, and two OpenMP loops each is executed for 5000 times. The graphml file generated is huge and not renderable.  

## [3-clause BSD License](LICENSE_BSD.txt) and Acknowledgement
Copyright (c) 2016 - 2019 Yonghong Yan and PASSlab (https://passlab.github.io) 
from University of South Carolina. All rights reserved. Funding for this research and 
development was provided by the National Science Foundation 
under award number CISE CCF-1833312, and CISE SHF-1551182. 
