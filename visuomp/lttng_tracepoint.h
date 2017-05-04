#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER lttng_visuomp

#if !defined(_TRACEPOINT_LTTNG_TRACEPOINT_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define _TRACEPOINT_LTTNG_TRACEPOINT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lttng/tracepoint.h>
#include <stdbool.h>
#include <ompt.h>

TRACEPOINT_EVENT(lttng_visuomp, parallel_begin,
	TP_ARGS(int, gtid,
		ompt_frame_t*, parent_task_frame,
		ompt_data_t*, parallel_data,
  		uint32_t, requested_team_size,
  		void *, codeptr_ra
	),
	TP_FIELDS(
		ctf_integer(int, gtid, gtid)
//		ctf_integer_hex(long int, frame, parent_task_frame->exit_runtime_frame)
		ctf_integer_hex(long int, frame, parent_task_frame->reenter_runtime_frame)
		ctf_integer_hex(long int, parallel_id, parallel_data->value)
		ctf_integer(int, team_size, requested_team_size)
		ctf_integer_hex(long int, codeptr_ra, codeptr_ra)
	)
)

TRACEPOINT_EVENT(lttng_visuomp, parallel_end,
	TP_ARGS(int, gtid,
		ompt_data_t*, parallel_data,
		ompt_task_data_t*, task_data,
  		void *, codeptr_ra
	),
	TP_FIELDS(
		ctf_integer(int, gtid, gtid)
		ctf_integer_hex(long int, parallel_id, parallel_data->value)
		ctf_integer_hex(long int, task_id, task_data->value)
		ctf_integer_hex(long int, codeptr_ra, codeptr_ra)
	)
)

TRACEPOINT_EVENT(lttng_visuomp, implicit_task_begin,
	TP_ARGS(int, gtid,
		ompt_data_t*, parallel_data,
    		ompt_data_t*, task_data,
  		unsigned int, team_size,
  		unsigned int, thread_num
	),
	TP_FIELDS(
		ctf_integer(int, gtid, gtid)
		ctf_integer_hex(long int, parallel_id, parallel_data->value)
		ctf_integer_hex(long int, task_id, task_data->value)
		ctf_integer(int, team_size, team_size)
		ctf_integer_hex(int, thread_num, thread_num)
	)
)

TRACEPOINT_EVENT(lttng_visuomp, implicit_task_end,
	TP_ARGS(int, gtid,
		ompt_data_t*, parallel_data,
    		ompt_data_t*, task_data,
  		unsigned int, team_size,
  		unsigned int, thread_num
	),
	TP_FIELDS(
		ctf_integer(int, gtid, gtid)
		ctf_integer_hex(long int, parallel_id, (parallel_data)?parallel_data->value:0)
		ctf_integer_hex(long int, task_id, task_data->value)
		ctf_integer(int, team_size, team_size)
		ctf_integer_hex(int, thread_num, thread_num)
	)
)

TRACEPOINT_EVENT(lttng_visuomp, thread_begin,
	TP_ARGS(int, gtid,
		ompt_data_t*, thread_data
	),
	TP_FIELDS(
		ctf_integer(int, gtid, gtid)
		ctf_integer_hex(long int, thread_data_id, thread_data->value)
	)
)

TRACEPOINT_EVENT(lttng_visuomp, thread_end,
	TP_ARGS(int, gtid,
		ompt_data_t*, thread_data
	),
	TP_FIELDS(
		ctf_integer(int, gtid, gtid)
		ctf_integer_hex(long int, thread_data_id, thread_data->value)
	)
)

TRACEPOINT_EVENT(lttng_visuomp, thread_idle_begin,
	TP_ARGS(int, gtid,
		ompt_data_t*, thread_data
	),
	TP_FIELDS(
		ctf_integer(int, gtid, gtid)
		ctf_integer_hex(long int, thread_data_id, thread_data->value)
	)
)

TRACEPOINT_EVENT(lttng_visuomp, thread_idle_end,
	TP_ARGS(int, gtid,
		ompt_data_t*, thread_data
	),
	TP_FIELDS(
		ctf_integer(int, gtid, gtid)
		ctf_integer_hex(long int, thread_data_id, thread_data->value)
	)
)

#endif /* _TRACEPOINT_LTTNG_TRACEPOINT_H */

#undef TRACEPOINT_INCLUDE_FILE
#define TRACEPOINT_INCLUDE_FILE ./lttng_tracepoint.h

/* This part must be outside ifdef protection */
#include <lttng/tracepoint-event.h>

#ifdef __cplusplus 
}
#endif
