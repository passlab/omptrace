#include "omptool.h"

/* so far we only handle max 256 threads */
thread_event_map_t event_maps[256];

void add_trace_record(int thread_id,int parallel_id, unsigned long frequency, double energy_consumed, double time_consumed) 
{
	//event_maps[thread_id].event_id = event_id;
	  event_maps[thread_id].parallel_id = parallel_id;
	  event_maps[thread_id].frequency = frequency;
	  event_maps[thread_id].energy_consumed = energy_consumed;
	  event_maps[thread_id].time_consumed = time_consumed;
}

