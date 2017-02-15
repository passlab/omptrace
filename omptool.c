#include "omptool.h"

/* so far we only handle max 256 threads */
thread_event_map event_maps[256];

void add_trace_record(int thread_id, event_id, parallel_id, ....) {

	event_maps[thread_id].event_id = event_id;
	  ...

}

