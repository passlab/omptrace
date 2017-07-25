#include <stdio.h>
#include <inttypes.h>
#include <execinfo.h>
#include <sched.h>
#include <string.h>
#include <stdlib.h>

#ifdef OMPT_USE_LIBUNWIND
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

#include <omp.h>
#include <ompt.h>
#include "omptool.h"

static ompt_set_callback_t ompt_set_callback;
static ompt_get_task_info_t ompt_get_task_info;
static ompt_get_thread_data_t ompt_get_thread_data;
static ompt_get_parallel_info_t ompt_get_parallel_info;
static ompt_get_unique_id_t ompt_get_unique_id;
static ompt_get_num_places_t ompt_get_num_places;
static ompt_get_place_proc_ids_t ompt_get_place_proc_ids;
static ompt_get_place_num_t ompt_get_place_num;
static ompt_get_partition_place_nums_t ompt_get_partition_place_nums;
static ompt_get_proc_id_t ompt_get_proc_id;

/*
#define ompt_callback_idle_spin  9900
#define ompt_callback_idle_suspend 9900

static void
on_ompt_callback_idle_spin(void * data) {
    const void *codeptr_ra  =  &on_ompt_callback_idle_spin;
    const void *frame  =  NULL; //OMPT_GET_FRAME_ADDRESS(0);
    int thread_id = get_global_thread_num();
    thread_event_map_t *emap = get_event_map(thread_id);
#ifdef OMPT_TRACING_SUPPORT
    ompt_trace_record_t *record = add_trace_record(thread_id, ompt_callback_idle_spin, frame, codeptr_ra);
#endif
  //  printf("Thread: %d idle spin\n", thread_id);
}

static void
on_ompt_callback_idle_suspend(void * data) {
    const void *codeptr_ra = &on_ompt_callback_idle_suspend;
    const void *frame = NULL; //OMPT_GET_FRAME_ADDRESS(0);
    int thread_id = get_global_thread_num();
    thread_event_map_t *emap = get_event_map(thread_id);
#ifdef OMPT_TRACING_SUPPORT
    ompt_trace_record_t *record = add_trace_record(thread_id, ompt_callback_idle_suspend, frame, codeptr_ra);
#endif
//    printf("Thread: %d idle suspend\n", thread_id);
}

*/

static void
on_ompt_callback_sync_region(
        ompt_sync_region_kind_t kind,
        ompt_scope_endpoint_t endpoint,
        ompt_data_t *parallel_data,
        ompt_data_t *task_data,
        const void *codeptr_ra)
{
    int thread_id = get_global_thread_num();
    thread_event_map_t *emap = get_event_map(thread_id);
#ifdef OMPT_TRACING_SUPPORT
    ompt_trace_record_t * parallel_lgrecord = task_data->ptr;
#endif

    ompt_trace_record_t *record;
    switch(endpoint)
    {
        case ompt_scope_begin: {
            ompt_lexgion_t *lgp = ompt_lexgion_begin(emap, codeptr_ra); /* for consolidating all event */
#ifdef OMPT_TRACING_SUPPORT
            record = add_trace_record_begin(emap, ompt_callback_sync_region, NULL, lgp, parallel_lgrecord);
#endif
            switch (kind) {
                case ompt_sync_region_barrier:
#ifdef OMPT_TRACING_SUPPORT
                    record->kind = ompt_sync_region_barrier;
#endif
                    //printf("%" PRIu64 ": ompt_event_barrier_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", return_address=%p\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra);
                    //print_ids(0);
                    break;
                case ompt_sync_region_taskwait:
#ifdef OMPT_TRACING_SUPPORT
                    record->kind = ompt_sync_region_taskwait;
#endif
                    break;
                case ompt_sync_region_taskgroup:
#ifdef OMPT_TRACING_SUPPORT
                    record->kind = ompt_sync_region_taskgroup;
#endif
                    break;
            }
            break;
        }
        case ompt_scope_end: {
#ifdef OMPT_TRACING_SUPPORT
            record = add_trace_record_end(emap, ompt_callback_sync_region, codeptr_ra);
#endif
            switch (kind) {
                case ompt_sync_region_barrier:
#ifdef OMPT_TRACING_SUPPORT
                    record->kind = ompt_sync_region_barrier;
#endif
                    //printf("%" PRIu64 ": ompt_event_barrier_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", return_address=%p\n", ompt_get_thread_data()->value, (parallel_data)?parallel_data->value:0, task_data->value, codeptr_ra);
                    break;
                case ompt_sync_region_taskwait:
#ifdef OMPT_TRACING_SUPPORT
                    record->kind = ompt_sync_region_taskwait;
#endif
                    break;
                case ompt_sync_region_taskgroup:
#ifdef OMPT_TRACING_SUPPORT
                    record->kind = ompt_sync_region_taskgroup;
#endif
                    break;
            }
            pop_lexgion(emap);
            break;
        }
    }
}

static void
on_ompt_callback_sync_region_wait(
        ompt_sync_region_kind_t kind,
        ompt_scope_endpoint_t endpoint,
        ompt_data_t *parallel_data,
        ompt_data_t *task_data,
        const void *codeptr_ra)
{
    int thread_id = get_global_thread_num();
    thread_event_map_t *emap = get_event_map(thread_id);
#ifdef OMPT_TRACING_SUPPORT
    ompt_trace_record_t * parallel_lgrecord = task_data->ptr;
#endif

    ompt_trace_record_t *record;
    switch(endpoint)
    {
        case ompt_scope_begin: {
            ompt_lexgion_t *lgp = ompt_lexgion_begin(emap,
                                                     codeptr_ra); /* for consolidating all event, sync_region and syn_region_wait are merged into one lexgion */
#ifdef OMPT_TRACING_SUPPORT
            record = add_trace_record_begin(emap, ompt_callback_sync_region_wait, NULL, lgp, parallel_lgrecord);
#endif
            switch (kind) {
                case ompt_sync_region_barrier:
#ifdef OMPT_TRACING_SUPPORT
                    record->kind = ompt_sync_region_barrier;
#endif
                    //printf("%" PRIu64 ": ompt_event_barrier_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", return_address=%p\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra);
                    //print_ids(0);
                    break;
                case ompt_sync_region_taskwait:
#ifdef OMPT_TRACING_SUPPORT
                    record->kind = ompt_sync_region_taskwait;
#endif
                    break;
                case ompt_sync_region_taskgroup:
#ifdef OMPT_TRACING_SUPPORT
                    record->kind = ompt_sync_region_taskgroup;
#endif
                    break;
            }
            break;
        }
        case ompt_scope_end: {
#ifdef OMPT_TRACING_SUPPORT
            record = add_trace_record_end(emap, ompt_callback_sync_region_wait, codeptr_ra);
#endif
            switch (kind) {
                case ompt_sync_region_barrier:
#ifdef OMPT_TRACING_SUPPORT
                    record->kind = ompt_sync_region_barrier;
#endif
                    //printf("%" PRIu64 ": ompt_event_barrier_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", return_address=%p\n", ompt_get_thread_data()->value, (parallel_data)?parallel_data->value:0, task_data->value, codeptr_ra);
                    break;
                case ompt_sync_region_taskwait:
#ifdef OMPT_TRACING_SUPPORT
                    record->kind = ompt_sync_region_taskwait;
#endif
                    break;
                case ompt_sync_region_taskgroup:
#ifdef OMPT_TRACING_SUPPORT
                    record->kind = ompt_sync_region_taskgroup;
#endif
                    break;
            }
            pop_lexgion(emap);
            break;
        }
    }
}

static void on_ompt_callback_idle(
        ompt_scope_endpoint_t endpoint) {
    int thread_id = get_global_thread_num();
    thread_event_map_t *emap = get_event_map(thread_id);
    const void *codeptr_ra = &on_ompt_callback_idle;

    switch (endpoint) {
        case ompt_scope_begin: {
            const void *frame = NULL; //TODO: FIXME OMPT_GET_FRAME_ADDRESS(0);
            ompt_lexgion_t * lgp = ompt_lexgion_begin(emap, codeptr_ra); /* consolidate all the idle event */
#ifdef OMPT_TRACING_SUPPORT
            ompt_trace_record_t *record = add_trace_record_begin(emap, ompt_callback_idle, frame, lgp, NULL);
#endif
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
   //         printf("Thread: %d idle begin\n", thread_id);
   //         print_frame(0);
   //         printf("frame  address: %p\n", OMPT_GET_FRAME_ADDRESS(0));
   //         printf("return address: %p\n", OMPT_GET_RETURN_ADDRESS(0));
            break;
        }
        case ompt_scope_end: {
#ifdef OMPT_TRACING_SUPPORT
            ompt_trace_record_t *record = add_trace_record_end(emap, ompt_callback_idle, codeptr_ra);
#endif
            pop_lexgion(emap);
#ifdef PE_OPTIMIZATION_SUPPORT
            int id = sched_getcpu();
            int pair_id;

            if (id != TOTAL_NUM_CORES && id != 0) {
                /*set up the state of kernel cpu id as the beginning of idle.*/
                HWTHREADS_IDLE_FLAG[id] = 1;
                record->frequency = pe_adjust_freq(id, CORE_HIGH_FREQ);
            }
#endif
            //printf("Thread: %d idle end\n", thread_id);
            break;
        }
    }
}

static void
on_ompt_callback_work(
        ompt_work_type_t wstype,
        ompt_scope_endpoint_t endpoint,
        ompt_data_t *parallel_data,
        ompt_data_t *task_data,
        uint64_t count,
        const void *codeptr_ra)
{
    int thread_id = get_global_thread_num();
    thread_event_map_t *emap = get_event_map(thread_id);
#ifdef OMPT_TRACING_SUPPORT
    ompt_trace_record_t * parallel_lgrecord = parallel_data->ptr;
#endif

    ompt_trace_record_t *record;
    switch(endpoint)
    {
        case ompt_scope_begin: {
            ompt_lexgion_t *lgp = ompt_lexgion_begin(emap,
                                                     codeptr_ra); /* for consolidating all event, sync_region and syn_region_wait are merged into one lexgion */
#ifdef OMPT_TRACING_SUPPORT
            record = add_trace_record_begin(emap, ompt_callback_sync_region_wait, NULL, lgp, parallel_lgrecord);
#endif
            break;
        }
        case ompt_scope_end:
#ifdef OMPT_TRACING_SUPPORT
            record = add_trace_record_end(emap, ompt_callback_sync_region_wait, codeptr_ra);
#endif
            pop_lexgion(emap);
            break;
    }

    switch(wstype)
    {
        case ompt_work_loop:
#ifdef OMPT_TRACING_SUPPORT
            record->kind = ompt_work_loop;
#endif
            //printf("%" PRIu64 ": ompt_event_loop_begin: parallel_id=%" PRIu64 ", parent_task_id=%" PRIu64 ", workshare_function=%p, count=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra, count);
            break;
        case ompt_work_sections:
#ifdef OMPT_TRACING_SUPPORT
            record->kind = ompt_work_sections;
#endif
            //impl
            break;
        case ompt_work_single_executor:
#ifdef OMPT_TRACING_SUPPORT
            record->kind = ompt_work_single_executor;
#endif
            //printf("%" PRIu64 ": ompt_event_single_in_block_begin: parallel_id=%" PRIu64 ", parent_task_id=%" PRIu64 ", workshare_function=%p, count=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra, count);
            break;
        case ompt_work_single_other:
#ifdef OMPT_TRACING_SUPPORT
            record->kind = ompt_work_single_other;
#endif
            //printf("%" PRIu64 ": ompt_event_single_others_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", workshare_function=%p, count=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra, count);
            break;
        case ompt_work_workshare:
#ifdef OMPT_TRACING_SUPPORT
            record->kind = ompt_work_workshare;
#endif
            //impl
            break;
        case ompt_work_distribute:
#ifdef OMPT_TRACING_SUPPORT
            record->kind = ompt_work_distribute;
#endif
            //impl
            break;
        case ompt_work_taskloop:
#ifdef OMPT_TRACING_SUPPORT
            record->kind = ompt_work_taskloop;
#endif
            //impl
            break;
    }
}

static void
on_ompt_callback_master(
        ompt_scope_endpoint_t endpoint,
        ompt_data_t *parallel_data,
        ompt_data_t *task_data,
        const void *codeptr_ra)
{
    int thread_id = get_global_thread_num();
    thread_event_map_t *emap = get_event_map(thread_id);
#ifdef OMPT_TRACING_SUPPORT
    ompt_trace_record_t * parallel_lgrecord = parallel_data->ptr;
#endif

    ompt_trace_record_t *record;
    switch(endpoint)
    {
        case ompt_scope_begin: {
            ompt_lexgion_t *lgp = ompt_lexgion_begin(emap,
                                                     codeptr_ra); /* for consolidating all event, sync_region and syn_region_wait are merged into one lexgion */
#ifdef OMPT_TRACING_SUPPORT
            record = add_trace_record_begin(emap, ompt_callback_master, NULL, lgp, parallel_lgrecord);
#endif
            //printf("%" PRIu64 ": ompt_event_master_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", codeptr_ra=%p\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra);
            break;
        }
        case ompt_scope_end: {
#ifdef OMPT_TRACING_SUPPORT
            record = add_trace_record_end(emap, ompt_callback_master, codeptr_ra);
#endif
            pop_lexgion(emap);
            //printf("%" PRIu64 ": ompt_event_master_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", codeptr_ra=%p\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra);
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
//    const void *codeptr_ra = OMPT_GET_RETURN_ADDRESS(0); /* address of the function who calls __kmpc_fork_call */
    const void *frame = parent_task_frame->reenter_runtime_frame;
    //const void *frame = OMPT_GET_FRAME_ADDRESS(0); /* the frame of the function who calls __kmpc_fork_call */
    int thread_id = get_global_thread_num();
    thread_event_map_t * emap = &event_maps[thread_id];
    ompt_lexgion_t * lgp = ompt_lexgion_begin(emap, codeptr_ra);
    int team_size = requested_team_size;
    int diff;
#ifdef OMPT_MEASUREMENT_SUPPORT
    if (lgp->total_record == 1) { /* the first record */
        ompt_measure_init(&lgp->current);
        ompt_measure_init(&lgp->accu);
    } else {
#ifdef REX_RAUTO_TUNING
        if (lgp->current.requested_team_size == requested_team_size) { /* if we are executing the parallel lexgion of the same problem size */
            if (lgp->total_record == 2) { /* second time, tuning started */
                    memcpy(&lgp->best, &lgp->current, sizeof(ompt_measurement_t));
                    lgp->best_counter = 1;
                    team_size = requested_team_size - 1;
                    if (team_size <= 0) team_size = 1;
                    __kmp_push_num_threads(NULL, thread_id, team_size);
            } else {
                if (lgp->best_counter < 5) { /* we will keep auto-tuning for at least 5 times */
                    diff = ompt_measure_compare(&lgp->best, &lgp->current);
                    //printf("perf improvement of last one over the best: %d%%\n", diff);
                    if (diff >= 0){ /* the second time, or better performance */
                        memcpy(&lgp->best, &lgp->current, sizeof(ompt_measurement_t));
                        lgp->best_counter = 1;
                        team_size = lgp->current.team_size - 1;
                        if (team_size <= 0) team_size = 1;
                        __kmp_push_num_threads(NULL, thread_id, team_size);
                        printf("set team size for %X: %d->%d, improvement %d%%\n", codeptr_ra, requested_team_size, team_size, diff);
                    } else {
                        team_size = lgp->best.team_size;
                        lgp->best_counter++;
                        __kmp_push_num_threads(NULL, thread_id, team_size);
                    }
                } else {
                    team_size = lgp->best.team_size;
                    lgp->best_counter++;
                    __kmp_push_num_threads(NULL, thread_id, team_size);
                    //printf("No tune anymore for this lexgion: %X, at count: %d\n", codeptr_ra, lgp->total_record);
                }
            }
        }
#endif
    }
    lgp->current.requested_team_size = requested_team_size;
    lgp->current.team_size = team_size;
#endif

#ifdef OMPT_TRACING_SUPPORT
    ompt_trace_record_t *record = add_trace_record_begin(emap, ompt_callback_parallel_begin, frame, lgp, NULL);
    parallel_data->ptr = record; /* for tracing, the parallel_data->ptr store the actural trace record */
    record->requested_team_size = requested_team_size;
    record->team_size = team_size;
    record->codeptr_ra = codeptr_ra;
#else
    parallel_data->ptr = lgp; /* store the lexgion as part of the parallel data to pass around */
#endif

#ifdef OMPT_MEASUREMENT_SUPPORT
    ompt_measure(&lgp->current);
#endif

    //printf("Thread: %d parallel begin: FRAME_ADDRESS: %p, LOCATION: %p, exit_runtime_frame: %p, reenter_runtime_frame: %p, codeptr_ra: %p\n",
    //thread_id, record->user_frame, record->codeptr_ra, parent_task_frame->exit_runtime_frame, parent_task_frame->reenter_runtime_frame, codeptr_ra);
    //print_ids(4);
}

static void
on_ompt_callback_parallel_end(
        ompt_data_t *parallel_data,
        ompt_data_t *task_data,
        ompt_invoker_t invoker,
        const void *codeptr_ra) {
    int thread_id = get_global_thread_num();
    thread_event_map_t *emap = get_event_map(thread_id);
    ompt_lexgion_t * lgp;
    ompt_trace_record_t * parallel_lgrecord;

#ifdef OMPT_TRACING_SUPPORT
    parallel_lgrecord = parallel_data->ptr;
    lgp = parallel_lgrecord->lgp;
#else
    lgp = parallel_data->ptr;
#endif

#ifdef OMPT_MEASUREMENT_SUPPORT
    ompt_measure_consume(&lgp->current);
    ompt_measure_accu(&lgp->accu, &lgp->current);
#endif

    //const void *codeptr_ra  =  OMPT_GET_RETURN_ADDRESS(2); /* address of the function who calls __kmpc_fork_call */
    const void *frame = NULL; // OMPT_GET_FRAME_ADDRESS(0);

#ifdef OMPT_TRACING_SUPPORT
    ompt_trace_record_t *record = add_trace_record_end(emap, ompt_callback_parallel_end, codeptr_ra);

#ifdef OMPT_MEASUREMENT_SUPPORT
    record->measurement = lgp->current;
#endif

#ifdef OMPT_ONLINE_TRACING_PRINT
    printf("Thread: %d, parallel: %p, record: %d  |", thread_id, codeptr_ra, record->match_record);
    ompt_measure_print_header(&lgp->current);
    printf("                                    \t|");
    ompt_measure_print(&lgp->current, -1);
#endif
#endif
    pop_lexgion(emap);

//    printf("Thread: %d parallel end\n", thread_id);
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
on_ompt_callback_implicit_task(
        ompt_scope_endpoint_t endpoint,
        ompt_data_t *parallel_data,
        ompt_data_t *task_data,
        unsigned int team_size,
        unsigned int thread_num)
{
    int thread_id = get_global_thread_num();
    thread_event_map_t * emap = get_event_map(thread_id);
    ompt_lexgion_t * paralel_lgp;
    ompt_trace_record_t * parallel_lgrecord;
    ompt_trace_record_t * record;
    switch(endpoint)
    {
        case ompt_scope_begin:
            /* in this call back, parallel_data is NULL for ompt_scope_end endpoint, thus to know the parallel_data at the end,
             * we need to pass the needed fields of parallel_data in the scope_begin to the task_data */
            task_data->value = ompt_get_unique_id();
            task_data->ptr = parallel_data->ptr;
#ifdef OMPT_TRACING_SUPPORT
            parallel_lgrecord = task_data->ptr;
            paralel_lgp = parallel_lgrecord->lgp;
            record = add_trace_record_begin(emap, ompt_callback_implicit_task, NULL, paralel_lgp, parallel_lgrecord);
#else
            paralel_lgp = (ompt_lexgion_t *) task_data->ptr;
#endif
            //printf("%" PRIu64 ": ompt_event_implicit_task_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", team_size=%" PRIu32 ", thread_num=%" PRIu32 "\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, team_size, thread_num);
            break;
        case ompt_scope_end:
#ifdef OMPT_TRACING_SUPPORT
//            parallel_lgrecord = task_data->ptr;
//            paralel_lgp = parallel_lgrecord->lgp;
            /* blame shifting, whatever it calls
             * implicit_task_end event may be trigger by the fork_barrier of the following parallel region, e.g. when the thread
             * who reaches a barrier earlier than others and sleeps. Thus the event does not reflect the actual end of the implicit
             * task
             */
            record = add_trace_record_end(emap, ompt_callback_implicit_task, NULL);
#endif
            //printf("%" PRIu64 ": ompt_event_implicit_task_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", team_size=%" PRIu32 ", thread_num=%" PRIu32 "\n", ompt_get_thread_data()->value, (parallel_data)?parallel_data->value:0, task_data->value, team_size, thread_num);
            break;
    }
}

static void
on_ompt_callback_thread_begin(
        ompt_thread_type_t thread_type,
        ompt_data_t *thread_data) {
    int thread_id = get_global_thread_num();
    thread_event_map_t * emap = init_thread_event_map(thread_id);
    const void *codeptr_ra = &on_ompt_callback_thread_begin;
    const void *frame = NULL; //OMPT_GET_FRAME_ADDRESS(0);
    ompt_lexgion_t * lgp = ompt_lexgion_begin(emap, codeptr_ra);
#ifdef OMPT_TRACING_SUPPORT
    emap->records = (ompt_trace_record_t *) malloc(sizeof(ompt_trace_record_t) * MAX_NUM_RECORDS);
    /* it is important to note here that this lgp is NOT the parallel lgp */
    ompt_trace_record_t *record = add_trace_record_begin(emap, ompt_callback_thread_begin, frame, lgp, NULL);
    thread_data->ptr = record;
#else
    thread_data->ptr = lgp;
#endif
    //printf("Thread: %d thread begin\n", thread_id);
    ompt_measure_init(&emap->thread_total);
    ompt_measure(&emap->thread_total);
}

static void
on_ompt_callback_thread_end(
        ompt_data_t *thread_data) {
    int thread_id = get_global_thread_num();
    thread_event_map_t *emap = get_event_map(thread_id);
    const void *codeptr_ra = &on_ompt_callback_thread_end; /* address of the function who calls __kmpc_fork_call */
    const void *frame = NULL; //OMPT_GET_FRAME_ADDRESS(0);
    //printf("Thread: %d thread end\n", thread_id);
#ifdef OMPT_TRACING_SUPPORT
    ompt_trace_record_t *record = add_trace_record_end(emap, ompt_callback_thread_end, codeptr_ra);
#endif
    pop_lexgion(emap);

//    fini_thread_event_map(thread_id);
    ompt_measure_consume(&emap->thread_total);
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
//    register_callback(ompt_callback_idle);
//    register_callback(ompt_callback_idle_spin);
//    register_callback(ompt_callback_idle_suspend);
    register_callback(ompt_callback_sync_region);
    //register_callback(ompt_callback_sync_region_wait);
    register_callback_t(ompt_callback_sync_region_wait, ompt_callback_sync_region_t);
    register_callback(ompt_callback_parallel_begin);
    register_callback(ompt_callback_parallel_end);
    register_callback(ompt_callback_implicit_task);
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

    memset(event_maps, 0, sizeof(thread_event_map_t)*MAX_NUM_THREADS);
    ompt_measure_global_init( );
    ompt_measure_init(&total_consumed);
    ompt_measure(&total_consumed);

    return 1; //success
}

void ompt_finalize(ompt_fns_t *fns) {
    // on_ompt_event_runtime_shutdown();
    ompt_measure_consume(&total_consumed);
    ompt_measure_global_fini( );
    printf("==============================================================================================\n");
    printf("Total OpenMP Execution: | ");
    ompt_measure_print_header(&total_consumed);
    printf("                        | ");
    ompt_measure_print(&total_consumed, NULL);
    printf("==============================================================================================\n");

    /*
    void* callstack[128];
    int i, frames = backtrace(callstack, 128);
    char** strs = backtrace_symbols(callstack, frames);
    for (i = 0; i < frames; ++i) {
        printf("%s\n", strs[i]);
    }
     */

//    int thread_id = get_global_thread_num();
    thread_event_map_t *emap = get_event_map(0);
#ifdef OMPT_TRACING_SUPPORT
#ifdef OMPT_TRACING_GRAPHML_DUMP
    list_parallel_lexgions(emap);
    ompt_event_maps_to_graphml(event_maps);
#endif
#endif
}

ompt_fns_t *ompt_start_tool(
        unsigned int omp_version,
        const char *runtime_version) {
    static ompt_fns_t ompt_fns = {&ompt_initialize, &ompt_finalize};
    return &ompt_fns;
}
