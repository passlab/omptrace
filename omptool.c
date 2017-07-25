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
volatile int num_threads = 0; /* this counter is not guaranteed to provide the
 * actual exact number of threads until after it become useless :-) */
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
    emap->lexgion_last_index = -2;
    emap->lexgion_recent = -1;
    emap->records = NULL;
    emap->lexgion_stack = NULL;
    emap->record_stack = NULL;

    /* this is data racing, but acceptable, since we donot rely on this counter to know the
     * total number of threads */
    num_threads ++;

    return emap;
}

/** fini thread event map including deallocate memory for trace records
 *
 */
void fini_thread_event_map(int thread_id) {
    free(get_event_map(thread_id)->records);
}

void tribute_record_lexgion(ompt_lexgion_t *lgp, ompt_trace_record_t *rd) {
    /* add record to the lexgion */
    if (lgp->total_record == 1) { /* the first record */
        lgp->most_recent = rd;
        rd->next = NULL;
        //printf("add first record (%X) for lexgion (%X): %X, now total: %d\n", record, lgp, lgp->codeptr_ra, lgp->total_record);
    } else {
        rd->next = lgp->most_recent;
        lgp->most_recent = rd;
        //printf("add one record (%X) for lexgion (%X): %X, now total: %d\n", record, lgp, lgp->codeptr_ra, lgp->total_record);
    }
}

/**
 * add a begin event trace record
 * @param emap
 * @param event_id
 * @param frame
 * @param lgp: the lexgion pointer
 * @param parallel_lgrecord: the trace record that launches the lexgion this record belongs to. If NULL, this record is the
 *                  launching record
 * @return
 */
ompt_trace_record_t *add_trace_record_begin(thread_event_map_t * emap, int event_id, const ompt_frame_t *frame,
                                            ompt_lexgion_t *lgp, ompt_trace_record_t *parallel_lgrecord) {
    emap->counter++;
    int counter = emap->counter;
    ompt_trace_record_t *rd = get_trace_record_from_emap(emap, counter);
    rd->record_id = counter;
    rd->event = event_id;
    rd->lgp = lgp;
    rd->parallel_lgrecord = parallel_lgrecord;

    rd->uid = (((uint64_t) emap->thread_id) << 4) + emap->counter;
    rd->match_record = -1;
    //printf("Add trace record: %d\n", counter);

    rd->endpoint = ompt_scope_begin;
    rd->user_frame = frame;

    /* tribute to its own lexgion */
    tribute_record_lexgion(lgp, rd);

    /* push to the record stack */
    push_record(emap, rd);

    return rd;
}

ompt_trace_record_t *add_trace_record_end(thread_event_map_t *emap, int event_id, const void *codeptr_ra) {
    emap->counter++;
    int counter = emap->counter;
    ompt_trace_record_t *rd = get_trace_record_from_emap(emap, counter);
    rd->record_id = counter;
    rd->event = event_id;
    rd->endpoint = ompt_scope_end;
    rd->next = NULL;
    rd->codeptr_ra = codeptr_ra;

    ompt_trace_record_t * begin_record = top_record(emap);
    rd->lgp = begin_record->lgp;
    rd->parallel_lgrecord = begin_record->parallel_lgrecord;

    rd->uid = (((uint64_t)emap->thread_id) << 4)  + emap->counter;
    //printf("Add trace record: %d\n", counter);
    /* pop record stack */
    pop_record(emap);

    /* link the begin and end record so we can easily find each other */
    begin_record->match_record = counter;
    rd->match_record = begin_record->record_id;

    return rd;
}

void push_lexgion(thread_event_map_t * emap, ompt_lexgion_t * lgp) {
    lgp->parent = emap->lexgion_stack;
    emap->lexgion_stack = lgp;
}

ompt_lexgion_t * pop_lexgion(thread_event_map_t *emap) {
    ompt_lexgion_t * top = emap->lexgion_stack;
    emap->lexgion_stack = top->parent;
    return top;
}

void push_record(thread_event_map_t * emap, ompt_trace_record_t * record) {
    record->parent = emap->record_stack;
    emap->record_stack = record;
}

ompt_trace_record_t * pop_record(thread_event_map_t * emap) {
    ompt_trace_record_t * top = emap->record_stack;
    emap->record_stack = top->parent;
    return top;
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
        push_lexgion(emap, lgp);
        //printf("lexgion_begin, first lexgion(%d, %X) (first time encountered): %X\n", 0, lgp, codeptr_ra);
        return lgp;
    }

    int i;

    /* search forward from the most recent one */
    for (i=emap->lexgion_recent; i<=emap->lexgion_last_index; i++) {
        if (emap->lexgions[i].codeptr_ra == codeptr_ra) {
            emap->lexgion_recent = i; /* cache it for future search */
            lgp = &emap->lexgions[i];
            lgp->total_record++;
            //printf("lexgion_begin(%d, %X): %X, most recent record: %X\n", i, lgp, codeptr_ra, lgp->most_recent);
            push_lexgion(emap, lgp);
            return lgp;
        }
    }
    /* search from 0 to most recent one */
    for (i=0; i<emap->lexgion_recent; i++) {
        if (emap->lexgions[i].codeptr_ra == codeptr_ra) {
            emap->lexgion_recent = i;
            lgp = &emap->lexgions[i];
            lgp->total_record++;
            push_lexgion(emap, lgp);
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
    push_lexgion(emap, lgp);
    return lgp;
}

#define OMPT_CSV_OUTPUT 1

static void print_lexgion(int count, thread_event_map_t * emap, ompt_lexgion_t * lgp) {
    printf("================================= #%d Parallel at %p (total %d executions): ============================\n",
           count, lgp->codeptr_ra, lgp->total_record);
    printf("Accumulated Stats: | ");
    ompt_measure_print_header(&lgp->accu);
    printf("                   | ");
    FILE* csv_fid = NULL;
    ompt_measure_print(&lgp->accu, csv_fid);

#if defined(OMPT_CSV_OUTPUT)
    char filename[128];
    sprintf(filename, "%d_%p_%d.csv", count, lgp->codeptr_ra, lgp->total_record);
    csv_fid = fopen(filename, "w+");
#endif
#if defined(OMPT_TRACING_SUPPORT) && defined(OMPT_MEASUREMENT_SUPPORT)
    printf("---------------------------------- Execution Records ------------------------------------------------\n");
    printf("#Record_id(team size)| ");
    ompt_measure_print_header(&lgp->accu);
    ompt_trace_record_t * record = lgp->most_recent;
    count = 1;
    while (record != NULL) {
        printf("#%d: %d(%d)\t| ", count, record->record_id, record->team_size);
        /* measurement data is stored in the end_record, which can be accessed through the match_record index */
        ompt_trace_record_t * end_record = &emap->records[record->match_record];
        ompt_measure_print(&end_record->measurement, csv_fid);
        record = record->next;
        count++;
    }
#if defined(OMPT_CSV_OUTPUT)
    fclose(csv_fid);
#endif
    printf("-----------------------------------------------------------------------------------------------------\n");
#endif
    printf("==================================================================================================================\n");
}

void list_parallel_lexgions(thread_event_map_t *emap) {
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
        if (lgp->most_recent->event == ompt_callback_parallel_begin)
            print_lexgion(++counter, emap, lgp);
    }
    /* search from 0 to most recent one */
    for (i=0; i<emap->lexgion_recent; i++) {
        ompt_lexgion_t * lgp = &emap->lexgions[i];
        if (lgp->most_recent->event == ompt_callback_parallel_begin)
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
    //printf("%d PAPI events added\n", num);
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

#define PAPI_CPI_PRINT 1
/**
 * print the info in certain format
 * @param me
 */
void ompt_measure_print(ompt_measurement_t * me, FILE* csvfile) {
    printf("%.2f", me->time_stamp);
#if defined(OMPT_CSV_OUTPUT)
    if (csvfile != NULL) fprintf(csvfile, "%.2f,", me->time_stamp);
#endif
#ifdef PE_MEASUREMENT_SUPPORT
    double package_energy = me->pe_package[0];
    double pp0_energy = me->pe_pp0[0];
    double pp1_energy = me->pe_pp1[0];
    double dram_energy = me->pe_dram[0];
    double total_energy = package_energy + dram_energy;
    printf("\t\t%.2f\t\t%.2f\t\t%.2f\t\t%.2f\t\t\t%.2f", total_energy, package_energy, pp1_energy, pp0_energy, dram_energy);
#if defined(OMPT_CSV_OUTPUT)
    if (csvfile != NULL) fprintf(csvfile, "%.2f,%.2f,%.2f,%.2f,%.2f,", total_energy, package_energy, pp1_energy, pp0_energy, dram_energy);
#endif
#endif
#ifdef PAPI_MEASUREMENT_SUPPORT
    printf("\t\t");
#ifdef PAPI_CPI_PRINT
    printf("%.3f\t\t", ((double)me->papi_counter[1])/((double)me->papi_counter[0]));
#if defined(OMPT_CSV_OUTPUT)
    if (csvfile != NULL) fprintf(csvfile, "%.3f,", ((double)me->papi_counter[1])/((double)me->papi_counter[0]));
#endif
#endif
    int i;
    for (i=0; i<me->num_papi_events; i++) {
        printf("%lld\t\t", me->papi_counter[i]);
#if defined(OMPT_CSV_OUTPUT)
        if (csvfile != NULL) fprintf(csvfile, "%lld,", me->papi_counter[i]);
#endif
    }
#endif
#if defined(OMPT_CSV_OUTPUT)
    if (csvfile != NULL) fprintf(csvfile, "\n");
#endif
    printf("\n");
}

void ompt_measure_print_header(ompt_measurement_t * me) {
    printf("Time(ms)");
#ifdef PE_MEASUREMENT_SUPPORT
    printf("\tEnergy (j) total (PKG+DRAM): package\tPP0\t\t\tPP1\t\t\tDRAM");
#endif
#ifdef PAPI_MEASUREMENT_SUPPORT
    printf("\t\t");
#ifdef PAPI_CPI_PRINT
    printf("PAPI_CPI\t");
#endif
    int i;
    int Events[NUM_PAPI_EVENTS];
    int number = NUM_PAPI_EVENTS;
//    PAPI_list_events(me->eventSet, Events, &number);
//    printf("%d PAPI events\n", number);
    char EventName[PAPI_MAX_STR_LEN];
    for (i=0; i<number; i++) {
        //PAPI_event_code_to_name(Events[i], EventName);
        PAPI_event_code_to_name(PAPI_Events[i], EventName);
        printf("%s\t", EventName);
    }
#endif
    printf("\n");
}

void ompt_event_maps_to_graphml(thread_event_map_t* maps) {

    const char graphml_filename[] = "OMPTrace.graphml";
    FILE *graphml_file = fopen(graphml_filename, "w+");
    /* graphml format //
    <?xml version="1.0" encoding="UTF-8"?>
        <graphml xmlns="http://graphml.graphdrawing.org/xmlns"
            xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
            xsi:schemaLocation="http://graphml.graphdrawing.org/xmlns
            http://graphml.graphdrawing.org/xmlns/1.0/graphml.xsd">

            <graph id="G" edgedefault="directed">
                <node id="n0"/>
                <edge source="n0" target="n2"/>
                <node id="n1"/>
                <node id="n2"/>
            </graph>
        </graphml>
     */
    fprintf(graphml_file, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(graphml_file, "<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\"\n");
    fprintf(graphml_file, "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n");
    fprintf(graphml_file, "xsi:schemaLocation=\"http://graphml.graphdrawing.org/xmlns http://graphml.graphdrawing.org/xmlns/1.0/graphml.xsd\">\n");
    fprintf(graphml_file, "<graph id=\"G\" edgedefault=\"directed\">\n");

    int i;
    for (i=0; i<MAX_NUM_THREADS; i++) {
        thread_event_map_t * emap = get_event_map(i);
        if (i != 0 && emap->thread_id == 0) {/* this is the unused map after the last used one */
            num_threads = i;
            break;
        }

        int j = 0;
        for (j=0; j<emap->counter; j++) {
            ompt_trace_record_t * record = get_trace_record_from_emap(emap, j);
            /* create a graph node for each trace record, we need to create a unqiue node id, set node shape/size and color */
            fprintf(graphml_file, "<node id=\"%d-%d\"/>\n", i, j); /* record_id should be asserted to be equal to j */
            if (j > 0) { /* create the direct edge between two consecutive trace record */
                fprintf(graphml_file, "<edge source=\"%d-%d\" target=\"%d-%d\"/>\n", i, j-1, i, j); /* record_id should be asserted to be equal to j */
            }
        }
    }

    fprintf(graphml_file, "</graph>");
    fprintf(graphml_file, "</graphml>");
    fclose(graphml_file);

}