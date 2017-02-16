#include "omptool.h"

/* so far we only handle max 256 threads */
thread_event_map_t event_maps[256];

void init_event_maps(int thread_id, ompt_data_t thread_data) {
	event_maps[thread_id].thread_id = thread_id;
	event_maps[thread_id].thread_data = thread_data;
	event_maps[thread_id].counter = -1;
}

void add_trace_record(int thread_id, int event_id, ompt_frame_t * frame, void * codeptr_ra) {
	maps->counter ++;
	int counter = maps->counter;
	ompt_trace_record_t * rd = &event_maps[thread_id].records[counter];

	rd->event_id = event_id;
	rd->frame = frame;
	rd->codeptr_ra = codeptr_ra;
}

ompt_trace_record_t get_trace_record(int thread_id, int index) {
	return &event_maps[thread_id].records[index];
}

void set_trace_parallel_id(int thread_id, int counter, ompt_id_t parallel_id) {
	ompt_trace_record_t * rd = &event_maps[thread_id].records[counter];
	rd->parallel_id = parallel_id;
}

void add_trace_record(int thread_id,int parallel_id, unsigned long frequency, double energy_consumed, double time_consumed) 
{
	//event_maps[thread_id].event_id = event_id;
	  event_maps[thread_id].parallel_id = parallel_id;
	  event_maps[thread_id].frequency = frequency;
	  event_maps[thread_id].energy_consumed = energy_consumed;
	  event_maps[thread_id].time_consumed = time_consumed;
}

