#include <stdio.h>
#include <inttypes.h>
#include <execinfo.h>
#include <sched.h>
#include "cpufreq.h"

#ifdef OMPT_USE_LIBUNWIND
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

#include <omp.h>
#include <ompt.h>
#include <rex.h>
#include "omptool.h"

static const char *ompt_thread_type_t_values[] = {
        NULL,
        "ompt_thread_initial",
        "ompt_thread_worker",
        "ompt_thread_other"
};

static const char *ompt_task_type_t_values[] = {
        NULL,
        "ompt_task_initial",
        "ompt_task_implicit",
        "ompt_task_explicit",
        "ompt_task_target"
};

static ompt_get_task_info_t ompt_get_task_info;
static ompt_get_thread_data_t ompt_get_thread_data;
static ompt_get_parallel_info_t ompt_get_parallel_info;
static ompt_get_unique_id_t ompt_get_unique_id;

double ompt_time;
#ifdef USERSPACE
//extern int iteration_ompt;
//int inner_counter = 0;
double ompt_parallel_time;
unsigned long high_freq = 2300000;
unsigned long low_freq  = 1300000;
unsigned long kernelCpuId_freq[72];
int state_of_idle[72] = {1}; // 0 is in the beginning of idle state; 1 means the end of idle state. 
int ompt_num_threads;
int first_time = 1;// 1 is the first time for running the parallel code, 0 is not
extern thread_event_map_t event_maps[256];
#endif

static void print_ids(int level) {
    ompt_frame_t *frame;
    ompt_data_t *parallel_data;
    ompt_data_t *task_data;
    int exists_parallel = ompt_get_parallel_info(level, &parallel_data, NULL);
    int exists_task = ompt_get_task_info(level, NULL, &task_data, &frame, NULL, NULL);
    if (frame)
        printf("%" PRIu64 ": level %d: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", exit_frame=%p, reenter_frame=%p\n",
               ompt_get_thread_data()->value, level, exists_parallel ? parallel_data->value : 0,
               exists_task ? task_data->value : 0, frame->exit_runtime_frame, frame->reenter_runtime_frame);
    else
        printf("%" PRIu64 ": level %d: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", frame=%p\n",
               ompt_get_thread_data()->value, level, exists_parallel ? parallel_data->value : 0,
               exists_task ? task_data->value : 0, frame);
}

/*
#define print_frame(level)\
do {\
  unw_cursor_t cursor;\
  unw_context_t uc;\
  unw_word_t fp;\
  unw_getcontext(&uc);\
  unw_init_local(&cursor, &uc);\
  int tmp_level = level;\
  unw_get_reg(&cursor, UNW_REG_SP, &fp);\
  printf("callback %p\n", (void*)fp);\
  while (tmp_level > 0 && unw_step(&cursor) > 0)\
  {\
    unw_get_reg(&cursor, UNW_REG_SP, &fp);\
    printf("callback %p\n", (void*)fp);\
    tmp_level--;\
  }\
  if(tmp_level == 0)\
    printf("%" PRIu64 ": __builtin_frame_address(%d)=%p\n", ompt_get_thread_data()->value, level, (void*)fp);\
  else\
    printf("%" PRIu64 ": __builtin_frame_address(%d)=%p\n", ompt_get_thread_data()->value, level, NULL);\
} while(0)

#define print_frame(level)\
do {\
  printf("%" PRIu64 ": __builtin_frame_address(%d)=%p\n", ompt_get_thread_data()->value, level, __builtin_frame_address(level));\
} while(0)
*/

static void print_current_address() {
    int real_level = 2;
    void *array[real_level];
    size_t size;
    void *address;

    size = backtrace(array, real_level);
    if (size == real_level)
        address = array[real_level - 1] - 4;
    else
        address = NULL;
    printf("%" PRIu64 ": current_address=%p\n", ompt_get_thread_data()->value, address);
}

static void
on_ompt_callback_idle(
        ompt_scope_endpoint_t endpoint) {
#ifdef USERSPACE
    int id = sched_getcpu();
    int pair_id;
#endif
    switch (endpoint) {
        case ompt_scope_begin:
            //printf("%" PRIu64 ": ompt_event_idle_begin:\n", ompt_get_thread_data()->value);
            //printf("%" PRIu64 ": ompt_event_idle_begin: thread_id=%" PRIu64 "\n", ompt_get_thread_data()->value, thread_data.value);
#ifdef USERSPACE
            //if((id != 36 || id != 0) && event_maps[0].time_consumed > 0.1)
            if(id != 36 || id != 0)
            {
              /*set up the state of kernel cpu id as the beginning of idle.*/
              state_of_idle[id] = 0;
              /*pair id of the current in the same core*/
              if(id<36) pair_id = id+36;
              else pair_id = id-36;
              /*if both kernel cpu id are at the idle state, set up both as low frequency */
                  if(state_of_idle[id] == 0 && state_of_idle[pair_id] == 0)
                  {
                      cpufreq_set_frequency(id,low_freq);
                      event_maps[id].records[0].frequency = low_freq;
                      cpufreq_set_frequency(pair_id,low_freq);
                      event_maps[pair_id].records[0].frequency = low_freq;
                  }
            }
#endif
            break;
        case ompt_scope_end:
            //printf("%" PRIu64 ": ompt_event_idle_end:\n", ompt_get_thread_data()->value);
            //printf("%" PRIu64 ": ompt_event_idle_end: thread_id=%" PRIu64 "\n", ompt_get_thread_data()->value, thread_data.value);
#ifdef USERSPACE
            //if((id != 36 || id != 0) && event_maps[0].time_consumed > 0.1)
            if(id != 36 || id != 0)
            {
              cpufreq_set_frequency(id,high_freq);
              event_maps[id].records[0].frequency = high_freq;
              state_of_idle[id] = 1;
            }
#endif
            break;
    }
}

static void
on_ompt_callback_parallel_begin(
        ompt_data_t *parent_task_data,
        const ompt_frame_t *parent_task_frame,
        ompt_data_t *parallel_data,
        uint32_t requested_team_size,
//  uint32_t actual_team_size,
        ompt_invoker_t invoker,
        const void *codeptr_ra) {
    parallel_data->value = ompt_get_unique_id();
    ompt_id_t thread_id = rex_get_global_thread_num();
    ompt_trace_record_t *record = add_trace_record(thread_id, ompt_callback_parallel_begin, NULL, codeptr_ra);
    record->parallel_id = parallel_data->value;
    record->time_stamp = omp_get_wtime();
    mark_region_begin(thread_id);
    add_pe_measurement(record);
    //print_ids(4);
}

static void
on_ompt_callback_parallel_end(
        ompt_data_t *parallel_data,
        ompt_task_data_t *task_data,
        ompt_invoker_t invoker,
        const void *codeptr_ra) {
    ompt_id_t thread_id = rex_get_global_thread_num();
    thread_event_map_t *emap = get_event_map(thread_id);
    ompt_trace_record_t *end_record = add_trace_record(thread_id, ompt_callback_parallel_end, NULL, codeptr_ra);
    end_record->time_stamp = omp_get_wtime();
    end_record->parallel_id = parallel_data->value;
    add_pe_measurement(end_record);

    /* find the trace record for the begin_event of the parallel region */
    ompt_trace_record_t *begin_record = get_trace_record(thread_id, emap->last_region_begin);

    double energy = energy_consumed(begin_record->pe_record->package, end_record->pe_record->package);
    printf("Energy_consumed:%.6fj, Time elasped: %fs\n", energy, end_record->time_stamp - begin_record->time_stamp);

    mark_region_end(thread_id);

/*
  if(inner_counter == iteration_ompt)
  {
	int i;
	for(i = 0;i<72;i++)
		if(i != 0 || i != 36)
		{
		cpufreq_set_frequency(i,low_freq);
                event_maps[i].frequency = low_freq;
		}
  }
  else inner_counter++;
*/
}

static void
on_ompt_callback_thread_begin(
        ompt_thread_type_t thread_type,
        ompt_data_t *thread_data) {
    ompt_id_t thread_id = rex_get_global_thread_num();
    init_thread_event_map(thread_id, thread_data);
    thread_data->value = ompt_get_unique_id();
    //printf("%" PRIu64 ": ompt_event_thread_begin: thread_type=%s=%d, thread_id=%" PRIu64 "\n", ompt_get_thread_data()->value, ompt_thread_type_t_values[thread_type], thread_type, thread_data->value);
}

static void
on_ompt_callback_thread_end(
        ompt_data_t *thread_data) {
    ompt_id_t thread_id = rex_get_global_thread_num();
    fini_thread_event_map(thread_id);
    //printf("%" PRIu64 ": ompt_event_thread_end: thread_id=%" PRIu64 "\n", ompt_get_thread_data()->value, thread_data->value);
    //printf("%" PRIu64 ": ompt_event_thread_end: thread_type=%s=%d, thread_id=%" PRIu64 "\n", ompt_get_thread_data()->value, ompt_thread_type_t_values[thread_type], thread_type, thread_data->value);
}

#define register_callback_t(name, type)                       \
do{                                                           \
  type f_##name = &on_##name;                                 \
  if (ompt_set_callback(name, (ompt_callback_t)f_##name) ==   \
      ompt_set_never)                                         \
    printf("0: Could not register callback '" #name "'\n");   \
}while(0)

#define register_callback(name) register_callback_t(name, name##_t)

int ompt_initialize(
        ompt_function_lookup_t lookup,
        ompt_fns_t *fns) {
    ompt_set_callback_t ompt_set_callback = (ompt_set_callback_t) lookup("ompt_set_callback");
    ompt_get_task_info = (ompt_get_task_info_t) lookup("ompt_get_task_info");
    ompt_get_thread_data = (ompt_get_thread_data_t) lookup("ompt_get_thread_data");
    ompt_get_parallel_info = (ompt_get_parallel_info_t) lookup("ompt_get_parallel_info");
    ompt_get_unique_id = (ompt_get_unique_id_t) lookup("ompt_get_unique_id");
    register_callback(ompt_callback_idle);
    register_callback(ompt_callback_parallel_begin);
    register_callback(ompt_callback_parallel_end);
    register_callback(ompt_callback_thread_begin);
    register_callback(ompt_callback_thread_end);
#ifdef USERSPACE
    int i;
    //ompt_num_threads = omp_get_num_threads();
    /*for(i = 0;i<ompt_num_threads;i++)
    {
            kernelCpuId_freq[i] = high_freq;
    }
    for(i = 0;i<72;i++)
            if(i != 0 || i != 36)
            {
            cpufreq_set_frequency(i,low_freq);
            event_maps[i].frequency = low_freq;
            }
    else{
    cpufreq_set_frequency(i,high_freq);
            event_maps[i].frequency = high_freq;
    }
*/
#endif

    init_measurement();
    epoch_begin.time_stamp = omp_get_wtime();
    pe_measure(pe_epoch_begin.package, pe_epoch_begin.pp0, pe_epoch_begin.pp1, pe_epoch_begin.dram);
    return 1; //success
}

void ompt_finalize(ompt_fns_t *fns) {
    // on_ompt_event_runtime_shutdown();

    /* stop the RAPL power collection and read the power/energy info */
    epoch_end.time_stamp = omp_get_wtime();
    pe_measure(pe_epoch_end.package, pe_epoch_end.pp0, pe_epoch_end.pp1, pe_epoch_end.dram);
    printf("Total time: %.2f(s)\n", epoch_end.time_stamp - epoch_begin.time_stamp);
    printf("\t\tPackage energy: %.6fJ\n",
           energy_consumed(pe_epoch_begin.package, pe_epoch_end.package));
    printf("\t\tPowerPlane0 (cores): %.6fJ\n",
           energy_consumed(pe_epoch_begin.pp0, pe_epoch_end.pp0));
    printf("\t\tPowerPlane1 (if avail): %.6f J\n",
           energy_consumed(pe_epoch_begin.pp1, pe_epoch_end.pp1));
    printf("\t\tDRAM: %.6fJ\n",
           energy_consumed(pe_epoch_begin.dram, pe_epoch_end.dram));
}

ompt_fns_t *ompt_start_tool(
        unsigned int omp_version,
        const char *runtime_version) {
    static ompt_fns_t ompt_fns = {&ompt_initialize, &ompt_finalize};
    return &ompt_fns;
}
