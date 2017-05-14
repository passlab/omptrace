## towards_tr4 llvm openmp runtime standalone build on my Ubuntu vm

Note: 
 * append -DLIBOMP_HWLOC_INSTALL_DIR=/opt/hwloc-1.11.2-install to point to the location of hwloc install if cmake cannot find it. 

~~~~~
yan@vm:~/tools/llvm-openmp/BUILD$ cmake -G "Unix Makefiles" -DLIBOMP_OMPT_SUPPORT=on -DLIBOMP_OMPT_TRACE=on -DLIBOMP_USE_HWLOC=on  ..
-- The C compiler identification is GNU 5.4.0
-- The CXX compiler identification is GNU 5.4.0
-- Check for working C compiler: /usr/bin/cc
-- Check for working C compiler: /usr/bin/cc -- works
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Detecting C compile features
-- Detecting C compile features - done
-- Check for working CXX compiler: /usr/bin/c++
-- Check for working CXX compiler: /usr/bin/c++ -- works
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Performing Test LIBOMP_HAVE_STD_CPP11_FLAG
-- Performing Test LIBOMP_HAVE_STD_CPP11_FLAG - Success
-- Performing Test LIBOMP_HAVE_FNO_EXCEPTIONS_FLAG
-- Performing Test LIBOMP_HAVE_FNO_EXCEPTIONS_FLAG - Success
-- Performing Test LIBOMP_HAVE_FNO_RTTI_FLAG
-- Performing Test LIBOMP_HAVE_FNO_RTTI_FLAG - Success
-- Performing Test LIBOMP_HAVE_X_CPP_FLAG
-- Performing Test LIBOMP_HAVE_X_CPP_FLAG - Success
-- Performing Test LIBOMP_HAVE_WERROR_FLAG
-- Performing Test LIBOMP_HAVE_WERROR_FLAG - Success
-- Performing Test LIBOMP_HAVE_WNO_UNUSED_FUNCTION_FLAG
-- Performing Test LIBOMP_HAVE_WNO_UNUSED_FUNCTION_FLAG - Success
-- Performing Test LIBOMP_HAVE_WNO_UNUSED_LOCAL_TYPEDEF_FLAG
-- Performing Test LIBOMP_HAVE_WNO_UNUSED_LOCAL_TYPEDEF_FLAG - Failed
-- Performing Test LIBOMP_HAVE_WNO_UNUSED_VALUE_FLAG
-- Performing Test LIBOMP_HAVE_WNO_UNUSED_VALUE_FLAG - Success
-- Performing Test LIBOMP_HAVE_WNO_UNUSED_VARIABLE_FLAG
-- Performing Test LIBOMP_HAVE_WNO_UNUSED_VARIABLE_FLAG - Success
-- Performing Test LIBOMP_HAVE_WNO_SWITCH_FLAG
-- Performing Test LIBOMP_HAVE_WNO_SWITCH_FLAG - Success
-- Performing Test LIBOMP_HAVE_WNO_COVERED_SWITCH_DEFAULT_FLAG
-- Performing Test LIBOMP_HAVE_WNO_COVERED_SWITCH_DEFAULT_FLAG - Failed
-- Performing Test LIBOMP_HAVE_WNO_DEPRECATED_REGISTER_FLAG
-- Performing Test LIBOMP_HAVE_WNO_DEPRECATED_REGISTER_FLAG - Failed
-- Performing Test LIBOMP_HAVE_WNO_SIGN_COMPARE_FLAG
-- Performing Test LIBOMP_HAVE_WNO_SIGN_COMPARE_FLAG - Success
-- Performing Test LIBOMP_HAVE_WNO_GNU_ANONYMOUS_STRUCT_FLAG
-- Performing Test LIBOMP_HAVE_WNO_GNU_ANONYMOUS_STRUCT_FLAG - Failed
-- Performing Test LIBOMP_HAVE_WNO_UNKNOWN_PRAGMAS_FLAG
-- Performing Test LIBOMP_HAVE_WNO_UNKNOWN_PRAGMAS_FLAG - Success
-- Performing Test LIBOMP_HAVE_WNO_MISSING_FIELD_INITIALIZERS_FLAG
-- Performing Test LIBOMP_HAVE_WNO_MISSING_FIELD_INITIALIZERS_FLAG - Success
-- Performing Test LIBOMP_HAVE_WNO_MISSING_BRACES_FLAG
-- Performing Test LIBOMP_HAVE_WNO_MISSING_BRACES_FLAG - Success
-- Performing Test LIBOMP_HAVE_WNO_COMMENT_FLAG
-- Performing Test LIBOMP_HAVE_WNO_COMMENT_FLAG - Success
-- Performing Test LIBOMP_HAVE_WNO_SELF_ASSIGN_FLAG
-- Performing Test LIBOMP_HAVE_WNO_SELF_ASSIGN_FLAG - Failed
-- Performing Test LIBOMP_HAVE_WNO_VLA_EXTENSION_FLAG
-- Performing Test LIBOMP_HAVE_WNO_VLA_EXTENSION_FLAG - Failed
-- Performing Test LIBOMP_HAVE_WNO_FORMAT_PEDANTIC_FLAG
-- Performing Test LIBOMP_HAVE_WNO_FORMAT_PEDANTIC_FLAG - Failed
-- Performing Test LIBOMP_HAVE_MSSE2_FLAG
-- Performing Test LIBOMP_HAVE_MSSE2_FLAG - Success
-- Performing Test LIBOMP_HAVE_FTLS_MODEL_FLAG
-- Performing Test LIBOMP_HAVE_FTLS_MODEL_FLAG - Success
-- Performing Test LIBOMP_HAVE_MMIC_FLAG
-- Performing Test LIBOMP_HAVE_MMIC_FLAG - Failed
-- Performing Test LIBOMP_HAVE_M32_FLAG
-- Performing Test LIBOMP_HAVE_M32_FLAG - Success
-- Performing Test LIBOMP_HAVE_X_FLAG
-- Performing Test LIBOMP_HAVE_X_FLAG - Success
-- Performing Test LIBOMP_HAVE_WARN_SHARED_TEXTREL_FLAG
-- Performing Test LIBOMP_HAVE_WARN_SHARED_TEXTREL_FLAG - Success
-- Performing Test LIBOMP_HAVE_AS_NEEDED_FLAG
-- Performing Test LIBOMP_HAVE_AS_NEEDED_FLAG - Success
-- Performing Test LIBOMP_HAVE_VERSION_SCRIPT_FLAG
-- Performing Test LIBOMP_HAVE_VERSION_SCRIPT_FLAG - Success
-- Performing Test LIBOMP_HAVE_STATIC_LIBGCC_FLAG
-- Performing Test LIBOMP_HAVE_STATIC_LIBGCC_FLAG - Success
-- Performing Test LIBOMP_HAVE_Z_NOEXECSTACK_FLAG
-- Performing Test LIBOMP_HAVE_Z_NOEXECSTACK_FLAG - Success
-- Performing Test LIBOMP_HAVE_FINI_FLAG
-- Performing Test LIBOMP_HAVE_FINI_FLAG - Success
-- Looking for pthread.h
-- Looking for pthread.h - found
-- Looking for pthread_create
-- Looking for pthread_create - not found
-- Looking for pthread_create in pthreads
-- Looking for pthread_create in pthreads - not found
-- Looking for pthread_create in pthread
-- Looking for pthread_create in pthread - found
-- Found Threads: TRUE  
-- Found Perl: /usr/bin/perl (found version "5.22.1") 
-- Performing Test LIBOMP_HAVE_VERSION_SYMBOLS
-- Performing Test LIBOMP_HAVE_VERSION_SYMBOLS - Success
-- Performing Test LIBOMP_HAVE___BUILTIN_FRAME_ADDRESS
-- Performing Test LIBOMP_HAVE___BUILTIN_FRAME_ADDRESS - Success
-- Performing Test LIBOMP_HAVE_WEAK_ATTRIBUTE
-- Performing Test LIBOMP_HAVE_WEAK_ATTRIBUTE - Success
-- Looking for include files windows.h, psapi.h
-- Looking for include files windows.h, psapi.h - not found
-- Looking for EnumProcessModules in psapi
-- Looking for EnumProcessModules in psapi - not found
-- Looking for hwloc.h
-- Looking for hwloc.h - found
-- Looking for hwloc_topology_init in /usr/lib/x86_64-linux-gnu/libhwloc.so
-- Looking for hwloc_topology_init in /usr/lib/x86_64-linux-gnu/libhwloc.so - found
-- LIBOMP: Operating System     -- Linux
-- LIBOMP: Target Architecture  -- x86_64
-- LIBOMP: Build Type           -- Release
-- LIBOMP: OpenMP Version       -- 50
-- LIBOMP: Library Kind         -- SHARED
-- LIBOMP: Library Type         -- normal
-- LIBOMP: Fortran Modules      -- FALSE
-- LIBOMP: Build                -- 20140926
-- LIBOMP: Use Stats-gathering  -- FALSE
-- LIBOMP: Use Debugger-support -- FALSE
-- LIBOMP: Use ITT notify       -- TRUE
-- LIBOMP: Use OMPT-support     -- on
-- LIBOMP: Use OMPT-optional  -- TRUE
-- LIBOMP: Use Adaptive locks   -- TRUE
-- LIBOMP: Use quad precision   -- TRUE
-- LIBOMP: Use TSAN-support     -- FALSE
-- LIBOMP: Use Hwloc library    -- on
-- Found PythonInterp: /usr/bin/python (found version "2.7.12") 
-- Looking for sqrt in m
-- Looking for sqrt in m - found
-- Looking for __atomic_load_1
-- Looking for __atomic_load_1 - not found
-- Looking for __atomic_load_1 in atomic
-- Looking for __atomic_load_1 in atomic - found
-- LIBOMP: Cannot find llvm-lit.
-- LIBOMP: Please put llvm-lit in your PATH, set LIBOMP_LLVM_LIT_EXECUTABLE to its full path or point OPENMP_LLVM_TOOLS_DIR to its directory
CMake Warning at runtime/cmake/LibompUtils.cmake:21 (message):
  LIBOMP: The check-libomp target will not be available!
Call Stack (most recent call first):
  runtime/test/CMakeLists.txt:61 (libomp_warning_say)


-- Performing Test LIBOMPTARGET_HAVE_STD_CPP11_FLAG
-- Performing Test LIBOMPTARGET_HAVE_STD_CPP11_FLAG - Success
-- Performing Test LIBOMPTARGET_HAVE_WERROR_FLAG
-- Performing Test LIBOMPTARGET_HAVE_WERROR_FLAG - Success
-- Found LIBOMPTARGET_DEP_LIBELF: /usr/lib/x86_64-linux-gnu/libelf.so  
-- Found PkgConfig: /usr/bin/pkg-config (found version "0.29.1") 
-- Could NOT find LIBOMPTARGET_DEP_LIBFFI (missing:  LIBOMPTARGET_DEP_LIBFFI_LIBRARIES LIBOMPTARGET_DEP_LIBFFI_INCLUDE_DIRS) 
-- LIBOMPTARGET: Building offloading runtime library libomptarget.
-- LIBOMPTARGET: Not building aarch64 offloading plugin: machine not found in the system.
-- LIBOMPTARGET: Building CUDA offloading plugin.
-- LIBOMPTARGET: Not building PPC64 offloading plugin: machine not found in the system.
-- LIBOMPTARGET: Not building PPC64le offloading plugin: machine not found in the system.
-- LIBOMPTARGET: Not building x86_64 offloading plugin: libffi dependency not found.
-- LIBOMPTARGET: Cannot find llvm-lit.
-- LIBOMPTARGET: Please put llvm-lit in your PATH or set LIBOMPTARGET_LLVM_LIT_EXECUTABLE to its full path or point OPENMP_LLVM_TOOLS_DIR to its directory
CMake Warning at libomptarget/cmake/Modules/LibomptargetUtils.cmake:21 (message):
  LIBOMPTARGET: The check-libomptarget target will not be available!
Call Stack (most recent call first):
  libomptarget/test/CMakeLists.txt:32 (libomptarget_warning_say)


-- Configuring done
-- Generating done
-- Build files have been written to: /home/yan/tools/llvm-openmp/BUILD
yan@vm:~/tools/llvm-openmp/BUILD$ make
Scanning dependencies of target libomp-needed-headers
[  2%] Generating kmp_i18n_id.inc
[  5%] Generating kmp_i18n_default.inc
[  5%] Built target libomp-needed-headers
Scanning dependencies of target omp
[  7%] Building C object runtime/src/CMakeFiles/omp.dir/thirdparty/ittnotify/ittnotify_static.c.o
[ 10%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_alloc.cpp.o
[ 12%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_atomic.cpp.o
[ 15%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_csupport.cpp.o
[ 17%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_debug.cpp.o
[ 20%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_itt.cpp.o
[ 22%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_environment.cpp.o
[ 25%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_error.cpp.o
[ 27%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_global.cpp.o
[ 30%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_i18n.cpp.o
[ 32%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_io.cpp.o
[ 35%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_runtime.cpp.o
[ 37%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_settings.cpp.o
[ 40%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_str.cpp.o
[ 42%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_tasking.cpp.o
[ 45%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_taskq.cpp.o
[ 47%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_threadprivate.cpp.o
[ 50%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_utility.cpp.o
[ 52%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_barrier.cpp.o
[ 55%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_wait_release.cpp.o
[ 57%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_affinity.cpp.o
[ 60%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_dispatch.cpp.o
[ 62%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_lock.cpp.o
[ 65%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_sched.cpp.o
[ 67%] Building CXX object runtime/src/CMakeFiles/omp.dir/z_Linux_util.cpp.o
[ 70%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_gsupport.cpp.o
[ 72%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_taskdeps.cpp.o
[ 75%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_cancel.cpp.o
[ 77%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_ftn_cdecl.cpp.o
[ 80%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_ftn_extra.cpp.o
[ 82%] Building CXX object runtime/src/CMakeFiles/omp.dir/kmp_version.cpp.o
[ 85%] Building CXX object runtime/src/CMakeFiles/omp.dir/ompt-general.cpp.o
[ 87%] Building C object runtime/src/CMakeFiles/omp.dir/z_Linux_asm.s.o
[ 90%] Linking C shared library libomp.so
[ 90%] Built target omp
Scanning dependencies of target omptarget
[ 92%] Building CXX object libomptarget/CMakeFiles/omptarget.dir/src/omptarget.cpp.o
[ 95%] Linking CXX shared library libomptarget.so
[ 95%] Built target omptarget
Scanning dependencies of target omptarget.rtl.cuda
[ 97%] Building CXX object libomptarget/plugins/cuda/CMakeFiles/omptarget.rtl.cuda.dir/src/rtl.cpp.o
[100%] Linking CXX shared library libomptarget.rtl.cuda.so
[100%] Built target omptarget.rtl.cuda
yan@vm:~/tools/llvm-openmp/BUILD$ ls

~~~~~
