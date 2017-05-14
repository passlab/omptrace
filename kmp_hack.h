#ifndef __KMP_HACK_H__
#define __KMP_HACK_H__

extern int __kmpc_global_thread_num(void *);
extern int __kmpc_global_num_threads(void *);

#define get_global_thread_num() __kmpc_global_thread_num(NULL)
#define get_global_num_threads() __kmpc_global_num_threads(NULL)

#endif
