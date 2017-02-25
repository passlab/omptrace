
# Visu and power steering tool using OMPT

## Prerequisite and Installation
1. OpenMP implementation that supports OMPT interface, which is the omptool branch of the https://github.com/passlab/llvm-openmp REX repo. The omptool branch merged the official llvm-openmp repo with the following two changes:
    1. The implementation of the latest OMPT (https://github.com/OpenMPToolsInterface/LLVM-openmp/tree/towards_tr4)
    1. The master branch of the REX repo, which mainly provided two functions for omptool (one to retrieve the unique thread number (0, 1, ...) and the other for returning the total num of threads in the runtime system), see below. llvm-openmp actually provide those functions, but are not exposed.
    
### Installation of the omptool branch
The omptool branch can be installed either standalone or with llvm/clang compiler. 
  1. clone the repo and checkout the omptool branch (standalone or in the llvm/clang source tree)
   
           git clone https://github.com/passlab/llvm-openmp
           git checkout omptool
           
  1. cmake to create the makefile with OMPT_SUPPORT and REX_SUPPORT enabled, make it and install it
    
           mkdir BUILD
           cd BUILD
           cmake -G "Unix Makefiles" -DLIBOMP_OMPT_SUPPORT=TRUE -DLIBOMP_REX_SUPPORT=TRUE -DCMAKE_INSTALL_PREFIX=<install_path> ../llvm-openmp
           make; make install
           
           # For using other compiler, cmake should be called as: CC=/opt/gcc-5.3.0-install/bin/gcc CXX=/opt/gcc-5.3.0-install/bin/g++   cmake -G "Unix Makefiles" -DLIBOMP_OMPT_SUPPORT=TRUE -DLIBOMP_REX_SUPPORT=TRUE -DCMAKE_INSTALL_PREFIX=../../llvm-openmp-ompt-rex-install ..
           
  1. location for header files (omp.h, ompt.h, and rex.h) and libomp.so library are `<install_path>/include` and `<install_path>/lib` if the runtime is installed standalone. If it is installed as part of clang/llvm, the header location is `<install_path>/lib/clang/5.0.0/include`, and the libomp.so is from `<install_path>/lib`. Setup the library path for execution by letting LD_LIBRARY_PATH env include the lib path. For development and compiling, you need to provide the header path and lib path to the -I and -L flags of the compiler.

#### On orion.ec.oakland.edu
The compiler and runtime are already installed in /opt/llvm/llvm-ompt-install, so set the following
two env in your shell:

    export PATH=/opt/llvm/llvm-ompt-install/bin:$PATH
    export LD_LIBRARY_PATH=/opt/llvm/llvm-ompt-install/lib:$LD_LIBRARY_PATH
    
##### To get global thread num and the total number of threads:
Use the following two functions after you #include rex.h in the source code:

    int rex_get_global_thread_num(); /* return global thread num numbered from 0 ... */
    int rex_get_total_num_threads(); /* return the total number of threads in the runtime */
    
The rex.h header file is located in `<install_path>/lib/clang/5.0.0/include` and you may need to put the `-I<install_path>/lib/clang/5.0.0/include` flag in compiler if you are not using clang. 

#### on fornax and using icc
llvm-openmp runtime needs to be installed as standalone (use gcc 5.3.0 compiler, not the system default 4.8.3) by following the above insturctions and replacing the official OpenMP runtime from icc by letting LD_LIBRARY_PATH point to the ompt-enabled openmp runtime library. 

## To try omptool
1. Go to example folder, check the [Makefile](examples/Makefile) and modify the two variable `OMPT_REX_INCLUDE` and `OMP_LIB_PATH` to point to right location of omp.h/ompt.h/rex.h files and the libomp.so lib as mentioned in the above. 
1. make to compile the examples, e.g. axpy. Currently, to make building simple, we build all the objects of omptool in the application binary (axpy and matmul). This is not required. The omptool could be built into a library (libomptool.so, e.g.) and use LD_PRELOAD to load the omptool lib and we can directly execute axpy/matmul or other examples directly without the need to recompile the application. 
1. make sure LD_LIBRARY_PATH to include the ompt-rex enabled libomp.so. Also to make sure axpy will use it, use `ldd axpy` to check the list of required library. 


## Reference and Documentation for OMPT and Visualization
 * The wiki page https://github.com/passlab/passlab.github.io/wiki/Visualization-of-Data-Layout-and-Access-of-Parallel-Program-for-Productive-Performance-Analysis-and-Tuning
 * Chapter 4 (tool support) of the latest OpenMP TR4 (http://www.openmp.org/wp-content/uploads/openmp-tr4.pdf)
