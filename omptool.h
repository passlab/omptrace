#include "ompt.h"

#define MAX_NUM_RECORDs 1000000
/* For one million records, we will need about 72Mbytes of memory to store
 * the tracing for each thread. 
 */
typedef struct thread_event_map {
  ompt_id_t thread_id;
  ompt_data_t thread_data;
  int counter;
  
  ompt_id_t parallel_id[MAX_NUM_RECORDS];
  int event_id[MAX_NUM_RECORDS];
  ompt_id_t graph_id[MAX_NUM_RECORDS];
  ompt_frame_t * frame[MAX_NUM_RECORDS];
  void * codeptr_ra[MAX_NUM_RECORDS];
  ompt_id_t target_id[MAX_NUM_RECORDS]
  
  unsigned long frequency[MAX_NUM_RECORDS];
  double time_stamp[MAX_NUM_RECORDS];
  double energy_consumed[MAX_NUM_RECORDS];
  
  //double energy_consumed;
  //double time_consumed;
} thread_event_map_t;

extern thread_event_map_t event_maps[];

extern void init_event_maps(int thread_id, ompt_data_t thread_data);
extern void add_trace_record(int thread_id, int event_id, ompt_frame_t * frame, void * codeptr_ra);
extern void set_trace_parallel_id(int thread_id, int counter, ompt_id_t parallel_id);

extern void add_trace_record_simple(int thread_id,int parallel_id, unsigned long frequency, double energy_consumed,double time_consumed);
