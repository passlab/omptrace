
## Visu and power steering tool using OMPT

## Prerequisite and Installation
1. OpenMP implementation that supports OMPT interface, which is the omptool branch of the https://github.com/passlab/llvm-openmp REX repo. The omptool branch merged the official llvm-openmp repo with the following two changes:
    1. The implementation of the latest OMPT (https://github.com/OpenMPToolsInterface/LLVM-openmp/tree/towards_tr4)
    1. The master branch of the REX repo, which mainly provided two functions for omptool (one to retrieve the unique thread number (0, 1, ...) and the other for returning the total num of threads in the runtime system), see below. llvm-openmp actually provide those functions, but are not exposed.
    
For installation, the omptool branch can either be installed standalone or with llvm/clang compiler. 
  1. clone the repo and checkout the omptool branch (standalone or in the llvm/clang source tree)
   
           git clone https://github.com/passlab/llvm-openmp
           git checkout omptool
           
  1. cmake to create the makefile with OMPT_SUPPORT and REX_SUPPORT enabled
    
           mkdir BUILD
           cd BUILD
           cmake -G "Unix Makefiles" -DLIBOMP_OMPT_SUPPORT=TRUE -DLIBOMP_REX_SUPPORT=TRUE -DCMAKE_INSTALL_PREFIX=<install_path> ../llvm-openmp
           
           
    



### On orion.ec.oakland.edu
the compiler and runtime are already installed in /opt/llvm/llvm-ompt-install, so set the following
two env in your shell:

    export PATH=/opt/llvm/llvm-ompt-install/bin:$PATH
    export LD_LIBRARY_PATH=/opt/llvm/llvm-ompt-install/lib:$LD_LIBRARY_PATH
    
 If you want to build the LLVM runtime with the OpenMP support, please use the following cmake command:
 
    cmake -G "Unix Makefiles" -DLIBOMP_OMPT_SUPPORT=TRUE -DLIBOMP_REX_SUPPORT=TRUE -DCMAKE_INSTALL_PREFIX=/opt/llvm/llvm-ompt-install

Then, go to tests folder, and "make" to generate the matrix multiplication binary
Execute the code ./mmomp which will output lots of callback output we set in the callback.h file. 
"make hello" will create the simpler hello example

#### To get global thread num and the total number of threads:
Use the following two functions after you #include rex.h in the source code:

    int rex_get_global_thread_num(); /* return global thread num numbered from 0 ... */
    int rex_get_total_num_threads(); /* return the total number of threads in the runtime */
    
The rex.h header file is located in /opt/llvm/llvm-ompt-install/lib/clang/5.0.0/include and you may need to put the -I/opt/llvm/llvm-ompt-install/lib/clang/5.0.0/include flag in compiler if you are not using clang. 

### on fornax and using icc
llvm-openmp runtime needs to be installed and replacing the official OpenMP runtime from icc
https://github.com/OpenMPToolsInterface/LLVM-openmp/tree/towards_tr4 by letting LD_LIBRARY_PATH points to the ompt-enabled openmp runtime library. 

To use with clang:
Clang/LLVM and the above openmp runtime needs to be installed

## Reference and Documentation for OMPT and Visualization
 * The wiki page https://github.com/passlab/passlab.github.io/wiki/Visualization-of-Data-Layout-and-Access-of-Parallel-Program-for-Productive-Performance-Analysis-and-Tuning
 * Chapter 4 (tool support) of the latest OpenMP TR4 (http://www.openmp.org/wp-content/uploads/openmp-tr4.pdf)
 
## Development for visuomp (old)
Modify the callback.h file to have each callback do different things and in the tests folder to rebuild mmomp and run to 
test  your changes

## Development for power steering (old)
Build all power/freq related sources and the ompt_power.h file into one library libpowersteering.so

When you build you application, link the application with the library. 
icc or clang
clang -fopenmp axpy.c -L<llvm-openmp-lib-location> -lomp  -lpowersteering -o axpy

ldd axpy to check the executable uses the right library
