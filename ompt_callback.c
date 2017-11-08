#define _BSD_SOURCE
#include <stdio.h>
#include <inttypes.h>
#include <omp.h>
#include <ompt.h>

#include "omptool.h"

static const char* ompt_thread_type_t_values[] = {
        NULL,
        "ompt_thread_initial",
        "ompt_thread_worker",
        "ompt_thread_other"
};

static const char* ompt_task_status_t_values[] = {
        NULL,
        "ompt_task_complete",
        "ompt_task_yield",
        "ompt_task_cancel",
        "ompt_task_others"
};
static const char* ompt_cancel_flag_t_values[] = {
        "ompt_cancel_parallel",
        "ompt_cancel_sections",
        "ompt_cancel_do",
        "ompt_cancel_taskgroup",
        "ompt_cancel_activated",
        "ompt_cancel_detected",
        "ompt_cancel_discarded_task"
};

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
static ompt_enumerate_states_t ompt_enumerate_states;
static ompt_enumerate_mutex_impls_t ompt_enumerate_mutex_impls;

static void print_ids(int level)
{
    ompt_frame_t* frame ;
    ompt_data_t* parallel_data;
    ompt_data_t* task_data;
    int exists_task = ompt_get_task_info(level, NULL, &task_data, &frame, &parallel_data, NULL);
    if (frame)
    {
        printf("%" PRIu64 ": task level %d: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", exit_frame=%p, reenter_frame=%p\n", ompt_get_thread_data()->value, level, exists_task ? parallel_data->value : 0, exists_task ? task_data->value : 0, frame->exit_frame, frame->enter_frame);
    }
    else
        printf("%" PRIu64 ": task level %d: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", frame=%p\n", ompt_get_thread_data()->value, level, exists_task ? parallel_data->value : 0, exists_task ? task_data->value : 0, frame);
}

#define print_frame(level)\
do {\
  printf("%" PRIu64 ": __builtin_frame_address(%d)=%p\n", ompt_get_thread_data()->value, level, __builtin_frame_address(level));\
} while(0)

#define print_current_address(id)\
{}              /* Empty block between "#pragma omp ..." and __asm__ statement as a workaround for icc bug */ \
__asm__("nop"); /* provide an instruction as jump target (compiler would insert an instruction if label is target of a jmp ) */ \
ompt_label_##id:\
    printf("%" PRIu64 ": current_address=%p or %p\n", ompt_get_thread_data()->value, (char*)(&& ompt_label_##id)-1, (char*)(&& ompt_label_##id)-4)
/* "&& label" returns the address of the label (GNU extension); works with gcc, clang, icc */
/* for void-type runtime function, the label is after the nop (-1), for functions with return value, there is a mov instruction before the label (-4) */

#define print_fuzzy_address(id)\
{}              /* Empty block between "#pragma omp ..." and __asm__ statement as a workaround for icc bug */ \
__asm__("nop"); /* provide an instruction as jump target (compiler would insert an instruction if label is target of a jmp ) */ \
ompt_label_##id:\
    printf("%" PRIu64 ": fuzzy_address=0x%lx or 0x%lx\n", ompt_get_thread_data()->value, ((uint64_t)(char*)(&& ompt_label_##id))/256-1, ((uint64_t)(char*)(&& ompt_label_##id))/256)
/* "&& label" returns the address of the label (GNU extension); works with gcc, clang, icc */
/* for void-type runtime function, the label is after the nop (-1), for functions with return value, there is a mov instruction before the label (-4) */

static void format_task_type(int type, char* buffer)
{
    char* progress = buffer;
    if(type & ompt_task_initial) progress += sprintf(progress, "ompt_task_initial");
    if(type & ompt_task_implicit) progress += sprintf(progress, "ompt_task_implicit");
    if(type & ompt_task_explicit) progress += sprintf(progress, "ompt_task_explicit");
    if(type & ompt_task_target) progress += sprintf(progress, "ompt_task_target");
    if(type & ompt_task_undeferred) progress += sprintf(progress, "|ompt_task_undeferred");
    if(type & ompt_task_untied) progress += sprintf(progress, "|ompt_task_untied");
    if(type & ompt_task_final) progress += sprintf(progress, "|ompt_task_final");
    if(type & ompt_task_mergeable) progress += sprintf(progress, "|ompt_task_mergeable");
    if(type & ompt_task_merged) progress += sprintf(progress, "|ompt_task_merged");
}

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
    ompt_trace_record_t * parallel_record = task_data->ptr;
#endif

    ompt_trace_record_t *record;
    switch(endpoint)
    {
        case ompt_scope_begin: {
            ompt_lexgion_t * lgp;
            switch (kind) {
                case ompt_sync_region_barrier: {
#ifdef OMPT_TRACING_SUPPORT
                    ompt_trace_record_t *parallel_record = parallel_data->ptr;
                    ompt_lexgion_t *parallel_lgp = parallel_record->lgp;
                    if (codeptr_ra == NULL || parallel_lgp->codeptr_ra == codeptr_ra) {
                        /* this is the join barrier for the parallel region: if codeptr_ra == NULL: non-master thread;
                         * if parallel_lgp->codeptr_ra == codeptr_ra: master thread */
                        lgp = parallel_lgp;
                        record = add_trace_record_begin(emap, ompt_callback_sync_region, NULL, lgp, task_data->ptr,
                                                        parallel_record);
                        ompt_trace_record_t *implicit_task_record = task_data->ptr;
                        parallel_record->parallel_implicit_barrier_sync[implicit_task_record->thread_id_inteam*OFFSET4FS] = record;
                        //if (codeptr_ra != NULL) tribute_record_lexgion(lgp, record); /* master thread test */
                        //printf("%" PRIu64 ": ompt_event_join_barrier_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", return_address=%p\n",
                        //       ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra);
                    } else { /* other type of barrier, either implicit or explicit */
                        /* each thread will have a lexgion object for the same lexgion */
                        lgp = ompt_lexgion_begin(emap, ompt_callback_sync_region, codeptr_ra); /*  */
                        record = add_trace_record_begin(emap, ompt_callback_sync_region, NULL, lgp, task_data->ptr,
                                                        parallel_record);
                        tribute_record_lexgion(lgp, record);
                        //printf("%" PRIu64 ": ompt_event_barrier_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", return_address=%p\n",
                        //       ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra);
                    }
                    record->kind = ompt_sync_region_barrier;
#endif
                    //print_ids(0);
                    break;
                }
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
            switch (kind) {
                case ompt_sync_region_barrier: {
#ifdef OMPT_TRACING_SUPPORT
                    ompt_trace_record_t * begin_record = top_record(emap);
                    record = add_trace_record_end(emap, ompt_callback_sync_region, codeptr_ra);
                    ompt_trace_record_t *parallel_record = begin_record->parallel_record;
                    ompt_lexgion_t *parallel_lgp = parallel_record->lgp;
                    if (codeptr_ra == NULL || parallel_lgp->codeptr_ra == codeptr_ra) {
                        /* this is the join barrier for the parallel region: if codeptr_ra == NULL: non-master thread;
                         * if parallel_lgp->codeptr_ra == codeptr_ra: master thread */
                       // printf("%" PRIu64 ": ompt_event_join_barrier_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", return_address=%p\n",
                       //        ompt_get_thread_data()->value, (parallel_data) ? parallel_data->value : 0, task_data->value, codeptr_ra);
                    } else {
                        pop_lexgion(emap);
                        //printf("%" PRIu64 ": ompt_event_barrier_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", return_address=%p\n",
                        //       ompt_get_thread_data()->value, (parallel_data) ? parallel_data->value : 0, task_data->value, codeptr_ra);
                    }
                    record->kind = ompt_sync_region_barrier;
#endif
                    break;
                }
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
    ompt_trace_record_t * parallel_record = task_data->ptr;
#endif

    ompt_trace_record_t *record;
    switch(endpoint)
    {
        case ompt_scope_begin: {
            ompt_lexgion_t *lgp;
            switch (kind) {
                case ompt_sync_region_barrier: {
#ifdef OMPT_TRACING_SUPPORT
                    ompt_trace_record_t *parallel_record = parallel_data->ptr;
                    ompt_lexgion_t *parallel_lgp = parallel_record->lgp;
                    if (codeptr_ra == NULL || parallel_lgp->codeptr_ra == codeptr_ra) {
                        /* this is the join barrier for the parallel region: if codeptr_ra == NULL: non-master thread;
                         * if parallel_lgp->codeptr_ra == codeptr_ra: master thread */
                        lgp = parallel_lgp;
                        record = add_trace_record_begin(emap, ompt_callback_sync_region_wait, NULL, lgp, task_data->ptr,
                                                        parallel_record);
                        ompt_trace_record_t *implicit_task_record = task_data->ptr;
                        parallel_record->parallel_implicit_barrier_wait[implicit_task_record->thread_id_inteam*OFFSET4FS] = record;
                        //printf("%" PRIu64 ": ompt_event_join_barrier_wait_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", return_address=%p\n",
                        //       ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra);
                    } else {
                        lgp = ompt_lexgion_begin(emap, ompt_callback_sync_region_wait, codeptr_ra); /*  */
                        record = add_trace_record_begin(emap, ompt_callback_sync_region_wait, NULL, lgp, task_data->ptr, parallel_record);
                        //printf("%" PRIu64 ": ompt_event_barrier_wait_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", return_address=%p\n",
                        //       ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra);
                    }
                    //tribute_record_lexgion(lgp, record);
                    record->kind = ompt_sync_region_barrier;
#endif
                    //print_ids(0);
                    break;
                }
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
            switch (kind) {
                case ompt_sync_region_barrier: {
#ifdef OMPT_TRACING_SUPPORT
                    /* for parallel lexgion, parallel_data may be NULL at this point */
                    ompt_trace_record_t * begin_record = top_record(emap);
                    record = add_trace_record_end(emap, ompt_callback_sync_region_wait, codeptr_ra);
                    ompt_trace_record_t *parallel_record = begin_record->parallel_record;
                    ompt_lexgion_t *parallel_lgp = parallel_record->lgp;
                    if (codeptr_ra == NULL || parallel_lgp->codeptr_ra == codeptr_ra) {
                        /* this is the join barrier for the parallel region: if codeptr_ra == NULL: non-master thread;
                         * if parallel_lgp->codeptr_ra == codeptr_ra: master thread */
                        //printf("%" PRIu64 ": ompt_event_join_barrier_wait_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", return_address=%p\n",
                        //       ompt_get_thread_data()->value, (parallel_data) ? parallel_data->value : 0, task_data->value, codeptr_ra);
                    } else {
                        pop_lexgion(emap);
                        //printf("%" PRIu64 ": ompt_event_barrier_wait_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", return_address=%p\n",
                        //       ompt_get_thread_data()->value, (parallel_data) ? parallel_data->value : 0, task_data->value, codeptr_ra);
                    }
                    record->kind = ompt_sync_region_barrier;
#endif
                    break;
                }
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
            ompt_lexgion_t * lgp = ompt_lexgion_begin(emap, ompt_callback_idle, codeptr_ra); /* consolidate all the idle event */
#ifdef OMPT_TRACING_SUPPORT
            ompt_trace_record_t *record = add_trace_record_begin(emap, ompt_callback_idle, frame, lgp, NULL, NULL);
            tribute_record_lexgion(lgp, record);
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
    ompt_trace_record_t * parallel_record = parallel_data->ptr;
    ompt_trace_record_t * task_record = task_data->ptr;
#endif

    ompt_trace_record_t *record;
    switch(endpoint)
    {
        case ompt_scope_begin: {
            ompt_lexgion_t * lgp;
            switch(wstype)
            {
                case ompt_work_loop:
#ifdef OMPT_TRACING_SUPPORT
                    /* NOTE: for combined or composite construct such as "parallel for", there are two different lexgions in LLVM OpenMP,
                     * one for parallel and one for for. Also since this is per-thread callback, each thread will have its own lexgion object created */
                    lgp = ompt_lexgion_begin(emap, ompt_work_loop, codeptr_ra);
                    record = add_trace_record_begin(emap, ompt_callback_work, NULL, lgp, task_record, parallel_record);
                    record->kind = ompt_work_loop;
                    tribute_record_lexgion(lgp, record);
#endif
                    //printf("%" PRIu64 ": ompt_event_loop_begin: parallel_id=%" PRIu64 ", parent_task_id=%" PRIu64 ", workshare_function=%p, count=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra, count);
                    break;
                case ompt_work_sections:
                    lgp->type = ompt_work_sections;
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

            break;
        }
        case ompt_scope_end:
#ifdef OMPT_TRACING_SUPPORT
            record = add_trace_record_end(emap, ompt_callback_work, codeptr_ra);
#endif
            /* for if each thread has its own lexgion object for the same lexgion */
            //if (task_record->thread_id_inteam != 0)
            pop_lexgion(emap);
            switch(wstype)
            {
                case ompt_work_loop:
#ifdef OMPT_TRACING_SUPPORT
                    record->kind = ompt_work_loop;
#endif
                    //printf("%" PRIu64 ": ompt_event_loop_end: parallel_id=%" PRIu64 ", parent_task_id=%" PRIu64 ", workshare_function=%p, count=%" PRIu64 "\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra, count);
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
    ompt_trace_record_t * parallel_record = parallel_data->ptr;
#endif

    ompt_trace_record_t *record;
    switch(endpoint)
    {
        case ompt_scope_begin: {
            ompt_lexgion_t *lgp = ompt_lexgion_begin(emap, ompt_callback_master, codeptr_ra); /* for consolidating all event, sync_region and syn_region_wait are merged into one lexgion */
#ifdef OMPT_TRACING_SUPPORT
            record = add_trace_record_begin(emap, ompt_callback_master, NULL, lgp, task_data->ptr, parallel_record);
            tribute_record_lexgion(lgp, record);
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
    const void *frame = parent_task_frame->enter_frame;
    //const void *frame = OMPT_GET_FRAME_ADDRESS(0); /* the frame of the function who calls __kmpc_fork_call */
    int thread_id = get_global_thread_num();
    thread_event_map_t * emap = &event_maps[thread_id];
    ompt_lexgion_t * lgp = ompt_lexgion_begin(emap, ompt_callback_parallel_begin, codeptr_ra);
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
    ompt_trace_record_t *record = add_trace_record_begin(emap, ompt_callback_parallel_begin, frame, lgp, parent_task_data->ptr, NULL);
    record->parallel_record = record; /* this is the record for this parallel region. TODO: for nested parallel region */
    tribute_record_lexgion(lgp, record);
    parallel_data->ptr = record; /* for tracing, the parallel_data->ptr store the actural trace record */
    record->requested_team_size = requested_team_size;
    record->team_size = team_size;
    record->codeptr_ra = codeptr_ra;

    /* init the array for the implicit tasks array */
    int size = sizeof(ompt_trace_record_t*)*OFFSET4FS*team_size; /* sizes for thread-specific records of this parallel region */
    record->parallel_implicit_tasks = (ompt_trace_record_t**) malloc(3*size); /* we use one malloc for the three arrays */
    record->parallel_implicit_barrier_sync = record->parallel_implicit_tasks + size;
    record->parallel_implicit_barrier_wait = record->parallel_implicit_barrier_sync + size;
#else
    parallel_data->ptr = lgp; /* store the lexgion as part of the parallel data to pass around */
#endif

#ifdef OMPT_MEASUREMENT_SUPPORT
    ompt_measure(&lgp->current);
#endif

   // printf("%d: parallel begin: FRAME_ADDRESS: %p, LOCATION: %p, exit_runtime_frame: %p, reenter_runtime_frame: %p, codeptr_ra: %p\n",
    //   thread_id, record->user_frame, record->codeptr_ra, parent_task_frame->exit_runtime_frame, parent_task_frame->reenter_runtime_frame, codeptr_ra);

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
    ompt_trace_record_t * parallel_record;

#ifdef OMPT_TRACING_SUPPORT
    parallel_record = parallel_data->ptr;
    lgp = parallel_record->lgp;
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
    ompt_trace_record_t * parallel_record;
    ompt_trace_record_t * record;
    switch(endpoint)
    {
        case ompt_scope_begin:
            /* in this call back, parallel_data is NULL for ompt_scope_end endpoint, thus to know the parallel_data at the end,
             * we need to pass the needed fields of parallel_data in the scope_begin to the task_data */
            task_data->value = ompt_get_unique_id();
#ifdef OMPT_TRACING_SUPPORT
            parallel_record = parallel_data->ptr;
            paralel_lgp = parallel_record->lgp;
            record = add_trace_record_begin(emap, ompt_callback_implicit_task, NULL, paralel_lgp, NULL, parallel_record);
            int thread_num = omp_get_thread_num();
            record->thread_id_inteam = thread_num;
            task_data->ptr = record;

            parallel_record->parallel_implicit_tasks[thread_num*OFFSET4FS] = record;

            /* we donot tribute this per-thread event to the parallel lexgion that started by the master thread for several
             * reasons, e.g. data racing if adding the record to the list, etc */
#else
#endif
            //printf("%" PRIu64 ": ompt_event_implicit_task_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", team_size=%" PRIu32 ", thread_num=%" PRIu32 "\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, team_size, thread_num);
            break;
        case ompt_scope_end:
#ifdef OMPT_TRACING_SUPPORT
//            parallel_record = task_data->ptr;
//            paralel_lgp = parallel_record->lgp;
            /* blame shifting, whatever it calls
             * implicit_task_end event may be trigger by the fork_barrier of the following parallel region, e.g. when the thread
             * who reaches a barrier earlier than others and sleeps. Thus the event does not reflect the actual end of the implicit
             * task
             *
             * TODO: codeptr_ra pointer is NOT available
             */
            record = add_trace_record_end(emap, ompt_callback_implicit_task, NULL);
#endif
            //printf("%" PRIu64 ": ompt_event_implicit_task_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", team_size=%" PRIu32 ", thread_num=%" PRIu32 "\n", ompt_get_thread_data()->value, (parallel_data)?parallel_data->value:0, task_data->value, team_size, thread_num);
            break;
    }
}

static void
on_ompt_callback_task_create(
        ompt_data_t *parent_task_data,     /* id of parent task            */
        const ompt_frame_t *parent_frame,  /* frame data for parent task   */
        ompt_data_t* new_task_data,        /* id of created task           */
        int type,
        int has_dependences,
        const void *codeptr_ra)               /* pointer to outlined function */
{
    int thread_id = get_global_thread_num();
    thread_event_map_t * emap = get_event_map(thread_id);
    if(new_task_data->ptr)
        printf("%s\n", "0: new_task_data initially not null");
    new_task_data->value = ompt_get_unique_id();
    ompt_data_t *parallel_data;
    ompt_get_parallel_info(0, &parallel_data, NULL);

    //there is no paralllel_begin callback for implicit parallel region
    //thus it is initialized in initial task
    if(type & ompt_task_initial)
    {
        parallel_data->value = ompt_get_unique_id();
        parallel_data->ptr = NULL; /* so far, we do not have a record for implicit parallel region */

#ifdef OMPT_TRACING_SUPPORT
        ompt_trace_record_t * initial_task_record = add_trace_record_begin(emap, ompt_callback_task_create,
                                                                           parent_frame, NULL, NULL, NULL);
#endif
    } else {
        ompt_lexgion_t * lgp = ompt_lexgion_begin(emap, ompt_callback_task_create, codeptr_ra);
#ifdef OMPT_TRACING_SUPPORT
        ompt_trace_record_t * record = add_trace_record_begin(emap, ompt_callback_task_create, parent_frame, lgp,
                                                              parent_task_data->ptr, parallel_data->ptr);
        tribute_record_lexgion(lgp, record);
#endif

    }

    //char buffer[2048];
    //format_task_type(type, buffer);
    //printf("%" PRIu64 ": ompt_event_task_create: parent_task_id=%" PRIu64 ", parent_task_frame.exit=%p, parent_task_frame.reenter=%p, new_task_id=%" PRIu64 ", parallel_function=%p, task_type=%s=%d, has_dependences=%s\n", ompt_get_thread_data()->value, parent_task_data ? parent_task_data->value : 0, parent_frame ? parent_frame->exit_runtime_frame : NULL, parent_frame ? parent_frame->reenter_runtime_frame : NULL, new_task_data->value, codeptr_ra, buffer, type, has_dependences ? "yes" : "no");
}

static void
on_ompt_callback_task_schedule(
        ompt_data_t *first_task_data,
        ompt_task_status_t prior_task_status,
        ompt_data_t *second_task_data)
{
    //printf("%" PRIu64 ": ompt_event_task_schedule: first_task_id=%" PRIu64 ", second_task_id=%" PRIu64 ", prior_task_status=%s=%d\n", ompt_get_thread_data()->value, first_task_data->value, second_task_data->value, ompt_task_status_t_values[prior_task_status], prior_task_status);
    if(prior_task_status == ompt_task_complete)
    {
        printf("%" PRIu64 ": ompt_event_task_end: task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, first_task_data->value);
    }
}

static void
on_ompt_callback_task_dependences(
        ompt_data_t *task_data,
        const ompt_task_dependence_t *deps,
        int ndeps)
{
    printf("%" PRIu64 ": ompt_event_task_dependences: task_id=%" PRIu64 ", deps=%p, ndeps=%d\n", ompt_get_thread_data()->value, task_data->value, (void *)deps, ndeps);
}

static void
on_ompt_callback_task_dependence(
        ompt_data_t *first_task_data,
        ompt_data_t *second_task_data)
{
    printf("%" PRIu64 ": ompt_event_task_dependence_pair: first_task_id=%" PRIu64 ", second_task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, first_task_data->value, second_task_data->value);
}


static void
on_ompt_callback_thread_begin(
        ompt_thread_type_t thread_type,
        ompt_data_t *thread_data) {
    int thread_id = get_global_thread_num();
    thread_event_map_t * emap = init_thread_event_map(thread_id);
    const void *codeptr_ra = &on_ompt_callback_thread_begin;
    const void *frame = NULL; //OMPT_GET_FRAME_ADDRESS(0);
    ompt_lexgion_t * lgp = ompt_lexgion_begin(emap, ompt_callback_thread_begin, codeptr_ra);
#ifdef OMPT_TRACING_SUPPORT
    emap->records = (ompt_trace_record_t *) malloc(sizeof(ompt_trace_record_t) * MAX_NUM_RECORDS);
    /* it is important to note here that this lgp is NOT the parallel lgp */
    ompt_trace_record_t *record = add_trace_record_begin(emap, ompt_callback_thread_begin, frame, lgp, NULL, NULL);
    tribute_record_lexgion(lgp, record);
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
#ifdef OMPT_TRACING_SUPPORT
    ompt_trace_record_t *record = add_trace_record_end(emap, ompt_callback_thread_end, codeptr_ra);
    //printf("Thread: %d thread end, record: %d\n", thread_id, record->record_id);
#endif
    pop_lexgion(emap);

//    fini_thread_event_map(thread_id);
    ompt_measure_consume(&emap->thread_total);
}


static int
on_ompt_callback_control_tool(
        uint64_t command,
        uint64_t modifier,
        void *arg,
        const void *codeptr_ra)
{
    ompt_frame_t* omptTaskFrame;
    ompt_get_task_info(0, NULL, (ompt_data_t**) NULL, &omptTaskFrame, NULL, NULL);
    printf("%" PRIu64 ": ompt_event_control_tool: command=%" PRIu64 ", modifier=%" PRIu64 ", arg=%p, codeptr_ra=%p, current_task_frame.exit=%p, current_task_frame.reenter=%p \n", ompt_get_thread_data()->value, command, modifier, arg, codeptr_ra, omptTaskFrame->exit_frame, omptTaskFrame->enter_frame);
    return 0; //success
}

char *event_names[1000];

#define register_callback_t(name, type)                       \
do{                                                           \
  type f_##name = &on_##name;                                 \
  if (ompt_set_callback(name, (ompt_callback_t)f_##name) ==   \
      ompt_set_never)                                         \
    printf("0: Could not register callback '" #name "'\n");   \
  else                                                        \
    event_names[name] = #name;                                \
} while(0)

#define register_callback(name) register_callback_t(name, name##_t)

int ompt_initialize(
         ompt_function_lookup_t lookup,
         ompt_data_t *tool_data)
{
    ompt_set_callback = (ompt_set_callback_t) lookup("ompt_set_callback");
    ompt_get_task_info = (ompt_get_task_info_t) lookup("ompt_get_task_info");
    ompt_get_thread_data = (ompt_get_thread_data_t) lookup("ompt_get_thread_data");
    ompt_get_parallel_info = (ompt_get_parallel_info_t) lookup("ompt_get_parallel_info");
    ompt_get_unique_id = (ompt_get_unique_id_t) lookup("ompt_get_unique_id");

    ompt_get_num_places = (ompt_get_num_places_t) lookup("ompt_get_num_places");
    ompt_get_place_proc_ids = (ompt_get_place_proc_ids_t) lookup("ompt_get_place_proc_ids");
    ompt_get_place_num = (ompt_get_place_num_t) lookup("ompt_get_place_num");
    ompt_get_partition_place_nums = (ompt_get_partition_place_nums_t) lookup("ompt_get_partition_place_nums");
    ompt_get_proc_id = (ompt_get_proc_id_t) lookup("ompt_get_proc_id");
    ompt_enumerate_states = (ompt_enumerate_states_t) lookup("ompt_enumerate_states");
    ompt_enumerate_mutex_impls = (ompt_enumerate_mutex_impls_t) lookup("ompt_enumerate_mutex_impls");

//    register_callback(ompt_callback_idle_spin);
//    register_callback(ompt_callback_idle_suspend);
//    register_callback(ompt_callback_mutex_acquire);
//    register_callback_t(ompt_callback_mutex_acquired, ompt_callback_mutex_t);
//    register_callback_t(ompt_callback_mutex_released, ompt_callback_mutex_t);
//    register_callback(ompt_callback_nest_lock);
    register_callback(ompt_callback_sync_region);
    register_callback_t(ompt_callback_sync_region_wait, ompt_callback_sync_region_t);
    register_callback(ompt_callback_control_tool);
//    register_callback(ompt_callback_flush);
//    register_callback(ompt_callback_cancel);
    register_callback(ompt_callback_idle);
    register_callback(ompt_callback_implicit_task);
//    register_callback_t(ompt_callback_lock_init, ompt_callback_mutex_acquire_t);
//    register_callback_t(ompt_callback_lock_destroy, ompt_callback_mutex_t);
    register_callback(ompt_callback_work);
    register_callback(ompt_callback_master);
    register_callback(ompt_callback_parallel_begin);
    register_callback(ompt_callback_parallel_end);
    register_callback(ompt_callback_task_create);
    register_callback(ompt_callback_task_schedule);
    register_callback(ompt_callback_task_dependences);
    register_callback(ompt_callback_task_dependence);
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

    printf("0: NULL_POINTER=%p\n", (void*)NULL);
    printf("omptool initialized\n");

    return 1; //success
}

void ompt_finalize(ompt_data_t *tool_data) {
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
    list_parallel_lexgions(emap);
#ifdef OMPT_TRACING_SUPPORT
#ifdef OMPT_TRACING_GRAPHML_DUMP
    ompt_event_maps_to_graphml(event_maps);
#endif
#endif
}

ompt_start_tool_result_t* ompt_start_tool(
        unsigned int omp_version,
        const char *runtime_version)
{
    static ompt_start_tool_result_t ompt_start_tool_result = {&ompt_initialize,&ompt_finalize, 0};
    return &ompt_start_tool_result;
}
