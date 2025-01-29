#include <stdio.h>
#include "ompt.h"
#include "kmp_hack.h"

#define MAX_NUM_RECORDS 10000000
#define MAX_NUM_THREADS 64
#define MAX_NEST_DEPTH 16
#define MAX_HIST_PARALLEL 16
/* the max number of parallel regions in the original source code */
#define MAX_SRC_PARALLELS 64

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
 *
 * For each event (short or long), there are normally two record, begin record and end record, linked together through
 * the @match_record field. The @endpoint field tells whether it is a begin or end record.
 *
 * For measurement support enabled, the measured data (exe time, PAPI, power) info are stored in the end record.
 */

/* padding to eliminate false sharing in an array that will be accessed by multiple thread (one element per thread) */
#define OFFSET4FS 16
typedef struct ompt_trace_record {
    int thread_id;
    ompt_id_t thread_id_inteam;
    int requested_team_size; /* user setting of team size */
    int team_size;      /* the actual team size setting by the runtime/ompt */
    int event;
    int kind;
    ompt_scope_endpoint_t endpoint; /* begin or end */
    const void *user_frame;
    const void *codeptr_ra;  /* for *_begin record, codeptr_ra is the same as the lgp codeptr_ra,
                              * for *_end record, it is the address of the end of lexgion, ideally */
    struct ompt_lexgion *lgp; /* the lexgion of this record */
    struct ompt_trace_record *parent; /* the record for the lexgion that enclose the lexgion for this record */
    struct ompt_trace_record *task; /* the record for the enclosing task of this record */
    struct ompt_trace_record *parallel_record; /* The record for the innermost parellel lexgion that encloses this record */

    struct ompt_trace_record *next; /* the link of the link list for all the begin/start records of the same lexical region */
    ompt_id_t target_id;

    int record_id;
    int match_record; /* index for the matching record. the match for begin_event is end and the match for end_event is begin */

    /* for the purpose of saving memory for tracing record, we should enable this macro only if we really do TRACING and
     * Measurement the same time, though it does not hurt even if we donot do */
#if defined(OMPT_TRACING_SUPPORT) && defined(OMPT_MEASUREMENT_SUPPORT)
    ompt_measurement_t measurement;
#endif
    struct ompt_trace_record **parallel_implicit_tasks; /* an array for parallel record to store the pointers to all the records of implicit tasks */
    struct ompt_trace_record **parallel_implicit_barrier_sync; /* the records for sync_barrier_region of all the team threads */
    struct ompt_trace_record **parallel_implicit_barrier_wait; /* the records for sync_barrier_wait of all the team threads */
} ompt_trace_record_t;

/**
 * A lexgion (lexical region) represent a region in the source code
 * storing the lexical parallel regions encountered in the runtime.
 * A lexgion should be identified by the codeptr_ra and the type field together. codeptr_ra is the binary address of the
 * lexgion and type is the type of region. The reasons we need type for identify a lexgion are:
 * 1). OpenMP combined and composite construct, e.g. parallel for
 * 2). implicit barrier, e.g. parallel.
 * Becasue of that, the events for those constructs may use the same codeptr_ra for the callback, thus we need further
 * check the type so we know whether we need to create two different lexgion objects
 * */
typedef struct ompt_lexgion {
    /* we use the binary address of the lexgion as key for each lexgion, assuming that only one
     * call path of the containing function. This is the limitation since a function may be called from different site,
     * but this seems ok so far
     */
    int type; /* the type of a lexgion: parallel, master, singer, barrier, task, section, etc. we use trace record event id for this type */
    const void *codeptr_ra;
    ompt_trace_record_t * most_recent;
    int total_record; /* total number of records, i.e. totoal number of execution of the same parallel region */

    struct ompt_lexgion * parent; /* the immediately enclosing/parent lexgion */
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
    ompt_lexgion_t *lexgion_stack; /* the top of the stack through out execution, linked via parent field */
    ompt_trace_record_t * record_stack; /* the top of the stack through out execution, linked through parent field */

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
extern volatile int num_threads;
extern ompt_measurement_t total_consumed;

/* handy macro for get pointers to the event_map of a thread, or pointer to a trace record */
#define get_event_map(thread_id) (&event_maps[thread_id])
#define get_trace_record(thread_id, index) (&event_maps[thread_id].records[index])
#define get_trace_record_from_emap(emap, index) (&emap->records[index])
#define get_last_lexgion_record(emap) (emap->lexgion_stack[emap->innermost_lexgion]->most_recent)

#define top_record(emap) (emap->record_stack)
#define top_lexgion(emap) (emap->lexgion_stack)

/* functions for init/fini event map */
extern thread_event_map_t * init_thread_event_map(int thread_id);
extern void fini_thread_event_map(int thread_id);

/** mark in the map that the execution enters into a region (parallel region, master, single, etc)
 * can only be called when the lexgion event is added to the record
 */
extern void push_lexgion(thread_event_map_t * emap, ompt_lexgion_t * lexgion);
extern ompt_lexgion_t * pop_lexgion(thread_event_map_t * emap);
extern void push_record(thread_event_map_t * emap, ompt_trace_record_t * record);
extern ompt_trace_record_t * pop_record(thread_event_map_t * emap);
extern void list_parallel_lexgions(thread_event_map_t *emap);
extern ompt_lexgion_t *ompt_lexgion_begin(thread_event_map_t *emap, int type, const void *codeptr_ra);
extern ompt_trace_record_t *
add_trace_record_begin(thread_event_map_t *emap, int event_id, const ompt_frame_t *frame, ompt_lexgion_t *lgp,
                       ompt_trace_record_t *task_record, ompt_trace_record_t *parallel_record);
extern ompt_trace_record_t *add_trace_record_end(thread_event_map_t *emap, int event_id, const void *codeptr_ra);

extern void tribute_record_lexgion(ompt_lexgion_t *lgp, ompt_trace_record_t *record);

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
extern void ompt_event_maps_to_graphml(thread_event_map_t* maps);

extern double read_timer();
extern double read_timer_ms();

#ifdef  __cplusplus
};
#endif
