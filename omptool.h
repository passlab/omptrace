#include <ompt.h>

#define MAX_NUM_RECORDS 1000000
#define MAX_NUM_THREADS 512
#define MAX_NEST_DEPTH 16
#define MAX_HIST_PARALLEL 16
#define MAX_NUM_PACKAGES 16

/*
//macro for features, these macro can also be set through compiler flags in Makefile
#define PE_OPTIMIZATION_SUPPORT 1
#define PE_OPTIMIZATION_DVFS 1
#define PE_MEASUREMENT_SUPPORT 1
#define PAPI_MEASUREMENT_SUPPORT 1
*/

/**
 * The trace record for power and energy info. We only need record for master thread, and we do not need pe tracing
 * in every ompt event
 */
#ifdef PE_MEASUREMENT_SUPPORT
typedef struct ompt_pe_trace_record {
    double package[MAX_NUM_PACKAGES];
    double pp0[MAX_NUM_PACKAGES]; /* PP0 is core energy */
    double pp1[MAX_NUM_PACKAGES]; /* PP1 is uncore energy */
    double dram[MAX_NUM_PACKAGES];
} ompt_pe_trace_record_t;
#endif

#ifdef PE_OPTIMIZATION_SUPPORT
#include "cpufreq.h"
//extern int iteration_ompt;
//int inner_counter = 0;
extern int EXTERNAL_CONTROL_KNOB; /* a 0/1 flag set from external for turning on/off frequency control */
extern int TOTAL_NUM_CORES;
extern int SMT_WAY;
extern int TOTAL_NUM_HWTHREADS; /* Total number of HW threads, which is #cores * SMT-way */
extern unsigned long CORE_HIGH_FREQ;
extern unsigned long CORE_LOW_FREQ;
extern unsigned long HWTHREADS_FREQ[];
extern int HWTHREADS_IDLE_FLAG[]; // 0 is in the beginning of idle state; 1 means the end of idle state.
#endif

#ifdef PAPI_MEASUREMENT_SUPPORT
#include <papi.h>
#define NUM_PAPI_EVENTS 3
extern unsigned int PAPI_Events[];
typedef struct ompt_papi_counter_record {
long long papi_counter_values[NUM_PAPI_EVENTS];
}  ompt_papi_counter_record_t;
#endif

/* The trace record struc contains every posibble information we want to store per event
 * though not all the fields are used for any events
 * For one million records, we will need about 72Mbytes of memory to store
 * the tracing for each thread.
 */
typedef struct ompt_trace_record {
    ompt_id_t ompt_id;
    ompt_id_t thread_id_inteam;
    int event_id;
    short event_id_additional; /* additional info about the event, e.g. begin event of a callback_idle */
    ompt_id_t graph_id;
    void *user_frame;
    const void *codeptr_ra;
    ompt_id_t target_id;

    int record_id;
    int match_record; /* index for the matching record. the match for begin_event is end and the match for end_event is begin */

    unsigned long frequency;
    double time_stamp;
#ifdef PE_MEASUREMENT_SUPPORT
    ompt_pe_trace_record_t *pe_record;
#endif
#ifdef PAPI_MEASUREMENT_SUPPORT
    struct ompt_papi_counter_record * papi_record;
#endif
} ompt_trace_record_t;

/* each thread has an object of thread_event_map that stores all the tracing record along 
 * during the execution
 */
typedef struct thread_event_map {
    int thread_id;
    ompt_data_t *thread_data;
    int counter;
    /* the stack for storing the record indices of the region_begin events.
     * Considering nested region, this has to be stack
     */
    int region_begin_stack[MAX_NEST_DEPTH];
    int innermost_region_begin; /* the index for the begin event of the innermost region, the top of the stack */
    int past_parallell_regions[MAX_HIST_PARALLEL]; /* the queue for storing the past MAX_HIS_PARALLEL parallel regions in time order */
    int oldest_parallel;
    int youngest_parallel;

    ompt_trace_record_t *records;
} thread_event_map_t;

/* this is the array for store all the event tracing records by all the threads */
extern thread_event_map_t event_maps[];
extern ompt_trace_record_t epoch_begin;
extern ompt_trace_record_t epoch_end;
#ifdef PE_MEASUREMENT_SUPPORT
extern ompt_pe_trace_record_t pe_epoch_begin;
extern ompt_pe_trace_record_t pe_epoch_end;
#endif

/* handy macro for get pointers to the event_map of a thread, or pointer to a trace record */
#define get_event_map(thread_id) (&event_maps[thread_id])
#define get_trace_record(thread_id, index) (&event_maps[thread_id].records[index])
#define get_trace_record_from_emap(emap, index) (&emap->records[index])
#define get_last_region_begin_record(emap) (&emap->records[emap->region_begin_stack[emap->innermost_region_begin]])

/* functions for init/fini event map */
extern void init_thread_event_map(int thread_id, ompt_data_t *thread_data);
extern void fini_thread_event_map(int thread_id);

/** mark in the map that the execution enters into a region (parallel region, master, single, etc)
 * can only be called when the region_begin event is added to the record
 */
extern void mark_region_begin(int thread_id);
extern void mark_region_end(int thread_id);
extern void enqueu_parallel(thread_event_map_t * emap, int rid);
extern void list_past_parallels(thread_event_map_t * emap);
extern ompt_trace_record_t *add_trace_record(int thread_id, int event_id, ompt_frame_t *frame, const void *codeptr_ra);
extern void link_records(ompt_trace_record_t * begin, ompt_trace_record_t * end);
extern void set_trace_parallel_id(int thread_id, int counter, ompt_id_t parallel_id);

#ifdef PE_MEASUREMENT_SUPPORT
/**
 * measure energy and store in the array for each package
 */
extern void init_pe_units();
extern void pe_measure(double *package, double *pp0, double *pp1, double *dram);
extern unsigned long pe_adjust_freq(int id, unsigned long freq);
extern double energy_consumed(double *begin, double *end);
extern ompt_pe_trace_record_t *add_pe_measurement(ompt_trace_record_t *record);
#endif

#ifdef PAPI_MEASUREMENT_SUPPORT
/**
 * PAPI counter measurement
 */
extern ompt_papi_counter_record_t * add_papi_measurement_start_counters(ompt_trace_record_t * record);
extern void ompt_papi_stop_counters(ompt_papi_counter_record_t *papi_record);
#endif
