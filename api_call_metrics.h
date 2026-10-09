#pragma once

#include <Windows.h>
#include <string.h>

/* No allocation in hook entry (in particular NtAllocateVirtualMemory).
 * Windows stacks grow down. Returned/unwound frames are retired lazily on the
 * next entry, or when an enclosing invocation logs. Never fall back to another
 * invocation if a frame is absent, exhausted, or already consumed. */
#define API_CALL_METRICS_MAX_FRAMES 64

typedef struct _api_call_metrics_frame {
	ULONG_PTR entry_sp;
	ULONG_PTR log_sp;
	const char *name;
	LARGE_INTEGER start;
	BOOL valid;
} api_call_metrics_frame;

typedef struct _api_call_metrics_state {
	unsigned int count;
	api_call_metrics_frame frames[API_CALL_METRICS_MAX_FRAMES];
} api_call_metrics_state;

static __inline ULONG_PTR api_call_metrics_log_sp(ULONG_PTR sp, BOOL notail, unsigned int numargs)
{
	if (!notail)
		return sp;
#ifdef _WIN64
	/* Saved registers/flags + shadow space + return address; notail also
	 * copies stack arguments and allocates another shadow area for >4 args.
	 * Keep in sync with hook_create_pre_tramp_notail in hooking_64.c. */
	if (numargs > 4)
		return sp - 0xa0 - ((numargs - 4 + 1) & ~1U) * 8;
	return sp - 0x80;
#else
	/* pushfd/pushad + copied arguments + return address (hooking_32.c). */
	return sp - 40 - numargs * 4;
#endif
}

static __inline api_call_metrics_frame *api_call_metrics_enter(api_call_metrics_state *state,
	ULONG_PTR sp, ULONG_PTR log_sp, const char *name)
{
	api_call_metrics_frame *frame;
	while (state->count && state->frames[state->count - 1].entry_sp <= sp)
		state->count--;
	if (state->count == API_CALL_METRICS_MAX_FRAMES)
		return NULL;
	frame = &state->frames[state->count++];
	frame->entry_sp = sp;
	frame->log_sp = log_sp;
	frame->name = name;
	frame->start.QuadPart = 0;
	frame->valid = FALSE;
	return frame;
}

static __inline BOOL api_call_metrics_take(api_call_metrics_state *state,
	ULONG_PTR sp, const char *name, LARGE_INTEGER *start)
{
	unsigned int i;
	if (!sp)
		return FALSE; /* Auxiliary events have no API invocation. */
	for (i = state->count; i > 0; i--) {
		api_call_metrics_frame *frame = &state->frames[i - 1];
		/* Alt_ implementations are tail-jumped with the original entry SP. */
		if ((frame->entry_sp == sp || frame->log_sp == sp) && !strcmp(frame->name, name)) {
			BOOL valid = frame->valid;
			*start = frame->start;
			frame->valid = FALSE;
			state->count = i; /* Retire returned/unlogged nested invocations. */
			return valid;
		}
	}
	return FALSE;
}

static __inline BOOL api_call_metrics_duration(LARGE_INTEGER start, LARGE_INTEGER end,
	LARGE_INTEGER frequency, double *duration_us)
{
	if (frequency.QuadPart <= 0 || end.QuadPart < start.QuadPart)
		return FALSE;
	*duration_us = ((double)(end.QuadPart - start.QuadPart) * 1000000.0) /
		(double)frequency.QuadPart;
	return TRUE;
}
