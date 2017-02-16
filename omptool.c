#include "omptool.h"

/* so far we only handle max 256 threads */
thread_event_map_t event_maps[256];

void init_event_maps(int thread_id, ompt_data_t thread_data) {
	event_maps[thread_id].thread_id = thread_id;
	event_maps[thread_id].thread_data = thread_data;
	event_maps[thread_id].counter = -1;
}

void add_trace_record(int thread_id, int event_id, ompt_frame_t * frame, void * codeptr_ra) {
	thread_event_map_t * maps = &event_maps[thread_id];
	maps->counter ++;
	int counter = maps->counter;
	maps->event_id[counter] = event_id;
	maps->frame[counter] = frame;
	maps->codeptr_ra[counter] = codeptr_ra;
}

void set_trace_parallel_id(int thread_id, int counter, ompt_id_t parallel_id) {
	thread_event_map_t * maps = &event_maps[thread_id];
	int counter = maps->counter;
	maps->parallel_id[counter] = parallel_id;
}

void add_trace_record(int thread_id,int parallel_id, unsigned long frequency, double energy_consumed, double time_consumed) 
{
	//event_maps[thread_id].event_id = event_id;
	  event_maps[thread_id].parallel_id = parallel_id;
	  event_maps[thread_id].frequency = frequency;
	  event_maps[thread_id].energy_consumed = energy_consumed;
	  event_maps[thread_id].time_consumed = time_consumed;
}

