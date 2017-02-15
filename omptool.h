#include "ompt.h"

typedef struct thread_event_map {



} thread_event_map_t;

extern thread_event_map_t *event_maps;

extern void add_trace_record(int thread_id, event_id, parallel_id, ....);

extern void add_trace_record_simple( .. );
