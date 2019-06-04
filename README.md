
# OMPTrace for tracing and visualizing OpenMP program execution

## Prerequisite
OpenMP implementation that supports OMPT interface, which is the llvm OpenMP runtime (https://git.llvm.org/git/openmp.git/). 
   The LLVM OpenMP runtime can be installed standalone or with llvm/clang compiler. The instructions below are for standalone installation. 
  1. clone the repo and checkout the branch
   
           git clone https://git.llvm.org/git/openmp.git/ llvm-openmp
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
There are two examples so far you can experiment, axpy and jacobi. Check the Makefile to see how they can be compiled and executed with omptrace enabled.

