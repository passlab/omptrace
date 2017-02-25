#include "ompt.h"

#define MAX_NUM_RECORDS 1000000
#define MAX_NUM_THREADS 512
#define MAX_NEST_DEPTH 16
#define MAX_NUM_PACKAGES 16

/**
 * The trace record for power and energy info. We only need record for master thread, and we do not need pe tracing
 * in every ompt event
 */
typedef struct ompt_pe_trace_record {
    double package[MAX_NUM_PACKAGES];
    double pp0[MAX_NUM_PACKAGES]; /* PP0 is core energy */
    double pp1[MAX_NUM_PACKAGES]; /* PP1 is uncore energy */
    double dram[MAX_NUM_PACKAGES];
} ompt_pe_trace_record_t;

/* The trace record struc contains every posibble information we want to store per event
 * though not all the fields are used for any events
 * For one million records, we will need about 72Mbytes of memory to store
 * the tracing for each thread.
 */
typedef struct ompt_trace_record {
    ompt_id_t parallel_id;
    ompt_id_t thread_id_inteam;
    int event_id;
    ompt_id_t graph_id;
    ompt_frame_t *frame;
    const void *codeptr_ra;
    ompt_id_t target_id;

    unsigned long frequency;
    double time_stamp;
    ompt_pe_trace_record_t * pe_record;
} ompt_trace_record_t;

/* each thread has an object of thread_event_map that stores all the tracing record along 
 * during the execution
 */
typedef struct thread_event_map {
    ompt_id_t thread_id;
    ompt_data_t *thread_data;
    int counter;
    /* the stack for storing the record indices of the region_begin events.
     * Considering nested region, this has to be stack
     */
    int region_begin_stack[MAX_NEST_DEPTH];
    int last_region_begin; /* the index for the begin event of the innermost region, the top of the stack */
    ompt_trace_record_t *records;
} thread_event_map_t;

/* this is the array for store all the event tracing records by all the threads */
extern thread_event_map_t event_maps[];
extern ompt_trace_record_t epoch_begin;
extern ompt_trace_record_t epoch_end;
extern ompt_pe_trace_record_t pe_epoch_begin;
extern ompt_pe_trace_record_t pe_epoch_end;


/* handy macro for get pointers to the event_map of a thread, or pointer to a trace record */
#define get_event_map(thread_id) (&event_maps[thread_id])
#define get_trace_record(thread_id, index) (&event_maps[thread_id].records[index])
#define get_trace_record_from_emap(emap, index) (&emap->records[index])

/* functions for init/fini event map */
extern void init_thread_event_map(int thread_id, ompt_data_t * thread_data);
extern void fini_thread_event_map(int thread_id);

/** mark in the map that the execution enters into a region (parallel region, master, single, etc)
 * can only be called when the region_begin event is added to the record
 */
extern void mark_region_begin(int thread_id);
extern void mark_region_end(int thread_id);

extern ompt_trace_record_t *add_trace_record(int thread_id, int event_id, ompt_frame_t *frame, const void *codeptr_ra);
extern void set_trace_parallel_id(int thread_id, int counter, ompt_id_t parallel_id);

extern void init_measurement();
extern void init_pe_units();

/**
 * measure energy and store in the array for each package
 */
extern void pe_measure(double *package, double *pp0, double *pp1, double *dram);
extern double energy_consumed(double * begin, double * end);
extern ompt_pe_trace_record_t * add_pe_measurement(ompt_trace_record_t * record);

