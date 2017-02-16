#include <stdio.h>
#include <inttypes.h>
#include <omp.h>
#include <ompt.h>
#include <execinfo.h>
#include <sched.h>
#include "cpufreq.h"

#define USERSPACE

static const char* ompt_thread_type_t_values[] = {
  NULL,
  "ompt_thread_initial",
  "ompt_thread_worker",
  "ompt_thread_other"
};

static const char* ompt_task_type_t_values[] = {
  NULL,
  "ompt_task_initial",
  "ompt_task_implicit",
  "ompt_task_explicit",
  "ompt_task_target"
};

static ompt_get_task_data_t ompt_get_task_data;
static ompt_get_task_frame_t ompt_get_task_frame;
static ompt_get_thread_data_t ompt_get_thread_data;
static ompt_get_parallel_data_t ompt_get_parallel_data;
static ompt_get_unique_id_t ompt_get_unique_id;


double ompt_time;
#ifdef USERSPACE
unsigned long high_freq = 2300000;
unsigned long low_freq  = 1300000;
unsigned long kernelCpuId_freq[72];
int state_of_idle[72] = {1}; // 0 is in the beginning of idle state; 1 means the end of idle state. 
int ompt_num_threads;
//double timestamp;
#endif

static void print_ids(int level)
{
  ompt_frame_t* frame = ompt_get_task_frame(level);
  if (frame)
    printf("%" PRIu64 ": level %d: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", exit_frame=%p, reenter_frame=%p\n", ompt_get_thread_data()->value, level, ompt_get_parallel_data(level).value, ompt_get_task_data(level).value, frame->exit_runtime_frame, frame->reenter_runtime_frame);
  else
    printf("%" PRIu64 ": level %d: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", frame=%p\n", ompt_get_thread_data()->value, level, ompt_get_parallel_data(level).value, ompt_get_task_data(level).value,               frame);
}

#define print_frame(level)\
do {\
  printf("%" PRIu64 ": __builtin_frame_address(%d)=%p\n", ompt_get_thread_data()->value, level, __builtin_frame_address(level));\
} while(0)

static void print_current_address()
{
    int real_level = 2;
    void *array[real_level];
    size_t size;
    void *address;
  
    size = backtrace (array, real_level);
    if(size == real_level)
      address = array[real_level-1]-5;
    else
      address = NULL;
  printf("%" PRIu64 ": current_address=%p\n", ompt_get_thread_data()->value, address);
}

static void
on_ompt_callback_mutex_acquire(
  ompt_mutex_kind_t kind,
  unsigned int hint,
  unsigned int impl,
  ompt_wait_id_t wait_id,
  const void *codeptr_ra)
{
  switch(kind)
  {
    case ompt_mutex_lock:
      printf("%" PRIu64 ": ompt_event_wait_lock: wait_id=%" PRIu64 ", hint=%" PRIu32 ", impl=%" PRIu32 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, hint, impl, codeptr_ra);
      break;
    case ompt_mutex_nest_lock:
      printf("%" PRIu64 ": ompt_event_wait_nest_lock: wait_id=%" PRIu64 ", hint=%" PRIu32 ", impl=%" PRIu32 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, hint, impl, codeptr_ra);
      break;
    case ompt_mutex_critical:
      printf("%" PRIu64 ": ompt_event_wait_critical: wait_id=%" PRIu64 ", hint=%" PRIu32 ", impl=%" PRIu32 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, hint, impl, codeptr_ra);
      break;
    case ompt_mutex_atomic:
      printf("%" PRIu64 ": ompt_event_wait_atomic: wait_id=%" PRIu64 ", hint=%" PRIu32 ", impl=%" PRIu32 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, hint, impl, codeptr_ra);
      break;
    case ompt_mutex_ordered:
      printf("%" PRIu64 ": ompt_event_wait_ordered: wait_id=%" PRIu64 ", hint=%" PRIu32 ", impl=%" PRIu32 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, hint, impl, codeptr_ra);
      break;
    default:
      break;
  }
}

static void
on_ompt_callback_mutex_acquired(
  ompt_mutex_kind_t kind,
  ompt_wait_id_t wait_id,
  const void *codeptr_ra)
{
  switch(kind)
  {
    case ompt_mutex_lock:
      printf("%" PRIu64 ": ompt_event_acquired_lock: wait_id=%" PRIu64 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_mutex_nest_lock:
      printf("%" PRIu64 ": ompt_event_acquired_nest_lock_first: wait_id=%" PRIu64 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_mutex_critical:
      printf("%" PRIu64 ": ompt_event_acquired_critical: wait_id=%" PRIu64 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_mutex_atomic:
      printf("%" PRIu64 ": ompt_event_acquired_atomic: wait_id=%" PRIu64 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_mutex_ordered:
      printf("%" PRIu64 ": ompt_event_acquired_ordered: wait_id=%" PRIu64 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    default:
      break;
  }
}

static void
on_ompt_callback_mutex_released(
  ompt_mutex_kind_t kind,
  ompt_wait_id_t wait_id,
  const void *codeptr_ra)
{
  switch(kind)
  {
    case ompt_mutex_lock:
      printf("%" PRIu64 ": ompt_event_release_lock: wait_id=%" PRIu64 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_mutex_nest_lock:
      printf("%" PRIu64 ": ompt_event_release_nest_lock_last: wait_id=%" PRIu64 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_mutex_critical:
      printf("%" PRIu64 ": ompt_event_release_critical: wait_id=%" PRIu64 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_mutex_atomic:
      printf("%" PRIu64 ": ompt_event_release_atomic: wait_id=%" PRIu64 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_mutex_ordered:
      printf("%" PRIu64 ": ompt_event_release_ordered: wait_id=%" PRIu64 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    default:
      break;
  }
}

static void
on_ompt_callback_nest_lock(
    ompt_scope_endpoint_t endpoint,
    ompt_wait_id_t wait_id,
    const void *codeptr_ra)
{
  switch(endpoint)
  {
    case ompt_scope_begin:
      printf("%" PRIu64 ": ompt_event_acquired_nest_lock_next: wait_id=%" PRIu64 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_scope_end:
      printf("%" PRIu64 ": ompt_event_release_nest_lock_prev: wait_id=%" PRIu64 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
  }
}

static void
on_ompt_callback_sync_region(
  ompt_sync_region_kind_t kind,
  ompt_scope_endpoint_t endpoint,
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  const void *codeptr_ra)
{
  switch(endpoint)
  {
    case ompt_scope_begin:
      switch(kind)
      {
        case ompt_sync_region_barrier:
          printf("%" PRIu64 ": ompt_event_barrier_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", return_address=%p\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra);
          print_ids(0);
          break;
        case ompt_sync_region_taskwait:
          break;
        case ompt_sync_region_taskgroup:
          break;
      }
      break;
    case ompt_scope_end:
      switch(kind)
      {
        case ompt_sync_region_barrier:
          printf("%" PRIu64 ": ompt_event_barrier_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", return_address=%p\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra);
          break;
        case ompt_sync_region_taskwait:
          break;
        case ompt_sync_region_taskgroup:
          break;
      }
      break;
  }
}

static void
on_ompt_callback_sync_region_wait(
  ompt_sync_region_kind_t kind,
  ompt_scope_endpoint_t endpoint,
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  const void *codeptr_ra)
{
  switch(endpoint)
  {
    case ompt_scope_begin:
      switch(kind)
      {
        case ompt_sync_region_barrier:
          printf("%" PRIu64 ": ompt_event_wait_barrier_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", return_address=%p\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra);
          break;
        case ompt_sync_region_taskwait:
          break;
        case ompt_sync_region_taskgroup:
          break;
      }
      break;
    case ompt_scope_end:
      switch(kind)
      {
        case ompt_sync_region_barrier:
          printf("%" PRIu64 ": ompt_event_wait_barrier_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", return_address=%p\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra);
          break;
        case ompt_sync_region_taskwait:
          break;
        case ompt_sync_region_taskgroup:
          break;
      }
      break;
  }
}

static void
on_ompt_event_control(
  uint64_t command,
  uint64_t modifier)
{
  printf("%" PRIu64 ": ompt_event_control: command=%" PRIu64 ", modifier=%" PRIu64 "\n", ompt_get_thread_data()->value, command, modifier);
}

static void
on_ompt_callback_flush(
    ompt_data_t *thread_data,
    const void *codeptr_ra)
{
  printf("%" PRIu64 ": ompt_event_flush: codeptr_ra=%p\n", thread_data->value, codeptr_ra);
}


static void
on_ompt_callback_idle(
  ompt_scope_endpoint_t endpoint)
{

#ifdef USERSPACE
  int id = sched_getcpu();      
  int pair_id;
  //print_ids(0); 
  //double currentTime = omp_get_wtime();

  //if(currentTime - timestamp > 0.01)
  //{		
  //	timestamp = currentTime;
  //	id = sched_getcpu();
#endif

  switch(endpoint)
  {
    case ompt_scope_begin:
      /* reduce the frequency */
      printf("%" PRIu64 ": ompt_event_idle_begin:ID:%d\n", ompt_get_thread_data()->value,id);
      /* associate the address with cpu id */
      //printf("%" PRIu64 ": ompt_event_idle_begin: thread_id=%" PRIu64 "\n", ompt_get_thread_data()->value, thread_data.value);
#ifdef USERSPACE
      if(id != 36 || id != 0)	
      {
	//set up the state of kernel cpu id as the beginning of idle.
	state_of_idle[id] = 0;
	//pair id of the current in the same core
	if(id<36) pair_id = id+36;
	else pair_id = id-36;
	//if both kernel cpu id are at the idle state, set up both as low frequency 
	    if(state_of_idle[id] == 0 && state_of_idle[pair_id] == 0)
	    {
      	        cpufreq_set_frequency(id,low_freq);
	        kernelCpuId_freq[id] = low_freq;	
		cpufreq_set_frequency(pair_id,low_freq);
                kernelCpuId_freq[pair_id] = low_freq;
	    }
      }
#endif
      break;
    case ompt_scope_end:
      /* restore the frequency */
      printf("%" PRIu64 ": ompt_event_idle_end:ID:%d\n", ompt_get_thread_data()->value,id);
      //printf("%" PRIu64 ": ompt_event_idle_end: thread_id=%" PRIu64 "\n", ompt_get_thread_data()->value, thread_data.value);
#ifdef USERSPACE
      if(id != 36 || id != 0) 
      {	
      	cpufreq_set_frequency(id,high_freq);
      	kernelCpuId_freq[id] = high_freq;
	state_of_idle[id] = 1;
      }
#endif
      break;
  }
#ifdef USERSPACE
 // }
#endif

}

static void
on_ompt_callback_implicit_task(
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    //unsigned int team_size,
    unsigned int thread_num)
{
  switch(endpoint)
  {
    case ompt_scope_begin:
      task_data->value = ompt_get_unique_id();
      printf("%" PRIu64 ": ompt_event_implicit_task_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value);
      //printf("%" PRIu64 ": ompt_event_implicit_task_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", team_size=%" PRIu32 ", thread_num=%" PRIu32 "\n", ompt_get_thread_data().value, parallel_data->value, task_data->value, team_size, thread_num);
      break;
    case ompt_scope_end:
      printf("%" PRIu64 ": ompt_event_implicit_task_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value);
      //printf("%" PRIu64 ": ompt_event_implicit_task_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", team_size=%" PRIu32 ", thread_num=%" PRIu32 "\n", ompt_get_thread_data().value, parallel_data->value, task_data->value, team_size, thread_num);
      break;
  }
}

static void
on_ompt_event_initial_task_begin(
  ompt_parallel_data_t parallel_data,
  ompt_task_data_t task_data)
{
  printf("%" PRIu64 ": ompt_event_initial_task_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data.value, task_data.value);
}

static void
on_ompt_event_initial_task_end(
  ompt_parallel_data_t parallel_data,
  ompt_task_data_t task_data)
{
  printf("%" PRIu64 ": ompt_event_initial_task_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data.value, task_data.value);
}

static void
on_ompt_callback_lock_init(
  ompt_mutex_kind_t kind,
  unsigned int hint,
  unsigned int impl,
  ompt_wait_id_t wait_id,
  const void *codeptr_ra)
{
  switch(kind)
  {
    case ompt_mutex_lock:
      printf("%" PRIu64 ": ompt_event_init_lock: wait_id=%" PRIu64 ", hint=%" PRIu32 ", impl=%" PRIu32 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, hint, impl, codeptr_ra);
      break;
    case ompt_mutex_nest_lock:
      printf("%" PRIu64 ": ompt_event_init_nest_lock: wait_id=%" PRIu64 ", hint=%" PRIu32 ", impl=%" PRIu32 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, hint, impl, codeptr_ra);
      break;
    default:
      break;
  }
}

static void
on_ompt_callback_lock_destroy(
  ompt_mutex_kind_t kind,
  ompt_wait_id_t wait_id,
  const void *codeptr_ra)
{
  switch(kind)
  {
    case ompt_mutex_lock:
      printf("%" PRIu64 ": ompt_event_destroy_lock: wait_id=%" PRIu64 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_mutex_nest_lock:
      printf("%" PRIu64 ": ompt_event_destroy_nest_lock: wait_id=%" PRIu64 ", return_address=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    default:
      break;
  }
}

static void
on_ompt_callback_work(
  ompt_work_type_t wstype,
  ompt_scope_endpoint_t endpoint,
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  uint64_t count,
  const void *codeptr_ra)
{
  switch(endpoint)
  {
    case ompt_scope_begin:
      switch(wstype)
      {
        case ompt_work_loop:
          printf("%" PRIu64 ": ompt_event_loop_begin: parallel_id=%" PRIu64 ", parent_task_id=%" PRIu64 ", workshare_function=%p, count=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra, count);
          break;
        case ompt_work_sections:
          //impl
          break;
        case ompt_work_single_executor:
          printf("%" PRIu64 ": ompt_event_single_in_block_begin: parallel_id=%" PRIu64 ", parent_task_id=%" PRIu64 ", workshare_function=%p, count=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra, count);
          break;
        case ompt_work_single_other:
          printf("%" PRIu64 ": ompt_event_single_others_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", workshare_function=%p, count=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra, count);
          break;
        case ompt_work_workshare:
          //impl
          break;
        case ompt_work_distribute:
          //impl
          break;
        case ompt_work_taskloop:
          //impl
          break;
      }
      break;
    case ompt_scope_end:
      switch(wstype)
      {
        case ompt_work_loop:
          printf("%" PRIu64 ": ompt_event_loop_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", workshare_function=%p, count=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra, count);
          break;
        case ompt_work_sections:
          //impl
          break;
        case ompt_work_single_executor:
          printf("%" PRIu64 ": ompt_event_single_in_block_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", workshare_function=%p, count=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra, count);
          break;
        case ompt_work_single_other:
          printf("%" PRIu64 ": ompt_event_single_others_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", workshare_function=%p, count=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra, count);
          break;
        case ompt_work_workshare:
          //impl
          break;
        case ompt_work_distribute:
          //impl
          break;
        case ompt_work_taskloop:
          //impl
          break;
      }
      break;
  }
}

static void
on_ompt_callback_master(
  ompt_scope_endpoint_t endpoint,
  ompt_parallel_data_t *parallel_data,
  ompt_task_data_t *task_data,
  const void *codeptr_ra)
{
  switch(endpoint)
  {
    case ompt_scope_begin:
      printf("%" PRIu64 ": ompt_event_master_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", codeptr_ra=%p\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra);
      break;
    case ompt_scope_end:
      printf("%" PRIu64 ": ompt_event_master_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", codeptr_ra=%p\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra);
      break;
  }
}

static void
on_ompt_callback_parallel_begin(
  ompt_data_t *parent_task_data,
  ompt_frame_t *parent_task_frame,
  ompt_data_t* parallel_data,
  uint32_t requested_team_size,
  uint32_t actual_team_size,
  ompt_invoker_t invoker,
  void *codeptr_ra)
{
  parallel_data->value = ompt_get_unique_id();
  //printf("%" PRIu64 ": ompt_event_parallel_begin: parent_task_id=%" PRIu64 ", parent_task_frame.exit=%p, parent_task_frame.reenter=%p, parallel_id=%" PRIu64 ", requested_team_size=%" PRIu32 ", parallel_function=%p, invoker=%d, actual_team_size=%" PRIu32 "\n", ompt_get_thread_data()->value, parent_task_data->value, parent_task_frame->exit_runtime_frame, parent_task_frame->reenter_runtime_frame, parallel_data->value, requested_team_size, codeptr_ra, invoker, actual_team_size);
  print_ids(1);
}

static void
on_ompt_callback_parallel_end(
  ompt_data_t *parallel_data,
  ompt_task_data_t *task_data,
  ompt_invoker_t invoker,
  void *codeptr_ra)
{
  printf("%" PRIu64 ": ompt_event_parallel_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", invoker=%d, codeptr_ra=%p\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, invoker, codeptr_ra);
}

static void
on_ompt_event_runtime_shutdown(void)
{
  printf("%d: ompt_event_runtime_shutdown\n", omp_get_thread_num());
}

static void
on_ompt_event_sections_begin(
  ompt_parallel_data_t parallel_data,
  ompt_task_data_t parent_task_data,
  void *workshare_function)
{
  printf("%" PRIu64 ": ompt_event_sections_begin: parallel_id=%" PRIu64 ", parent_task_id=%" PRIu64 ", workshare_function=%p\n", ompt_get_thread_data()->value, parallel_data.value, parent_task_data.value, workshare_function);
}

static void
on_ompt_event_sections_end(
  ompt_parallel_data_t parallel_data,
  ompt_task_data_t task_data)
{
  printf("%" PRIu64 ": ompt_event_sections_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data.value, task_data.value);
}

static void
on_ompt_callback_task_create(
    ompt_task_data_t *parent_task_data,    /* id of parent task            */
    ompt_frame_t *parent_frame,  /* frame data for parent task   */
    ompt_task_data_t* new_task_data,      /* id of created task           */
    ompt_task_type_t type,
    int has_dependences,
    void *codeptr_ra)               /* pointer to outlined function */
{
  new_task_data->value = ompt_get_unique_id();
  printf("%" PRIu64 ": ompt_event_task_create: parent_task_id=%" PRIu64 ", parent_task_frame.exit=%p, parent_task_frame.reenter=%p, new_task_id=%" PRIu64 ", parallel_function=%p, task_type=%s=%d, has_dependences=%s\n", ompt_get_thread_data()->value, parent_task_data->value, parent_frame->exit_runtime_frame, parent_frame->reenter_runtime_frame, new_task_data->value, codeptr_ra, ompt_task_type_t_values[type], type, has_dependences ? "yes" : "no");
}

static void
on_ompt_callback_task_schedule(
    ompt_task_data_t *first_task_data,
    ompt_task_status_t prior_task_status,
    ompt_task_data_t *second_task_data)
{
  printf("%" PRIu64 ": ompt_event_task_schedule: first_task_id=%" PRIu64 ", second_task_id=%" PRIu64 ", prior_task_status=%d\n", ompt_get_thread_data()->value, first_task_data->value, second_task_data->value, prior_task_status);
  if(prior_task_status == ompt_task_complete)
  {
    printf("%" PRIu64 ": ompt_event_task_end: task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, first_task_data->value);
  }
}

static void
on_ompt_callback_task_dependences(
  ompt_task_data_t *task_data,
  const ompt_task_dependence_t *deps,
  int ndeps)
{
  printf("%" PRIu64 ": ompt_event_task_dependences: task_id=%" PRIu64 ", deps=%p, ndeps=%d\n", ompt_get_thread_data()->value, task_data->value, (void *)deps, ndeps);
}

static void
on_ompt_callback_task_dependence(
  ompt_task_data_t *first_task_data,
  ompt_task_data_t *second_task_data)
{
  printf("%" PRIu64 ": ompt_event_task_dependence_pair: first_task_id=%" PRIu64 ", second_task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, first_task_data->value, second_task_data->value);
}

static void
on_ompt_event_wait_taskgroup_begin(
  ompt_parallel_data_t parallel_data,
  ompt_task_data_t task_data)
{
  printf("%" PRIu64 ": ompt_event_wait_taskgroup_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data.value, task_data.value);
}

static void
on_ompt_event_taskgroup_begin(
  ompt_parallel_data_t parallel_data,
  ompt_task_data_t task_data)
{
  printf("%" PRIu64 ": ompt_event_taskgroup_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data.value, task_data.value);
}

static void
on_ompt_event_wait_taskgroup_end(
  ompt_parallel_data_t parallel_data,
  ompt_task_data_t task_data)
{
  printf("%" PRIu64 ": ompt_event_wait_taskgroup_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data.value, task_data.value);
}

static void
on_ompt_event_taskgroup_end(
  ompt_parallel_data_t parallel_data,
  ompt_task_data_t task_data)
{
  printf("%" PRIu64 ": ompt_event_taskgroup_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data.value, task_data.value);
}

static void
on_ompt_event_wait_taskwait_begin(
  ompt_parallel_data_t parallel_data,
  ompt_task_data_t task_data)
{
  printf("%" PRIu64 ": ompt_event_wait_taskwait_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data.value, task_data.value);
}

static void
on_ompt_event_taskwait_begin(
  ompt_parallel_data_t parallel_data,
  ompt_task_data_t task_data)
{
  printf("%" PRIu64 ": ompt_event_taskwait_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data.value, task_data.value);
}

static void
on_ompt_event_wait_taskwait_end(
  ompt_parallel_data_t parallel_data,
  ompt_task_data_t task_data)
{
  printf("%" PRIu64 ": ompt_event_wait_taskwait_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data.value, task_data.value);
}

static void
on_ompt_event_taskwait_end(
  ompt_parallel_data_t parallel_data,
  ompt_task_data_t task_data)
{
  printf("%" PRIu64 ": ompt_event_taskwait_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data.value, task_data.value);
}

static void
on_ompt_callback_thread_begin(
  ompt_thread_type_t thread_type,
  ompt_data_t *thread_data)
{
  thread_data->value = ompt_get_unique_id();
  printf("%" PRIu64 ": ompt_event_thread_begin: thread_type=%s=%d, thread_id=%" PRIu64 "\n", ompt_get_thread_data()->value, ompt_thread_type_t_values[thread_type], thread_type, thread_data->value);
}

static void
on_ompt_callback_thread_end(
  ompt_data_t *thread_data)
{
  printf("%" PRIu64 ": ompt_event_thread_end: thread_id=%" PRIu64 "\n", ompt_get_thread_data()->value, thread_data->value);
  //printf("%" PRIu64 ": ompt_event_thread_end: thread_type=%s=%d, thread_id=%" PRIu64 "\n", ompt_get_thread_data()->value, ompt_thread_type_t_values[thread_type], thread_type, thread_data->value);
}

static void
on_ompt_event_workshare_begin(
  ompt_parallel_data_t parallel_data,
  ompt_task_data_t parent_task_data,
  void *workshare_function)
{
  printf("%" PRIu64 ": ompt_event_workshare_begin: parallel_id=%" PRIu64 ", parent_task_id=%" PRIu64 ", workshare_function=%p\n", ompt_get_thread_data()->value, parallel_data.value, parent_task_data.value, workshare_function);
}

static void
on_ompt_event_workshare_end(
  ompt_parallel_data_t parallel_data,
  ompt_task_data_t task_data)
{
  printf("%" PRIu64 ": ompt_event_workshare_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data.value, task_data.value);
}

#define register_callback(name)                               \
{                                                             \
  if (ompt_set_callback(name, (ompt_callback_t)&on_##name) == \
      ompt_has_event_no_callback)                             \
    printf("0: Could not register callback '" #name "'\n");   \
}

int ompt_initialize(
  ompt_function_lookup_t lookup,
  ompt_fns_t* fns)
{
  ompt_set_callback_t ompt_set_callback = (ompt_set_callback_t) lookup("ompt_set_callback");
  ompt_get_task_data = (ompt_get_task_data_t) lookup("ompt_get_task_data");
  ompt_get_task_frame = (ompt_get_task_frame_t) lookup("ompt_get_task_frame");
  ompt_get_thread_data = (ompt_get_thread_data_t) lookup("ompt_get_thread_data");
  ompt_get_parallel_data = (ompt_get_parallel_data_t) lookup("ompt_get_parallel_data");
  ompt_get_unique_id = (ompt_get_unique_id_t) lookup("ompt_get_unique_id");
/*
  register_callback(ompt_callback_mutex_acquire);
  register_callback(ompt_callback_mutex_acquired);
  register_callback(ompt_callback_mutex_released);
  register_callback(ompt_callback_nest_lock);
  register_callback(ompt_callback_sync_region);
  register_callback(ompt_callback_sync_region_wait);
  register_callback(ompt_event_control);
  register_callback(ompt_callback_flush);
  register_callback(ompt_callback_idle);
  register_callback(ompt_callback_implicit_task);
  register_callback(ompt_event_initial_task_begin);
  register_callback(ompt_event_initial_task_end);
  register_callback(ompt_callback_lock_init);
  register_callback(ompt_callback_lock_destroy);
  register_callback(ompt_callback_work);
  register_callback(ompt_callback_master);
*/
    register_callback(ompt_callback_parallel_begin);
    register_callback(ompt_callback_parallel_end);
/*
  register_callback(ompt_event_runtime_shutdown);
  register_callback(ompt_event_sections_begin);
  register_callback(ompt_event_sections_end);
  register_callback(ompt_callback_task_create);
  register_callback(ompt_callback_task_schedule);
  register_callback(ompt_callback_task_dependences);
  register_callback(ompt_callback_task_dependence);
  register_callback(ompt_event_wait_taskgroup_begin);
  register_callback(ompt_event_wait_taskgroup_end);
  register_callback(ompt_event_wait_taskwait_begin);
  register_callback(ompt_event_wait_taskwait_end);
    register_callback(ompt_callback_thread_begin);
    register_callback(ompt_callback_thread_end);
  register_callback(ompt_event_workshare_begin);
  register_callback(ompt_event_workshare_end);
*/
  printf("0: NULL_POINTER=%p\n", NULL);


  /* initialize data structure for adjusting frequency by multiple threads */
 /* create an array of address and associate the thread address with the cpu/core/thread id */
  /* ompt_get_thread_data()->value is the address of the thread */
  /* start the RAPL power collection */
#ifdef USERSPACE
	int i;
	ompt_num_threads = omp_get_num_threads();
	for(i = 0;i<ompt_num_threads;i++)
	{
		kernelCpuId_freq[i] = high_freq;
	}
//	timestamp = omp_get_wtime();
#endif
      ompt_time = omp_get_wtime();
      energy_measure_before();
  return 1; //success
}

void ompt_finalize(ompt_fns_t* fns)
{
  on_ompt_event_runtime_shutdown();

  /* stop the RAPL power collection and read the power/energy info */
  ompt_time = omp_get_wtime() - ompt_time;
  printf("Total time: %.2f(s)\n",ompt_time);
  energy_measure_after();
}

ompt_fns_t* ompt_start_tool(
  unsigned int omp_version,
  const char *runtime_version)
{
  static ompt_fns_t ompt_fns = {&ompt_initialize,&ompt_finalize};
  return &ompt_fns;
}
