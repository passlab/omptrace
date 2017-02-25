#include <stdlib.h>
#include "omptool.h"

/* so far we only handle max 256 threads */
thread_event_map_t event_maps[MAX_NUM_THREADS];
ompt_trace_record_t epoch_begin;
ompt_trace_record_t epoch_end;
ompt_pe_trace_record_t pe_epoch_begin;
ompt_pe_trace_record_t pe_epoch_end;

void init_measurement() {
    /* init power/energy measurement */
    init_pe_units();
    /* PAPI init here */
}

/* init thread map including allocate memory for storing the trace records
 *
 */
void init_thread_event_map(int thread_id, ompt_data_t *thread_data) {
    thread_event_map_t *emap = get_event_map(thread_id);
    emap->thread_id = thread_id;
    emap->thread_data = thread_data;
    emap->counter = -1;
    emap->last_region_begin = -1;
    emap->records = (ompt_trace_record_t *) malloc(sizeof(ompt_trace_record_t) * MAX_NUM_RECORDS);
}

/**
 * can only be called after the event_begin is added to the record
 *
 * We basically use stack for push/pop the indices
 * @param thread_id
 */
void mark_region_begin(int thread_id) {
    thread_event_map_t *emap = get_event_map(thread_id);
    emap->last_region_begin++;
    emap->region_begin_stack[emap->last_region_begin] = emap->counter;
}

void mark_region_end(int thread_id) {
    thread_event_map_t *emap = get_event_map(thread_id);
    emap->last_region_begin--;
}

/** fini thread event map including deallocate memory for trace records
 *
 */
void fini_thread_event_map(int thread_id) {
    free(get_event_map(thread_id)->records);
}

/**
 * Add a trace record to a thread's event map and return the record
 */
ompt_trace_record_t *add_trace_record(int thread_id, int event_id, ompt_frame_t *frame, const void *codeptr_ra) {
    thread_event_map_t *emap = get_event_map(thread_id);
    emap->counter++;
    int counter = emap->counter;
    ompt_trace_record_t *rd = get_trace_record_from_emap(emap, counter);

    rd->event_id = event_id;
    rd->frame = frame;
    rd->codeptr_ra = codeptr_ra;
    return rd;
}

void set_trace_parallel_id(int thread_id, int counter, ompt_id_t parallel_id) {
    ompt_trace_record_t *rd = get_trace_record(thread_id, counter);
    rd->parallel_id = parallel_id;
}

ompt_pe_trace_record_t *add_pe_measurement(ompt_trace_record_t *record) {
    ompt_pe_trace_record_t *pe_record = (ompt_pe_trace_record_t *) malloc(sizeof(ompt_pe_trace_record_t));
    pe_measure(pe_record->package, pe_record->pp0, pe_record->pp1, pe_record->dram);
    return pe_record;
}

