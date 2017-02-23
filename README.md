## Visu and power steering tool using OMPT

## Prerequisite Installation
Any OpenMP implementation that supports OMPT interface. So far, we use LLVM/Clang and the OpenMP runtime
is from https://github.com/OpenMPToolsInterface/LLVM-openmp/tree/towards_tr4, which is so far the only 
runtime that support the latest TR4 OMPT. Make sure you put -DLIBOMP_OMPT_SUPPORT=TRUE in cmake when building the runtime, and 
export the library path to the LD_LIBRARY_PATH env. 

### On orion.ec.oakland.edu
the compiler and runtime are already installed in /opt/llvm/llvm-ompt-install, so set the following
two env in your shell:

    export PATH=/opt/llvm/llvm-ompt-install/bin:$PATH
    export LD_LIBRARY_PATH=/opt/llvm/llvm-ompt-install/lib:$LD_LIBRARY_PATH

Then, go to tests folder, and "make" to generate the matrix multiplication binary
Execute the code ./mmomp which will output lots of callback output we set in the callback.h file. 
"make hello" will create the simpler hello example

### on fornax and using icc
llvm-openmp runtime needs to be installed and replacing the official OpenMP runtime from icc
https://github.com/OpenMPToolsInterface/LLVM-openmp/tree/towards_tr4 by letting LD_LIBRARY_PATH points to the ompt-enabled openmp runtime library. 

To use with clang:
Clang/LLVM and the above openmp runtime needs to be installed

## Reference and Documentation for OMPT and Visualization
 * The wiki page https://github.com/passlab/passlab.github.io/wiki/Visualization-of-Data-Layout-and-Access-of-Parallel-Program-for-Productive-Performance-Analysis-and-Tuning
 * Chapter 4 (tool support) of the latest OpenMP TR4 (http://www.openmp.org/wp-content/uploads/openmp-tr4.pdf)
 
## Development
Modify the callback.h file to have each callback do different things and in the tests folder to rebuild mmomp and run to 
test  your changes

## Development
Build all power/freq related sources and the ompt_power.h file into one library libpowersteering.so

When you build you application, link the application with the library. 
icc or clang
clang -fopenmp axpy.c -L<llvm-openmp-lib-location> -lomp  -lpowersteering -o axpy

ldd axpy to check the executable uses the right library
