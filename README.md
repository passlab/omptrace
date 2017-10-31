
# OMPTool for performance measurement, tracing and auto-steering support

## Prerequisite and Installation
1. OpenMP implementation that supports OMPT interface, which is the towards_tr4 branch of https://github.com/OpenMPToolsInterface/LLVM-openmp/tree/towards_tr4
    
### Installation of the the towards_tr4 branch, which support OMPT, either as standalone or with llvm/clang compiler. 
  1. clone the repo and checkout the branch (standalone or in the llvm/clang source tree)
   
           git clone https://github.com/OpenMPToolsInterface/LLVM-openmp openmp
           cd openmp
           git remote update
           git checkout -t origin/towards_tr4
           
  1. cmake to create the makefile with OMPT_SUPPORT abled, build it and install it
    
           mkdir BUILD
           cd BUILD
           cmake -G "Unix Makefiles" -DLIBOMP_OMPT_SUPPORT=on -DCMAKE_INSTALL_PREFIX=<install_path> ..
           make; make install
           
      For using other compiler (on fornax), CC and CXX should be set for cmake. For example, on fornax as standalone: 
      
           CC=/opt/gcc-5.3.0-install/bin/gcc CXX=/opt/gcc-5.3.0-install/bin/g++ cmake -DLIBOMP_OMPT_SUPPORT=on -DCMAKE_INSTALL_PREFIX=<install_path> ..
           
  1. location for header files (omp.h and ompt.h) and libomp.so library are `<install_path>/include` and `<install_path>/lib` if the runtime is installed standalone. If it is installed as part of clang/llvm, the header location is `<install_path>/lib/clang/5.0.0/include`, and the libomp.so is from `<install_path>/lib`. Setup the library path for execution by letting LD_LIBRARY_PATH env include the lib path. For development and compiling, you need to provide the header path and lib path to the -I and -L flags of the compiler.

## Building omptool

### On fornax:
    git clone this repo
    cd omptool; 
 Modify the CMakeList.txt file to let the following variables point to the right location of OMPT-enabled runtime installation and PAPI. See below for the setting on fornax: 
~~~~
    set(OMP_STANDALONE_INSTALL /home/yan/tools/llvm-openmp/BUILD/runtime/src)
    set(OMP_LIB_PATH ${OMP_STANDALONE_INSTALL})
    set(OMP_INCLUDE ${OMP_STANDALONE_INSTALL})
    set(PAPI_INSTALL /usr/local)
~~~~

    mkdir build; cd build
    CC=/opt/gcc-5.3.0-install/bin/gcc CXX=/opt/gcc-5.3.0-install/bin/g++ cmake ..
    make
    
## Experiment
An application (either in omptool/examples or others such as Lulesh or SPECOMP) needs to be built with either icc or clang in order to use the LLVM OpenMP runtime. In our case, it is the OMPT-enabled LLVM OpenMP runtime. For example to try the omptools/examples/axpy.c

    cd omptools/examples
    clang -fopenmp axpy.c -o axpyclang  # or icc -fopenmp axpy.c -o axpyicc
    export LD_LIBRARY_PATH=/home/yan/tools/llvm-openmp/BUILD/runtime/src:$LD_LIBRARY_PATH
    LD_PRELOAD=/home/yan/tools/omptool/build/libomptool.so ./axpyclang 1024

## Reference and Documentation for OMPT and Visualization
 * The wiki page https://github.com/passlab/passlab.github.io/wiki/Visualization-of-Data-Layout-and-Access-of-Parallel-Program-for-Productive-Performance-Analysis-and-Tuning
 * Chapter 4 (tool support) of the latest OpenMP TR4 (http://www.openmp.org/wp-content/uploads/openmp-tr4.pdf)
