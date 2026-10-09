/* Standalone regression: production state helpers, generated machine-code
 * trampolines, and the production BSON metrics emitter. No monitor injection. */
#define WIN32_LEAN_AND_MEAN
#include <assert.h>
#include <stdio.h>
#include <intrin.h>
#include <Windows.h>
#include <psapi.h>
#include "../api_call_metrics.h"
#include "../bson/bson.h"

#include "../log.h"

volatile LONG g_log_index;
static api_call_metrics_state state;
static LONGLONG sequence;
static int logs, pre_logs;
static BOOL fail_start;
int WINAPI enter_hook(hook_t *h, ULONG_PTR sp, ULONG_PTR unused)
{
	api_call_metrics_frame *frame = api_call_metrics_enter(&state, sp,
		api_call_metrics_log_sp(sp, h->notail, h->numargs), h->funcname);
	assert(frame);
	frame->start.QuadPart = ++sequence;
	frame->valid = !fail_start;
	return 1;
}
struct _g_config g_config;
static void add_unwind_info(hook_t *h) { }
void emit_rel(unsigned char *buf, unsigned char *source, unsigned char *target)
{
	*(DWORD *)buf = (DWORD)(target - (source + 4));
}
#ifdef _WIN64
#include "tramp-64.h"
#else
#include "tramp-32.h"
#endif

typedef ULONG_PTR (WINAPI *call6)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
static call6 inner;
static call6 same;
static LONGLONG outer_start;
#define TAKE(name, expected) do { \
	LARGE_INTEGER taken_start; \
	assert(api_call_metrics_take(&state, (ULONG_PTR)_AddressOfReturnAddress(), name, &taken_start)); \
	assert(taken_start.QuadPart == (expected)); \
	assert(!api_call_metrics_take(&state, (ULONG_PTR)_AddressOfReturnAddress(), name, &taken_start)); \
	logs++; \
} while (0)

void loq(int index, const char *category, const char *name, int success,
 ULONG_PTR result, ULONG_PTR invocation_sp, const char *fmt, ...)
{
 LARGE_INTEGER start;
 if (!invocation_sp) {
  api_call_metrics_state saved = state;
  assert(!api_call_metrics_take(&state, invocation_sp, name, &start));
  assert(!memcmp(&state, &saved, sizeof(state)));
  return;
 }
 assert(api_call_metrics_take(&state, invocation_sp, name, &start));
 assert(start.QuadPart == sequence);
 assert(!api_call_metrics_take(&state, invocation_sp, name, &start));
 logs++;
}

static __declspec(noinline) void dll_notification(void)
{
	api_call_metrics_state saved = state;
	int ret = 0;
	LOQ_event_void("system", "");
	assert(!memcmp(&saved, &state, sizeof(state)));
}

static __declspec(noinline) ULONG_PTR WINAPI New_Inner(ULONG_PTR a, ULONG_PTR b,
	ULONG_PTR c, ULONG_PTR d, ULONG_PTR e, ULONG_PTR f)
{
	LARGE_INTEGER ignored;
	unsigned int count = state.count;
	assert(!api_call_metrics_take(&state, (ULONG_PTR)_AddressOfReturnAddress(),
		"DllLoadNotification", &ignored));
	assert(!api_call_metrics_take(&state, 0, "Inner", &ignored));
	assert(state.count == count);
	{ int ret = 0; LOQ_void("test", ""); }
	return a + b + c + d + e + f;
}
static __declspec(noinline) ULONG_PTR WINAPI New_Outer(ULONG_PTR a, ULONG_PTR b,
	ULONG_PTR c, ULONG_PTR d, ULONG_PTR e, ULONG_PTR f)
{
	outer_start = sequence;
	dll_notification();
	assert(inner(a, b, c, d, e, f) == 21);
	dll_notification();
	TAKE("Outer", outer_start);
	return 42;
}
static __declspec(noinline) ULONG_PTR WINAPI New_Same(ULONG_PTR a, ULONG_PTR b,
	ULONG_PTR c, ULONG_PTR d, ULONG_PTR e, ULONG_PTR f)
{
	LONGLONG start = sequence;
	if (a)
		assert(same(a - 1, b, c, d, e, f) == 7);
	TAKE("Same", start);
	return 7;
}
static __declspec(noinline) ULONG_PTR WINAPI New_Pre(ULONG_PTR a, ULONG_PTR b,
	ULONG_PTR c, ULONG_PTR d, ULONG_PTR e, ULONG_PTR f)
{
	if (pre_logs) {
		TAKE("Notail", sequence);
		return 0; /* New_ logs, trampoline continues to original. */
	}
	return 1; /* Trampoline tail-jumps to Alt_. */
}
static __declspec(noinline) ULONG_PTR WINAPI Alt_Pre(ULONG_PTR a, ULONG_PTR b,
	ULONG_PTR c, ULONG_PTR d, ULONG_PTR e, ULONG_PTR f)
{
	if (!pre_logs) {
		TAKE("Notail", sequence);
	}
	return 21;
}

#define NOTAIL_FUNCTIONS(suffix, ...) \
static __declspec(noinline) ULONG_PTR WINAPI New_Pre##suffix(__VA_ARGS__) { \
	if (pre_logs) { TAKE("Notail", sequence); return 0; } \
	return 1; \
} \
static __declspec(noinline) ULONG_PTR WINAPI Alt_Pre##suffix(__VA_ARGS__) { \
	if (!pre_logs) { TAKE("Notail", sequence); } \
	return 21; \
}
NOTAIL_FUNCTIONS(4, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d)
NOTAIL_FUNCTIONS(5, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e)
static call6 make_hook(hook_t *h, const char *name, void *new_func, void *alt_func, int notail, int numargs)
{
	unsigned char *p;
	h->funcname = name;
	h->new_func = new_func;
	h->alt_func = alt_func;
	h->notail = notail;
	h->numargs = (unsigned char)numargs;
	h->hookdata = VirtualAlloc(NULL, sizeof(hook_data_t), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	assert(h->hookdata);
	p = h->hookdata->tramp;
#ifdef _WIN64
	memcpy(p, "\xff\x25\x00\x00\x00\x00", 6);
	*(ULONG_PTR *)(p + 6) = (ULONG_PTR)alt_func;
#else
	p[0] = 0xe9;
	emit_rel(p + 1, p + 1, alt_func);
#endif
	if (notail) hook_create_pre_tramp_notail(h);
	else hook_create_pre_tramp(h);
	FlushInstructionCache(GetCurrentProcess(), h->hookdata, sizeof(hook_data_t));
	return (call6)h->hookdata->pre_tramp;
}

static bson g_bson[1];
static LARGE_INTEGER g_qpc_frequency;
static BOOL end_valid = TRUE;
static LONGLONG end_value = 150;
static BOOL test_qpc(LARGE_INTEGER *value)
{
	value->QuadPart = end_value;
	return end_valid;
}
#define QueryPerformanceCounter test_qpc
#include "metrics-bson.h"
#undef QueryPerformanceCounter
static void check_bson(BOOL valid, BOOL expected)
{
	LARGE_INTEGER start;
	bson_iterator it;
	const char *keys[] = {"qpc_start", "qpc_end", "qpc_frequency", "duration_us"};
	unsigned int i;
	start.QuadPart = 100;
	bson_init(g_bson);
	log_api_call_metrics(valid, start);
	bson_finish(g_bson);
	for (i = 0; i < 4; i++)
		assert((bson_find(&it, g_bson, keys[i]) != BSON_EOO) == expected);
	if (expected) {
		assert(bson_find(&it, g_bson, "qpc_start") == BSON_LONG);
		assert(bson_iterator_long(&it) == 100);
		assert(bson_find(&it, g_bson, "qpc_end") == BSON_LONG);
		assert(bson_iterator_long(&it) == end_value);
		assert(bson_find(&it, g_bson, "qpc_frequency") == BSON_LONG);
		assert(bson_iterator_long(&it) == 1000);
		assert(bson_find(&it, g_bson, "duration_us") == BSON_DOUBLE);
		assert(bson_iterator_double(&it) == (end_value - 100) * 1000.0);
	}
	assert(bson_find(&it, g_bson, "working_set_bytes") == BSON_LONG);
	bson_destroy(g_bson);
}

static void test_state(void)
{
	api_call_metrics_state s = {0};
	api_call_metrics_frame *frame;
	LARGE_INTEGER start;
	unsigned int i;
	frame = api_call_metrics_enter(&s, 10000, 10000, "Outer");
	frame->start.QuadPart = 123;
	frame->valid = TRUE;
	api_call_metrics_enter(&s, 9000, 9000, "NoLog");
	/* An unlogged or unwound child cannot strand the enclosing start. */
	assert(api_call_metrics_take(&s, 10000, "Outer", &start));
	assert(start.QuadPart == 123 && s.count == 1);
	/* Reusing the same stack slot replaces rather than reuses old state. */
	api_call_metrics_enter(&s, 10000, 10000, "Outer");
	assert(!api_call_metrics_take(&s, 10000, "Outer", &start));
	memset(&s, 0, sizeof(s));
	for (i = 0; i < API_CALL_METRICS_MAX_FRAMES; i++) {
		frame = api_call_metrics_enter(&s, 10000 - i * 10, 10000 - i * 10, "Same");
		assert(frame);
		frame->start.QuadPart = i + 1;
		frame->valid = TRUE;
	}
	assert(!api_call_metrics_enter(&s, 1, 1, "Same"));
	assert(!api_call_metrics_take(&s, 1, "Same", &start));
	assert(api_call_metrics_take(&s, 10000, "Same", &start));
	assert(start.QuadPart == 1);
}

int main(void)
{
	hook_t hi = {0}, ho = {0}, hs = {0}, hn = {0};
	hook_t hn4 = {0}, hn5 = {0};
	call6 outer, notail;
	typedef ULONG_PTR (WINAPI *call4)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
	typedef ULONG_PTR (WINAPI *call5)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
	call4 notail4;
	call5 notail5;
	int before, i;
	test_state();
	inner = make_hook(&hi, "Inner", New_Inner, NULL, 0, 6);
	outer = make_hook(&ho, "Outer", New_Outer, NULL, 0, 6);
	same = make_hook(&hs, "Same", New_Same, NULL, 0, 6);
	before = logs;
	for (i = 0; i < 10; i++) assert(outer(1, 2, 3, 4, 5, 6) == 42);
	assert(logs - before == 20);
	assert(same(3, 0, 0, 0, 0, 0) == 7);
	notail = make_hook(&hn, "Notail", New_Pre, Alt_Pre, 1, 6);
	pre_logs = 1;
	assert(notail(1, 2, 3, 4, 5, 6) == 21);
	pre_logs = 0;
	assert(notail(1, 2, 3, 4, 5, 6) == 21);
	notail4 = (call4)make_hook(&hn4, "Notail", New_Pre4, Alt_Pre4, 1, 4);
	notail5 = (call5)make_hook(&hn5, "Notail", New_Pre5, Alt_Pre5, 1, 5);
	for (pre_logs = 0; pre_logs <= 1; pre_logs++) {
		assert(notail4(1, 2, 3, 4) == 21);
		assert(notail5(1, 2, 3, 4, 5) == 21);
	}
	g_qpc_frequency.QuadPart = 1000;
	check_bson(TRUE, TRUE);
	end_value = 100; check_bson(TRUE, TRUE); /* Zero duration is valid. */
	check_bson(FALSE, FALSE); /* Start capture failure or auxiliary log. */
	end_valid = FALSE; check_bson(TRUE, FALSE);
	end_valid = TRUE; end_value = 99; check_bson(TRUE, FALSE);
	end_value = 150; g_qpc_frequency.QuadPart = 0; check_bson(TRUE, FALSE);
	g_qpc_frequency.QuadPart = -1; check_bson(TRUE, FALSE);
	puts("API metrics: state, native trampolines, nesting, notail, BSON passed");
	return 0;
}
