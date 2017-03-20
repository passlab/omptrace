#include <stdio.h>
#include <inttypes.h>
#include <execinfo.h>
#include <sys/timeb.h>
#include <sched.h>

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

static double read_timer() {
    struct timeb tm;
    ftime(&tm);
    return (double) tm.time + (double) tm.millitm / 1000.0;
}

/* read timer in ms */
static double read_timer_ms() {
    struct timeb tm;
    ftime(&tm);
    return (double) tm.time * 1000.0 + (double) tm.millitm;
}

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

#ifdef OMPT_USE_LIBUNWIND
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

#else
#define print_frame(level)\
do {\
  printf("%" PRIu64 ": __builtin_frame_address(%d)=%p\n", ompt_get_thread_data()->value, level, __builtin_frame_address(level));\
} while(0)
#endif

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
on_ompt_callback_idle_spin(
        void * data) {
    int thread_id = rex_get_global_thread_num();
    thread_event_map_t *emap = get_event_map(thread_id);
    ompt_trace_record_t *record = add_trace_record(thread_id, ompt_callback_idle_spin, NULL, NULL);
    record->time_stamp = read_timer();
  //  printf("Thread: %d idle spin\n", thread_id);
}

static void
on_ompt_callback_idle_suspend(
        void * data) {
    int thread_id = rex_get_global_thread_num();
    thread_event_map_t *emap = get_event_map(thread_id);
    ompt_trace_record_t *record = add_trace_record(thread_id, ompt_callback_idle_spin, NULL, NULL);
    record->time_stamp = read_timer();
//    printf("Thread: %d idle suspend\n", thread_id);
}


static void
on_ompt_callback_idle(
        ompt_scope_endpoint_t endpoint) {
    int thread_id = rex_get_global_thread_num();
    thread_event_map_t *emap = get_event_map(thread_id);
    ompt_trace_record_t *record = add_trace_record(thread_id, ompt_callback_idle, NULL, NULL);
    record->event_id_additional = endpoint;
    record->time_stamp = read_timer();

    switch (endpoint) {
        case ompt_scope_begin: {
#ifdef PE_OPTIMIZATION_SUPPORT
            int id = sched_getcpu();
            int coreid = id % TOTAL_NUM_CORES;
            int pair_id;
            if (id < TOTAL_NUM_CORES) pair_id = id + TOTAL_NUM_CORES;
            else pair_id = id - TOTAL_NUM_CORES;

            HWTHREADS_IDLE_FLAG[id] = 0;
            /* TODO: memory fence here */
            if (HWTHREADS_IDLE_FLAG[id] == 0 && HWTHREADS_IDLE_FLAG[pair_id] == 0) {
                record->frequency = pe_adjust_freq(coreid, CORE_LOW_FREQ);
                HWTHREADS_FREQ[id] = record->frequency;
            }

            //if((id != TOTAL_NUM_CORES || id != 0) && event_maps[0].time_consumed > 0.1)
            if (id != TOTAL_NUM_CORES && id != 0) {
                /*set up the state of kernel cpu id as the beginning of idle.*/
                record->frequency = pe_adjust_freq(id, CORE_LOW_FREQ);
                /*pair id of the current in the same core*/
                /*if both kernel cpu id are at the idle state, set up both as low frequency */
            }
#endif
            mark_region_begin(thread_id);
   //         printf("Thread: %d idle begin\n", thread_id);
   //         print_frame(0);
   //         printf("frame  address: %p\n", OMPT_GET_FRAME_ADDRESS(0));
   //         printf("return address: %p\n", OMPT_GET_RETURN_ADDRESS(0));
            break;
        }
        case ompt_scope_end: {
#ifdef PE_OPTIMIZATION_SUPPORT
            int id = sched_getcpu();
            int pair_id;

            if (id != TOTAL_NUM_CORES && id != 0) {
                /*set up the state of kernel cpu id as the beginning of idle.*/
                HWTHREADS_IDLE_FLAG[id] = 1;
                record->frequency = pe_adjust_freq(id, CORE_HIGH_FREQ);
            }
#endif
            ompt_trace_record_t *begin_record = get_last_region_begin_record(emap);
            /* link two event together */
            link_records(begin_record, record);
            mark_region_end(thread_id);
            //printf("Thread: %d idle end\n", thread_id);
            break;
        }
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
    int thread_id = rex_get_global_thread_num();
    thread_event_map_t * emap = &event_maps[thread_id];
    ompt_trace_record_t *record = add_trace_record(thread_id, ompt_callback_parallel_begin, NULL, codeptr_ra);
    record->team_size = requested_team_size;
    record->ompt_id = parallel_data->value;
    //record->user_frame = OMPT_GET_FRAME_ADDRESS(0); /* the frame of the function who calls __kmpc_fork_call */
    //record->codeptr_ra = OMPT_GET_RETURN_ADDRESS(1); /* the address of the function who calls __kmpc_fork_call */
    record->user_frame = parent_task_frame->reenter_runtime_frame;
    record->codeptr_ra = codeptr_ra;

    record->time_stamp = read_timer();

#ifdef PE_MEASUREMENT_SUPPORT
    add_pe_measurement(record);
#endif
#ifdef PAPI_MEASUREMENT_SUPPORT
    add_papi_measurement_start_counters(record);
#endif
    enqueu_parallel(emap, emap->counter);
    mark_region_begin(thread_id);
    //printf("Thread: %d parallel begin: FRAME_ADDRESS: %p, LOCATION: %p, exit_runtime_frame: %p, reenter_runtime_frame: %p, codeptr_ra: %p\n",
    //thread_id, record->user_frame, record->codeptr_ra, parent_task_frame->exit_runtime_frame, parent_task_frame->reenter_runtime_frame, codeptr_ra);
    //print_ids(4);
}

static void
on_ompt_callback_parallel_end(
        ompt_data_t *parallel_data,
        ompt_task_data_t *task_data,
        ompt_invoker_t invoker,
        const void *codeptr_ra) {
    int thread_id = rex_get_global_thread_num();
    thread_event_map_t *emap = get_event_map(thread_id);
    ompt_trace_record_t *end_record = add_trace_record(thread_id, ompt_callback_parallel_end, NULL, codeptr_ra);
    end_record->time_stamp = read_timer();
    end_record->ompt_id = parallel_data->value;
    /* find the trace record for the begin_event of the parallel region */
    ompt_trace_record_t *begin_record = get_last_region_begin_record(emap);

    /* pair the begin and end event together so we create a double-link between each other */
    link_records(begin_record, end_record);

#ifdef PE_MEASUREMENT_SUPPORT
    ompt_pe_trace_record_t * end_pe_record = add_pe_measurement(end_record);
#endif
#ifdef PAPI_MEASUREMENT_SUPPORT
    ompt_papi_stop_counters(begin_record->papi_record);
#endif

#ifdef ONLINE_TRACING_PRINT
    printf("Total time: %.3f(s)", end_record->time_stamp - begin_record->time_stamp);
#ifdef PE_MEASUREMENT_SUPPORT
    ompt_pe_trace_record_t * begin_pe_record = begin_record->pe_record;
    double package_energy = energy_consumed(begin_pe_record->package, end_pe_record->package);
    double pp0_energy = energy_consumed(begin_pe_record->pp0, end_pe_record->pp0);
    double pp1_energy = energy_consumed(begin_pe_record->pp1, end_pe_record->pp1);
    double dram_energy = energy_consumed(begin_pe_record->dram, end_pe_record->dram);
    double total_energy = package_energy + dram_energy;
    printf(", Energy total (PKG+DRAM): %.6fj(package: %.6fj, PP0: %.6fj, PP1: %.6fj, and DRAM: %.6fj)", total_energy,
            package_energy, pp1_energy, pp0_energy, dram_energy);
#endif
    printf("\n");
#endif
    mark_region_end(thread_id);
    printf("Thread: %d parallel end\n", thread_id);
    list_past_parallels(emap);
/*
  if(inner_counter == iteration_ompt)
  {
	int i;
	for(i = 0;i<72;i++)
		if(i != 0 || i != TOTAL_NUM_CORES)
		{
		cpufreq_set_frequency(i,CORE_LOW_FREQ);
                event_maps[i].frequency = CORE_LOW_FREQ;
		}
  }
  else inner_counter++;
*/
}

static void
on_ompt_callback_thread_begin(
        ompt_thread_type_t thread_type,
        ompt_data_t *thread_data) {
    int thread_id = rex_get_global_thread_num();
    init_thread_event_map(thread_id, thread_data);
    thread_data->value = thread_id; //ompt_get_unique_id();
    ompt_trace_record_t *record = add_trace_record(thread_id, ompt_callback_thread_begin, NULL, NULL);
    record->time_stamp = read_timer();
    mark_region_begin(thread_id);
    //printf("%" PRIu64 ": ompt_event_thread_begin: thread_type=%s=%d, thread_id=%" PRIu64 "\n", ompt_get_thread_data()->value, ompt_thread_type_t_values[thread_type], thread_type, thread_data->value);
    //printf("Thread: %d thread begin\n", thread_id);
}

static void
on_ompt_callback_thread_end(
        ompt_data_t *thread_data) {
    int thread_id = rex_get_global_thread_num();
    thread_event_map_t *emap = get_event_map(thread_id);
    ompt_trace_record_t *end_record = add_trace_record(thread_id, ompt_callback_thread_end, NULL, NULL);
    ompt_trace_record_t *begin_record = get_last_region_begin_record(emap);

    /* pair the begin and end event together so we create a double-link between each other */
    link_records(begin_record, end_record);
//    fini_thread_event_map(thread_id);
    //printf("Thread: %d thread end\n", thread_id);
}

#define register_callback_t(name, type)                       \
do{                                                           \
  type f_##name = &on_##name;                                 \
  if (ompt_set_callback(name, (ompt_callback_t)f_##name) ==   \
      ompt_set_never)                                         \
    printf("0: Could not register callback '" #name "'\n");   \
} while(0)

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
//    register_callback(ompt_callback_idle_spin);
    register_callback(ompt_callback_idle_suspend);
    register_callback(ompt_callback_parallel_begin);
    register_callback(ompt_callback_parallel_end);
    register_callback(ompt_callback_thread_begin);
    register_callback(ompt_callback_thread_end);
#ifdef PE_OPTIMIZATION_SUPPORT
    /* check the system to find total number of hardware cores, total number of hw threads (kernel processors),
     * SMT way and mapping of hwthread id with core id
     */
    int i;
    int coreid = sched_getcpu() % TOTAL_NUM_CORES;
    int hwth2id = coreid + TOTAL_NUM_CORES; /* NOTES: this only works for 2-way hyperthreading/SMT */
    for(i = 0; i < TOTAL_NUM_CORES; i++)
    {
        if (coreid == i) {
            HWTHREADS_FREQ[i] = CORE_HIGH_FREQ;
            cpufreq_set_frequency(i, CORE_HIGH_FREQ);
        } else {
            HWTHREADS_FREQ[i] = CORE_LOW_FREQ;
            cpufreq_set_frequency(i, CORE_LOW_FREQ);
        }
    }

    for (; i<TOTAL_NUM_HWTHREADS; i++) {
        if (hwth2id == i) {
            HWTHREADS_FREQ[i] = CORE_HIGH_FREQ;
        } else {
            HWTHREADS_FREQ[i] = CORE_LOW_FREQ;
        }
    }
#endif

    epoch_begin.time_stamp = read_timer();
#ifdef PE_MEASUREMENT_SUPPORT
    init_pe_units();
    pe_measure(pe_epoch_begin.package, pe_epoch_begin.pp0, pe_epoch_begin.pp1, pe_epoch_begin.dram);
#endif
    return 1; //success
}

void ompt_finalize(ompt_fns_t *fns) {
    // on_ompt_event_runtime_shutdown();

    /* stop the RAPL power collection and read the power/energy info */
    epoch_end.time_stamp = read_timer();
#ifdef PE_MEASUREMENT_SUPPORT
    pe_measure(pe_epoch_end.package, pe_epoch_end.pp0, pe_epoch_end.pp1, pe_epoch_end.dram);
#endif

    printf("Total time: %.3f(s)", epoch_end.time_stamp - epoch_begin.time_stamp);
#ifdef PE_MEASUREMENT_SUPPORT
    double package_energy = energy_consumed(pe_epoch_begin.package, pe_epoch_end.package);
    double pp0_energy = energy_consumed(pe_epoch_begin.pp0, pe_epoch_end.pp0);
    double pp1_energy = energy_consumed(pe_epoch_begin.pp1, pe_epoch_end.pp1);
    double dram_energy = energy_consumed(pe_epoch_begin.dram, pe_epoch_end.dram);
    double total_energy = package_energy + dram_energy;
    printf(", Energy total (PKG+DRAM): %.6fj(package: %.6fj, PP0: %.6fj, PP1: %.6fj, and DRAM: %.6fj)", total_energy,
            package_energy, pp1_energy, pp0_energy, dram_energy);
#endif
    printf("\n");
}

ompt_fns_t *ompt_start_tool(
        unsigned int omp_version,
        const char *runtime_version) {
    static ompt_fns_t ompt_fns = {&ompt_initialize, &ompt_finalize};
    return &ompt_fns;
}
