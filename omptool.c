#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "omptool.h"
#include <sys/timeb.h>

double read_timer() {
    struct timeb tm;
    ftime(&tm);
    return (double) tm.time + (double) tm.millitm / 1000.0;
}

/* read timer in ms */
double read_timer_ms() {
    struct timeb tm;
    ftime(&tm);
    return (double) tm.time * 1000.0 + (double) tm.millitm;
}

/* so far we only handle max 256 threads */
thread_event_map_t event_maps[MAX_NUM_THREADS];
ompt_measurement_t total_consumed;

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
int PAPI_Events[NUM_PAPI_EVENTS]={PAPI_TOT_INS, PAPI_TOT_CYC, PAPI_L1_DCM};
#endif


/* init thread map including allocate memory for storing the trace records
 *
 */
thread_event_map_t * init_thread_event_map(int thread_id) {
    thread_event_map_t *emap = get_event_map(thread_id);
    emap->thread_id = thread_id;
    emap->counter = -1;
    emap->innermost_lexgion = -1;
    emap->lexgion_last_index = -2;
    emap->lexgion_recent = -1;
    emap->records = NULL;
    return emap;
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
ompt_trace_record_t *add_trace_record(int thread_id, int event_id, const ompt_frame_t *frame, const void *codeptr_ra) {
    thread_event_map_t *emap = get_event_map(thread_id);
    emap->counter++;
    int counter = emap->counter;
    ompt_trace_record_t *rd = get_trace_record_from_emap(emap, counter);

    rd->event_id = event_id;
    rd->uid = (((uint64_t)thread_id) << 4)  + emap->counter;
    rd->user_frame = frame;
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

/**
 * can only be called after the event_begin is added to the record
 *
 * We basically use stack for push/pop the indices
 * @param emap
 */
void push_lexgion(thread_event_map_t * emap, ompt_lexgion_t * lgp) {
    emap->innermost_lexgion++;
    emap->lexgion_stack[emap->innermost_lexgion] = lgp;
//    printf("lexgion begin: %X at %d\n", lgp, emap->innermost_lexgion);
}

ompt_lexgion_t * pop_lexgion(thread_event_map_t *emap) {
    ompt_lexgion_t * recent = emap->lexgion_stack[emap->innermost_lexgion];
//    printf("lexgion end: %X at %d\n", recent, emap->innermost_lexgion);
    emap->innermost_lexgion--;
    return recent;
}

void add_record_lexgion(ompt_lexgion_t * lgp, ompt_trace_record_t * record) {
    if (lgp->total_record == 1) { /* the first record */
        lgp->most_recent = record;
        record->next = NULL;
        //printf("add first record (%X) for lexgion (%X): %X, now total: %d\n", record, lgp, lgp->codeptr_ra, lgp->total_record);
    } else {
        record->next = lgp->most_recent;
        lgp->most_recent = record;
        //printf("add one record (%X) for lexgion (%X): %X, now total: %d\n", record, lgp, lgp->codeptr_ra, lgp->total_record);
    }

}

ompt_lexgion_t * ompt_lexgion_begin(thread_event_map_t * emap, const void * codeptr_ra) {
    ompt_lexgion_t * lgp = NULL;
    if (emap->lexgion_recent == -1) { /* the very first parallel trace record */
        emap->lexgion_recent = 0;
        emap->lexgion_last_index = 0;
        lgp = &emap->lexgions[0];
        lgp->codeptr_ra = codeptr_ra;
        lgp->most_recent = NULL;
        lgp->total_record = 1;
        //printf("lexgion_begin, first lexgion(%d, %X) (first time encountered): %X\n", 0, lgp, codeptr_ra);
        return lgp;
    }

    int i;

    /* search forward from the most recent one */
    for (i=emap->lexgion_recent; i<=emap->lexgion_last_index; i++) {
        if (emap->lexgions[i].codeptr_ra == codeptr_ra) {
            emap->lexgion_recent = i;
            lgp = &emap->lexgions[i];
            lgp->total_record++;
            //printf("lexgion_begin(%d, %X): %X, most recent record: %X\n", i, lgp, codeptr_ra, lgp->most_recent);
            return lgp;
        }
    }
    /* search from 0 to most recent one */
    for (i=0; i<emap->lexgion_recent; i++) {
        if (emap->lexgions[i].codeptr_ra == codeptr_ra) {
            emap->lexgion_recent = i;
            lgp = &emap->lexgions[i];
            lgp->total_record++;
            //printf("lexgion_begin(%d, %X): %X, most recent record: %X\n", i, lgp, codeptr_ra, lgp->most_recent);
            return lgp;
        }
    }

    /* if we could not find it */
    i = emap->lexgion_last_index;
    i++;
    if (i == MAX_SRC_PARALLELS) {
        fprintf(stderr, "Max number of parallel regions (%d) allowed in the source code reached\n", MAX_SRC_PARALLELS);
    } else {
        emap->lexgion_last_index = i;
        emap->lexgion_recent = i;
        lgp = &emap->lexgions[i];
        lgp->codeptr_ra = codeptr_ra;
        //printf("lexgion_begin(%d, %X): first time encountered %X\n", i, lgp, codeptr_ra);
        lgp->most_recent = NULL;
        lgp->total_record = 1;
    }
    return lgp;
}

ompt_lexgion_t * ompt_lexgion_end(thread_event_map_t * emap) {
    ompt_lexgion_t * lgp = (ompt_lexgion_t *)emap->lexgion_stack[emap->innermost_lexgion];
    return lgp;
}

static void print_lexgion(int count, thread_event_map_t * emap, ompt_lexgion_t * lgp) {
    printf("================================= #%d Parallel at %p (total %d executions): ============================\n",
           count, lgp->codeptr_ra, lgp->total_record);
    printf("Accumulated Stats: | ");
    ompt_measure_print_header(&lgp->accu);
    printf("                   | ");
    ompt_measure_print(&lgp->accu);
#if defined(OMPT_TRACING_SUPPORT) && defined(OMPT_MEASUREMENT_SUPPORT)
    printf("---------------------------------- Execution Records ------------------------------------------------\n");
    printf("#Record_id(team size)| ");
    ompt_measure_print_header(&lgp->accu);
    ompt_trace_record_t * record = lgp->most_recent;
    count = 1;
    while (record != NULL) {
        printf("#%d: %d(%d)\t| ", count, record->record_id, record->team_size);
        ompt_measure_print(&record->measurement);
        record = record->next;
        count++;
    }
    printf("-----------------------------------------------------------------------------------------------------\n");
#endif
    printf("==================================================================================================================\n");
}

void list_past_lexgions(thread_event_map_t * emap) {
    if (emap->lexgion_last_index < 0) return;
    int i;
    printf("==============================================================================================\n");
    printf("==============================================================================================\n");
    printf("========================= Past Parallels (Total: %d, Listed from the most recent):==================\n", emap->lexgion_last_index+1);
    printf("==============================================================================================\n");
    printf("==============================================================================================\n");

    int counter = 0;
    /* search forward from the most recent one */
    for (i=emap->lexgion_recent; i<=emap->lexgion_last_index; i++) {
        ompt_lexgion_t * lgp = &emap->lexgions[i];
        print_lexgion(++counter, emap, lgp);
    }
    /* search from 0 to most recent one */
    for (i=0; i<emap->lexgion_recent; i++) {
        ompt_lexgion_t * lgp = &emap->lexgions[i];
        print_lexgion(++counter, emap, lgp);
    }
}

#ifdef PAPI_MEASUREMENT_SUPPORT
void PAPI_overflow_handler(int EventSet, void *address, long_long overflow_vector, void *context) {
    printf("Overflow at %p! bit=0x%llx \en", address, overflow_vector);
}
#endif

void ompt_measure_global_init() {
#ifdef PE_MEASUREMENT_SUPPORT
    init_pe_units();
#endif
#ifdef PAPI_MEASUREMENT_SUPPORT
    /*papi event initilization*/
    PAPI_library_init(PAPI_VER_CURRENT);
    //PAPI_thread_init(get_global_thread_num);
    PAPI_thread_init(pthread_self);
    int total = PAPI_num_counters();
    //printf("Total %d PAPI counters\n", total);
    PAPI_start_counters(PAPI_Events, NUM_PAPI_EVENTS);

    /*Call PAPI_overflow for an event set containing the PAPI_TOT_INS event
     * setting the threshold to 100000. Use the handler defined above.
     */
    int eventSet = PAPI_NULL;
    PAPI_create_eventset(&eventSet);
    int num = PAPI_add_events(eventSet, PAPI_Events, NUM_PAPI_EVENTS);
    printf("%d events added\n", num);
    int rtval = PAPI_overflow(eventSet, PAPI_TOT_INS, 1000, 0, PAPI_overflow_handler);
    if (rtval != PAPI_OK) printf("PAPI_overflow failed: %d\n", rtval);
#endif
}

/**
 * TODO
 */
void ompt_measure_global_fini() {
#ifdef PAPI_MEASUREMENT_SUPPORT
#endif
}

void ompt_measure_init(ompt_measurement_t * me) {
    memset(me, 0, sizeof(ompt_measurement_t));
#ifdef PAPI_MEASUREMENT_SUPPORT
    /*papi event initilization*/
    me->num_papi_events = NUM_PAPI_EVENTS;
/*
    me->eventSet = PAPI_NULL;
    PAPI_create_eventset(&me->eventSet);
    int num = PAPI_add_events(me->eventSet, PAPI_Events, me->num_papi_events);
    printf("%d events added\n", num);
//    PAPI_start(me->eventSet);
*/
#endif
}

void ompt_measure_fini(ompt_measurement_t * me) {
#ifdef PAPI_MEASUREMENT_SUPPORT
    PAPI_stop(me->eventSet, NULL);
#endif
}

void ompt_measure_reset(ompt_measurement_t * me) {
#ifdef PAPI_MEASUREMENT_SUPPORT
//    PAPI_reset(me->eventSet);
#endif
}
/**
 * @param me
 */
void ompt_measure(ompt_measurement_t * me) {
    me->time_stamp = read_timer_ms();
#ifdef PE_MEASUREMENT_SUPPORT
    pe_measure(me->pe_package, me->pe_pp0, me->pe_pp1, me->pe_dram);
#endif
#ifdef PAPI_MEASUREMENT_SUPPORT
    PAPI_read_counters(me->papi_counter, NUM_PAPI_EVENTS);
   // PAPI_read(me->eventSet, me->papi_counter);
#endif
}

/**
 * perform current stop/measurement and store the difference between current measurement and the me and store the difference
 * in me
 * @param me
 */
void ompt_measure_consume(ompt_measurement_t * me) {
    me->time_stamp = read_timer_ms() - me->time_stamp;
#ifdef PE_MEASUREMENT_SUPPORT
    ompt_measurement_t current;
    pe_measure(current.pe_package, current.pe_pp0, current.pe_pp1, current.pe_dram);

    me->pe_package[0] = energy_consumed(me->pe_package, current.pe_package);
    me->pe_pp0[0] = energy_consumed(me->pe_pp0, current.pe_pp0);
    me->pe_pp1[0] = energy_consumed(me->pe_pp1, current.pe_pp1);
    me->pe_dram[0] = energy_consumed(me->pe_dram, current.pe_package);
#endif
#ifdef PAPI_MEASUREMENT_SUPPORT
    long long papi_counter[me->num_papi_events];
    //PAPI_read(me->eventSet, papi_counter);
    PAPI_read_counters(papi_counter, NUM_PAPI_EVENTS);
    int i;
    for (i=0; i<me->num_papi_events; i++)
        me->papi_counter[i] = papi_counter[i] - me->papi_counter[i];
#endif
}

void ompt_measure_diff(ompt_measurement_t * consumed, ompt_measurement_t * begin_me, ompt_measurement_t * end_me) {
    consumed->time_stamp = end_me->time_stamp - begin_me->time_stamp;
#ifdef PE_MEASUREMENT_SUPPORT

    consumed->pe_package[0] = energy_consumed(begin_me->pe_package, end_me->pe_package);
    consumed->pe_pp0[0] = energy_consumed(begin_me->pe_pp0, end_me->pe_pp0);
    consumed->pe_pp1[0] = energy_consumed(begin_me->pe_pp1, end_me->pe_pp1);
    consumed->pe_dram[0] = energy_consumed(begin_me->pe_dram, end_me->pe_package);
#endif
#ifdef PAPI_MEASUREMENT_SUPPORT
    int i;
    for (i=0; i<end_me->num_papi_events; i++)
        consumed->papi_counter[i] = end_me->papi_counter[i] - begin_me->papi_counter[i];
#endif
}

/**
 * Compare two measurement according to either performance, energy or EDP.
 * The function returns the percentage of (best - current)/best in integer
 * @param best
 * @param current
 */
int ompt_measure_compare(ompt_measurement_t * best, ompt_measurement_t * current) {
    double diff = best->time_stamp - current->time_stamp;
    return (int)(100.0 * diff/best->time_stamp);
}

void ompt_measure_accu(ompt_measurement_t * accu, ompt_measurement_t * me) {
    accu->time_stamp += me->time_stamp;
#ifdef PE_MEASUREMENT_SUPPORT
    accu->pe_package[0] += me->pe_package[0];
    accu->pe_pp0[0] += me->pe_pp0[0];
    accu->pe_pp1[0] += me->pe_pp1[0];
    accu->pe_dram[0] += me->pe_dram[0];
#endif
#ifdef PAPI_MEASUREMENT_SUPPORT
    int i;
    for (i=0; i<accu->num_papi_events; i++)
        accu->papi_counter[i] += me->papi_counter[i];
#endif
}

/**
 * print the info in certain format
 * @param me
 */
void ompt_measure_print(ompt_measurement_t * me) {
    printf("%.2f", me->time_stamp);
#ifdef PE_MEASUREMENT_SUPPORT
    double package_energy = me->pe_package[0];
    double pp0_energy = me->pe_pp0[0];
    double pp1_energy = me->pe_pp1[0];
    double dram_energy = me->pe_dram[0];
    double total_energy = package_energy + dram_energy;
    printf("\t\t%.2f\t\t%.2f\t\t%.2f\t\t%.2f\t\t\t%.2f", total_energy, package_energy, pp1_energy, pp0_energy, dram_energy);
#endif
#ifdef PAPI_MEASUREMENT_SUPPORT
    printf("\t\t");
    int i;
    for (i=0; i<me->num_papi_events; i++) {
        printf("%lld\t\t", me->papi_counter[i]);
    }
#endif
    printf("\n");
}

void ompt_measure_print_header(ompt_measurement_t * me) {
    printf("Time(ms)");
#ifdef PE_MEASUREMENT_SUPPORT
    printf("\tEnergy (j) total (PKG+DRAM): package\tPP0\t\t\tPP1\t\t\tDRAM");
#endif
#ifdef PAPI_MEASUREMENT_SUPPORT
    int i;
    int Events[NUM_PAPI_EVENTS];
    int number = NUM_PAPI_EVENTS;
//    PAPI_list_events(me->eventSet, Events, &number);
//    printf("%d PAPI events\n", number);
    char EventName[PAPI_MAX_STR_LEN];
    printf("\t\t");
    for (i=0; i<number; i++) {
        //PAPI_event_code_to_name(Events[i], EventName);
        PAPI_event_code_to_name(PAPI_Events[i], EventName);
        printf("%s\t", EventName);
    }
#endif
    printf("\n");
}
