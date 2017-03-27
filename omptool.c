#include <stdlib.h>
#include <stdio.h>
#include "omptool.h"

/* so far we only handle max 256 threads */
thread_event_map_t event_maps[MAX_NUM_THREADS];
ompt_trace_record_t epoch_begin;
ompt_trace_record_t epoch_end;

#ifdef PE_MEASUREMENT_SUPPORT
ompt_pe_trace_record_t pe_epoch_begin;
ompt_pe_trace_record_t pe_epoch_end;
#endif

#ifdef PE_OPTIMIZATION_SUPPORT
int EXTERNAL_CONTROL_KNOB; /* a 0/1 flag set from external for turning on/off frequency control */
int TOTAL_NUM_CORES = 36;
int SMT_WAY = 2;
int TOTAL_NUM_HWTHREADS = 72; /* Total number of HW threads, which is #cores * SMT-way */
unsigned long CORE_HIGH_FREQ = 2300000;
unsigned long CORE_LOW_FREQ  = 1300000;
unsigned long HWTHREADS_FREQ[72];
int HWTHREADS_IDLE_FLAG[72] = {1}; // 0 is in the beginning of idle state; 1 means the end of idle state.
#endif

#ifdef PAPI_MEASUREMENT_SUPPORT
unsigned int PAPI_Events[NUM_PAPI_EVENTS]={PAPI_TOT_INS,PAPI_TOT_CYC, PAPI_L1_DCM};
#endif
/* init thread map including allocate memory for storing the trace records
 *
 */
void init_thread_event_map(int thread_id, ompt_data_t *thread_data) {
    thread_event_map_t *emap = get_event_map(thread_id);
    emap->thread_id = thread_id;
    emap->thread_data = thread_data;
    emap->counter = -1;
    emap->innermost_region_begin = -1;
    emap->src_parallel_region_last_index = -2;
    emap->src_parallel_region_recent = -1;
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
    emap->innermost_region_begin++;
    emap->region_begin_stack[emap->innermost_region_begin] = emap->counter;
    //printf("region begin: %d\n", emap->counter );
}

ompt_trace_record_t * get_last_region_begin(thread_event_map_t *emap) {
    return &emap->records[emap->region_begin_stack[emap->innermost_region_begin]];
}

void mark_region_end(int thread_id) {
    thread_event_map_t *emap = get_event_map(thread_id);
    //printf("region end: %d\n",  emap->region_begin_stack[emap->innermost_region_begin] );
    emap->innermost_region_begin--;
}

void add_parallel_src(thread_event_map_t * emap, const void * codeptr_ra, ompt_trace_record_t * record) {
    if (emap->src_parallel_region_recent == -1) { /* the very first parallel trace record */
        emap->src_parallel_region_recent = 0;
        emap->src_parallel_region_last_index = 0;
        emap->src_parallel_regions[0].codeptr_ra = codeptr_ra;
        emap->src_parallel_regions[0].most_recent = record;
        record->next = NULL;
        return;
    }

    int i;

    /* search forward from the most recent one */
    for (i=emap->src_parallel_region_recent; i<=emap->src_parallel_region_last_index; i++) {
        if (emap->src_parallel_regions[i].codeptr_ra == codeptr_ra) {
            emap->src_parallel_region_recent = i;

            ompt_parallel_src_t * srcp = &emap->src_parallel_regions[i];
            record->next = srcp->most_recent;
            srcp->most_recent = record;
            return;
        }
    }
    /* search from 0 to most recent one */
    for (i=0; i<emap->src_parallel_region_recent; i++) {
        if (emap->src_parallel_regions[i].codeptr_ra == codeptr_ra) {
            emap->src_parallel_region_recent = i;

            ompt_parallel_src_t *srcp = &emap->src_parallel_regions[i];
            record->next = srcp->most_recent;
            srcp->most_recent = record;
            return;
        }
    }

    /* if we could not find it */
    i = emap->src_parallel_region_last_index;
    i++;
    if (i == MAX_SRC_PARALLELS) {
        sprintf(stderr, "Max number of parallel regions (%d) allowed in the source code reached\n", MAX_SRC_PARALLELS);
    } else {
        emap->src_parallel_region_last_index = i;
        emap->src_parallel_region_recent = i;
        emap->src_parallel_regions[i].codeptr_ra = codeptr_ra;
        emap->src_parallel_regions[i].most_recent = record;
        record->next = NULL;

    }
}

void list_past_parallels(thread_event_map_t * emap) {
    int i;
    printf("Past Parallels:\n");
    printf("ID(Event ID)\t\tFrame\t\tAddress\t\tTime\t\t#threads\n");
    for (i=emap->counter; i>=0; i--) {
        ompt_trace_record_t * record = &emap->records[i];
        if (record->event_id == ompt_callback_parallel_begin) {
            printf("%llu(%d)\t%p\t%p\t%.2f\t%d\n", record->ompt_id, i, record->user_frame,
                   record->codeptr_ra, record->time_stamp, record->team_size);
        }
    };
}

void print_src_parallel(ompt_parallel_src_t * srcp) {
    printf("----------------------------- Parallel at %p: -----------------------------------------\n", srcp->codeptr_ra);
    printf("ID(Event ID)\t\tFrame\t\tAddress\t\tTime\t\t#threads\n");
    printf("--------------------------------------------------------------------------------------------\n");
    ompt_trace_record_t * record = srcp->most_recent;
    while (record != NULL) {
        printf("%llu(%d)\t%p\t%p\t%.2f\t%d\n", record->ompt_id, record->record_id, record->user_frame,
               record->codeptr_ra, record->time_stamp, record->team_size);
        record = record->next;
    }
    printf("--------------------------------------------------------------------------------------------\n");
}

void list_past_src_parallels(thread_event_map_t * emap) {
    int i;
    printf("==============================================================================================\n");
    printf("========================= Past Parallels (from the most recent):==============================\n");
    printf("--------------------------------------------------------------------------------------------\n");
    if (emap->src_parallel_region_last_index < 0) return;

    /* search forward from the most recent one */
    for (i=emap->src_parallel_region_recent; i<=emap->src_parallel_region_last_index; i++) {
        ompt_parallel_src_t * srcp = &emap->src_parallel_regions[i];
        print_src_parallel(srcp);
    }
    /* search from 0 to most recent one */
    for (i=0; i<emap->src_parallel_region_recent; i++) {
        ompt_parallel_src_t * srcp = &emap->src_parallel_regions[i];
        print_src_parallel(srcp);
    }
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
    //rd->frame = frame;
    rd->codeptr_ra = codeptr_ra;
    rd->match_record = -1;
    rd->record_id = counter;
    //printf("Add trace record: %d\n", counter);
    return rd;
}

void link_records(ompt_trace_record_t * begin, ompt_trace_record_t * end) {
    begin->match_record = end->record_id;
    end->match_record = begin->record_id;
}

void set_trace_parallel_id(int thread_id, int counter, ompt_id_t parallel_id) {
    ompt_trace_record_t *rd = get_trace_record(thread_id, counter);
    rd->ompt_id = parallel_id;
}

#ifdef PE_MEASUREMENT_SUPPORT
ompt_pe_trace_record_t *add_pe_measurement(ompt_trace_record_t *record) {
    ompt_pe_trace_record_t *pe_record = (ompt_pe_trace_record_t *) malloc(sizeof(ompt_pe_trace_record_t));
    pe_measure(pe_record->package, pe_record->pp0, pe_record->pp1, pe_record->dram);
    record->pe_record = pe_record;
    return pe_record;
}
#endif

#ifdef PAPI_MEASUREMENT_SUPPORT
ompt_papi_counter_record_t * add_papi_measurement_start_counters(ompt_trace_record_t * record) {
    ompt_papi_counter_record_t *papi_record = (ompt_papi_counter_record_t *) malloc(sizeof(ompt_papi_counter_record_t));
    PAPI_start_counters((int*)PAPI_Events, NUM_PAPI_EVENTS);
    record->papi_record = papi_record;
    return papi_record;

}

void ompt_papi_stop_counters(ompt_papi_counter_record_t *papi_record) {
    PAPI_stop_counters(papi_record->papi_counter_values, NUM_PAPI_EVENTS);
}
#endif