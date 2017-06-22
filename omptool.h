#include <stdio.h>
#include <ompt.h>
#include "kmp_hack.h"

#define MAX_NUM_RECORDS 1000000
#define MAX_NUM_THREADS 512
#define MAX_NEST_DEPTH 16
#define MAX_HIST_PARALLEL 16
/* the max number of parallel regions in the original source code */
#define MAX_SRC_PARALLELS 128
#define MAX_NUM_PACKAGES 16

/**
//macro for features, now in CMakeLists.txt

//For tracing
#define OMPT_TRACING_SUPPORT 1
#define OMPT_ONLINE_TRACING_PRINT 1

//For additional measurement
#define OMPT_MEASUREMENT_SUPPORT 1
#define PAPI_MEASUREMENT_SUPPORT 1
#define PE_MEASUREMENT_SUPPORT 1

//For optimization
#define PE_OPTIMIZATION_SUPPORT 1
#define PE_OPTIMIZATION_DVFS 1
*/

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
#endif

typedef struct ompt_measurement {
    double time_stamp;
    /* the configuration, e.g. number of threads, core frequency, etc when measuring */
    unsigned long frequency;
    int requested_team_size;
    int team_size;
#ifdef PE_MEASUREMENT_SUPPORT
    /* The trace record for power and energy info. We only need record for master thread, and we do not need pe tracing
     * in every ompt event
     */
    double pe_package[MAX_NUM_PACKAGES];
    double pe_pp0[MAX_NUM_PACKAGES]; /* PP0 is core energy */
    double pe_pp1[MAX_NUM_PACKAGES]; /* PP1 is uncore energy */
    double pe_dram[MAX_NUM_PACKAGES];
    double edp;
#endif

#ifdef PAPI_MEASUREMENT_SUPPORT
    int num_papi_events;
    int eventSet;
    long long papi_counter[NUM_PAPI_EVENTS];
#endif

} ompt_measurement_t;


/* The trace record struc contains every posibble information we want to store per event
 * though not all the fields are used for any events
 * For one million records, we will need about 72Mbytes of memory to store
 * the tracing for each thread.
 */
typedef struct ompt_trace_record {
    uint64_t uid;
    ompt_id_t thread_id_inteam;
    int requested_team_size; /* user setting of team size */
    int team_size;      /* the actual team size setting by the runtime/ompt */
    int event_id;
    short event_id_additional; /* additional info about the event, e.g. begin event of a callback_idle */
    ompt_id_t graph_id;
    const void *user_frame;
    const void *codeptr_ra;
    struct ompt_trace_record *next; /* the link of the link list for all the records of the same lexical region */
    ompt_id_t target_id;

    int record_id;
    int match_record; /* index for the matching record. the match for begin_event is end and the match for end_event is begin */

    /* for the purpose of saving memory for tracing record, we should enable this macro only if we really do TRACING and
     * Measurement the same time, though it does not hurt even if we donot do */
#if defined(OMPT_TRACING_SUPPORT) && defined(OMPT_MEASUREMENT_SUPPORT)
    ompt_measurement_t measurement;
#endif
} ompt_trace_record_t;

/**
 * A lexgion (lexical region) represent a region in the source code
 * storing the lexical parallel regions encountered in the runtime */
typedef struct ompt_lexgion {
    /* we use the binary address of the lexgion as key for each lexgion, assuming that only one
     * call path of the containing function. This is the limitation since a function may be called from different site,
     * but this seems ok so far
     */
    const void *codeptr_ra;
    ompt_trace_record_t * most_recent;
    int total_record; /* total number of records, i.e. totoal number of execution of the same parallel region */

    ompt_measurement_t accu;
    ompt_measurement_t best;
    int best_counter; /* how many times the configuration for the best measurement has been used */
    ompt_measurement_t current;
} ompt_lexgion_t;

/* each thread has an object of thread_event_map that stores all the tracing record along 
 * during the execution
 */
typedef struct thread_event_map {
    int thread_id;
    ompt_data_t *thread_data;
    int counter;
    /* the stack for storing the record indices of the lexgion events.
     * Considering nested region, this has to be stack
     */
    ompt_lexgion_t *lexgion_stack[MAX_NEST_DEPTH];
    int innermost_lexgion; /* the index for the begin event of the innermost region, the top of the stack */

    ompt_lexgion_t lexgions[MAX_SRC_PARALLELS];
    int lexgion_last_index; /* the last lexgion in the lexgions array */
    int lexgion_recent; /* the most-recently used lexgion in the lexgions array */
    ompt_measurement_t thread_total;

    ompt_trace_record_t *records;
} thread_event_map_t;

#ifdef  __cplusplus
extern "C" {
#endif

/* this is the array for store all the event tracing records by all the threads */
extern thread_event_map_t event_maps[];
extern ompt_measurement_t total_consumed;

/* handy macro for get pointers to the event_map of a thread, or pointer to a trace record */
#define get_event_map(thread_id) (&event_maps[thread_id])
#define get_trace_record(thread_id, index) (&event_maps[thread_id].records[index])
#define get_trace_record_from_emap(emap, index) (&emap->records[index])
#define get_last_lexgion_record(emap) (emap->lexgion_stack[emap->innermost_lexgion]->most_recent)

/* functions for init/fini event map */
extern thread_event_map_t * init_thread_event_map(int thread_id);
extern void fini_thread_event_map(int thread_id);

/** mark in the map that the execution enters into a region (parallel region, master, single, etc)
 * can only be called when the lexgion event is added to the record
 */
extern void push_lexgion(thread_event_map_t * emap, ompt_lexgion_t * lexgion);
extern ompt_lexgion_t * pop_lexgion(thread_event_map_t * emap);
extern void list_past_lexgions(thread_event_map_t * emap);
extern ompt_lexgion_t * ompt_lexgion_begin(thread_event_map_t * emap, const void * codeptr_ra);
extern ompt_lexgion_t * ompt_lexgion_end(thread_event_map_t * emap);
extern ompt_trace_record_t * add_trace_record(int thread_id, int event_id, const ompt_frame_t *frame, const void *codeptr_ra);
extern void add_record_lexgion(ompt_lexgion_t * lgp, ompt_trace_record_t * record);
extern void link_records(ompt_trace_record_t * begin, ompt_trace_record_t * end);

/**
 * runtime instrumentation API
 */
extern void ompt_measure_global_init( );
extern void ompt_measure_global_fini( );
extern void ompt_measure_init(ompt_measurement_t * me);
extern void ompt_measure_fini(ompt_measurement_t * me);
extern void ompt_measure_reset(ompt_measurement_t * me);
extern void ompt_measure(ompt_measurement_t * me);
extern void ompt_measure_consume(ompt_measurement_t * me);
extern void ompt_measure_diff(ompt_measurement_t * consumed, ompt_measurement_t * begin_me, ompt_measurement_t * end_me);
extern void ompt_measure_accu(ompt_measurement_t * accu, ompt_measurement_t * me);
extern int ompt_measure_compare(ompt_measurement_t * best, ompt_measurement_t * current);
extern void ompt_measure_print(ompt_measurement_t * me, FILE * csv_fid);
extern void ompt_measure_print_header(ompt_measurement_t * me);

#ifdef PE_MEASUREMENT_SUPPORT
/**
 * measure energy and store in the array for each package
 */
extern void init_pe_units();
extern void pe_measure(double *package, double *pp0, double *pp1, double *dram);
extern unsigned long pe_adjust_freq(int id, unsigned long freq);
extern double energy_consumed(double *begin, double *end);
#endif

extern double read_timer();
extern double read_timer_ms();

#ifdef  __cplusplus
};
#endif
