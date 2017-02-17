#include "ompt.h"

#define MAX_NUM_RECORDS 1000000
/* For one million records, we will need about 72Mbytes of memory to store
 * the tracing for each thread. 
 */

/* The trace record struc contains every posibble information we want to store per event
 * though not all the fields are used for any events
 */
#define ompt_id_t int
typedef struct ompt_trace_record {
  ompt_id_t parallel_id;
  int event_id;
  ompt_id_t graph_id;
  ompt_frame_t * frame;
  void * codeptr_ra;
  ompt_id_t target_id;
  
  unsigned long frequency;
  double time_stamp;
  double energy_consumed;
} ompt_trace_record_t;

/* each thread has an object of thread_event_map that stores all the tracing record along 
 * during the execution
 */
typedef struct thread_event_map {
  ompt_id_t thread_id;
  ompt_data_t thread_data;
  int counter;
  ompt_trace_record_t records[MAX_NUM_RECORDS];
} thread_event_map_t;

/* this is the array for store all the event tracing records by all the threads */
extern thread_event_map_t event_maps[];

extern void init_event_maps(int thread_id, ompt_data_t thread_data);
extern void add_trace_record(int thread_id, int event_id, ompt_frame_t * frame, void * codeptr_ra);
extern void set_trace_parallel_id(int thread_id, int counter, ompt_id_t parallel_id);

