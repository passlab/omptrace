
# OMPTool for performance measurement, tracing and auto-steering support

## Prerequisite and Installation
1. OpenMP implementation that supports OMPT interface, which is the llvm OpenMP runtime (https://git.llvm.org/git/openmp.git/)
    
### Installation of the LLVM OpenMP runtime, either as standalone or with llvm/clang compiler. The instructions below are for standalone and no installation. 
  1. clone the repo and checkout the branch
   
           git clone https://git.llvm.org/git/openmp.git/
           cd openmp
           
  1. cmake to create the makefile with OMPT_SUPPORT abled, build it and install it
    
           mkdir BUILD
           cd BUILD
           cmake -G "Unix Makefiles" -DLIBOMP_OMPT_SUPPORT=on ..
           make
           
      For using other compiler (on fornax), CC and CXX should be set for cmake. For example, on fornax as standalone: 
      
           CC=/opt/gcc-5.3.0-install/bin/gcc CXX=/opt/gcc-5.3.0-install/bin/g++ cmake -DLIBOMP_OMPT_SUPPORT=on ..
           
  1. The above instruction do not install the library, so all the headers and libomp.so are located in openmp/BUILD/runtime/src folder. 
     If you provide -DCMAKE_INSTALL_PREFIX=<install_path> in openmp or LLVM installation, header files (omp.h and ompt.h) and libomp.so 
     library are `<install_path>/include` and `<install_path>/lib` if the runtime is installed standalone. 
     If it is installed as part of clang/llvm, the header location is `<install_path>/lib/clang/5.0.0/include`, and libomp.so is from 
     `<install_path>/lib`. 

     In any case, you need to setup the library path for execution by letting LD_LIBRARY_PATH env include the lib path. 
     For development and compiling, you need to provide the header path and lib path to the -I and -L flags of the compiler.

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
An application (either in omptool/examples or others such as Lulesh or SPECOMP) needs to be built with either icc or clang in order to use the LLVM OpenMP runtime. In our case, it is the OMPT-enabled LLVM OpenMP runtime. 

A simple example in [examples/simple](examples/simple), which does not depend on the omptool (this repo) implementation, is provided for simple 
testing of the callback of OMPT, you can check the Makefile in the folder and it is simple to follow and modify to test your env setting

    cd omptool/examples/simple
    make run

Try omptool/examples/axpy.c
    
    cd omptool/examples
    clang -fopenmp axpy.c -o axpyclang  # or icc -fopenmp axpy.c -o axpyicc
    export LD_LIBRARY_PATH=/home/yanyh/tools/llvm-openmp/BUILD/runtime/src:$LD_LIBRARY_PATH
    ldd axpyclang # to check whether it will load the libomp.so in the folder we set in LD_LIBRARY_PATH
    LD_PRELOAD=/home/yanyh/tools/omptool/build/libomptool.so ./axpyclang 1024

## Reference and Documentation for OMPT and Visualization
 * The wiki page https://github.com/passlab/passlab.github.io/wiki/Visualization-of-Data-Layout-and-Access-of-Parallel-Program-for-Productive-Performance-Analysis-and-Tuning
 * Chapter 4 (tool support) of the latest OpenMP TR4 (http://www.openmp.org/wp-content/uploads/openmp-tr4.pdf)
