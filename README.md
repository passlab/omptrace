
# OMPTool for performance measurement, tracing and auto-steering support

## Prerequisite and Installation
1. OpenMP implementation that supports OMPT interface, which is the towards_tr4 branch of https://github.com/OpenMPToolsInterface/LLVM-openmp/tree/towards_tr4
    
### Installation of the the towards_tr4 branch, which support OMPT, either as standalone or with llvm/clang compiler. 
  1. clone the repo and checkout the branch (standalone or in the llvm/clang source tree)
   
           git clone https://github.com/OpenMPToolsInterface/LLVM-openmp openmp
	   cd openmp
           git remote update
           git checkout -b towards_tr4
           
  1. cmake to create the makefile with OMPT_SUPPORT abled, make it and install it (as of 2017/03/09, only version 45 is supported even the latest offcial runtime set 50 as default, which is happening after we merge, so we need to do -DLIBOMP_OMP_VERSION=45 in cmake)
    
           mkdir BUILD
           cd BUILD
           cmake -G "Unix Makefiles" -DLIBOMP_OMPT_SUPPORT=on -DLIBOMP_OMPT_TRACE=on -DLIBOMP_USE_HWLOC=on -DLIBOMP_HWLOC_INSTALL_DIR=/opt/hwloc-1.11.2-install -DCMAKE_INSTALL_PREFIX=<install_path> ..
           make; make install
           
      For using other compiler (on fornax), CC and CXX should be set for cmake. For example, on fornax as standalone: 
      
           CC=/opt/gcc-5.3.0-install/bin/gcc CXX=/opt/gcc-5.3.0-install/bin/g++ cmake -DLIBOMP_OMPT_SUPPORT=on -DLIBOMP_OMPT_TRACE=on -DLIBOMP_USE_HWLOC=on -DLIBOMP_HWLOC_INSTALL_DIR=/opt/hwloc-1.11.2-install -DCMAKE_INSTALL_PREFIX=<install_path> ..
           
  1. location for header files (omp.h and ompt.h) and libomp.so library are `<install_path>/include` and `<install_path>/lib` if the runtime is installed standalone. If it is installed as part of clang/llvm, the header location is `<install_path>/lib/clang/5.0.0/include`, and the libomp.so is from `<install_path>/lib`. Setup the library path for execution by letting LD_LIBRARY_PATH env include the lib path. For development and compiling, you need to provide the header path and lib path to the -I and -L flags of the compiler.

## Installation of omptool

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

#### On orion.ec.oakland.edu
The compiler and runtime are already installed in /opt/llvm/llvm-ompt-install, so set the following
two env in your shell:

    export PATH=/opt/llvm/llvm-ompt-install/bin:$PATH
    export LD_LIBRARY_PATH=/opt/llvm/llvm-ompt-install/lib:$LD_LIBRARY_PATH
    
##### To get global thread num and the total number of threads, using the following macro, which are defined in kmp_hack.h
	get_global_thread_num()
	get_global_num_threads()

#### on fornax and using icc
llvm-openmp runtime needs to be installed as standalone (use gcc 5.3.0 compiler, not the system default 4.8.3) by following the above insturctions and replacing the official OpenMP runtime from icc by letting LD_LIBRARY_PATH point to the ompt-enabled openmp runtime library. 

## To try omptool
1. Go to example folder, check the [Makefile](examples/Makefile) and modify the `OMP_LIB_PATH` to point to right location of omp.h/ompt.h files and the libomp.so lib as mentioned in the above. 
1. make to compile the examples, e.g. axpy. Currently, to make building simple, we build all the objects of omptool in the application binary (axpy and matmul). This is not required. The omptool could be built into a library (libomptool.so, e.g.) and use LD_PRELOAD to load the omptool lib and we can directly execute axpy/matmul or other examples directly without the need to recompile the application. 
1. make sure LD_LIBRARY_PATH to include the ompt-enabled libomp.so. Also to make sure axpy will use it, use `ldd axpy` to check the list of required library. 


## Reference and Documentation for OMPT and Visualization
 * The wiki page https://github.com/passlab/passlab.github.io/wiki/Visualization-of-Data-Layout-and-Access-of-Parallel-Program-for-Productive-Performance-Analysis-and-Tuning
 * Chapter 4 (tool support) of the latest OpenMP TR4 (http://www.openmp.org/wp-content/uploads/openmp-tr4.pdf)
