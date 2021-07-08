/*-
 *   BSD LICENSE
 *
 *   Copyright (c) Intel Corporation.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "spdk/stdinc.h"
#include "spdk/jsonrpc.h"
#include "spdk/rpc.h"
#include "spdk/event.h"
#include "spdk/util.h"
#include "spdk/env.h"

#if defined __has_include
#if __has_include(<ncurses/panel.h>)
#include <ncurses/ncurses.h>
#include <ncurses/panel.h>
#include <ncurses/menu.h>
#else
#include <ncurses.h>
#include <panel.h>
#include <menu.h>
#endif
#else
#include <ncurses.h>
#include <panel.h>
#include <menu.h>
#endif

#define RPC_MAX_THREADS 1024
#define RPC_MAX_POLLERS 1024
#define RPC_MAX_CORES 255
#define MAX_THREAD_NAME 128
#define MAX_POLLER_NAME 128
#define MAX_THREADS 4096
#define RR_MAX_VALUE 255

#define MAX_STRING_LEN 12289 /* 3x 4k monitors + 1 */
#define TAB_WIN_HEIGHT 3
#define TAB_WIN_LOCATION_ROW 1
#define TABS_SPACING 2
#define TABS_LOCATION_ROW 4
#define TABS_LOCATION_COL 0
#define TABS_DATA_START_ROW 3
#define TABS_DATA_START_COL 2
#define TABS_COL_COUNT 10
#define MENU_WIN_HEIGHT 3
#define MENU_WIN_SPACING 4
#define MENU_WIN_LOCATION_COL 0
#define RR_WIN_WIDTH 32
#define RR_WIN_HEIGHT 5
#define MAX_THREAD_NAME_LEN 26
#define MAX_THREAD_COUNT_STR_LEN 14
#define MAX_POLLER_NAME_LEN 36
#define MAX_POLLER_COUNT_STR_LEN 16
#define MAX_POLLER_TYPE_STR_LEN 8
#define MAX_POLLER_IND_STR_LEN 8
#define MAX_CORE_MASK_STR_LEN 16
#define MAX_CORE_STR_LEN 6
#define MAX_CORE_FREQ_STR_LEN 18
#define MAX_TIME_STR_LEN 12
#define MAX_POLLER_RUN_COUNT 20
#define MAX_PERIOD_STR_LEN 12
#define WINDOW_HEADER 12
#define FROM_HEX 16
#define THREAD_WIN_WIDTH 69
#define THREAD_WIN_HEIGHT 9
#define THREAD_WIN_FIRST_COL 2
#define CORE_WIN_FIRST_COL 16
#define CORE_WIN_WIDTH 48
#define CORE_WIN_HEIGHT 11
#define POLLER_WIN_HEIGHT 8
#define POLLER_WIN_WIDTH 64
#define POLLER_WIN_FIRST_COL 14
#define FIRST_DATA_ROW 7

enum tabs {
	THREADS_TAB,
	POLLERS_TAB,
	CORES_TAB,
	NUMBER_OF_TABS,
};

enum spdk_poller_type {
	SPDK_ACTIVE_POLLER,
	SPDK_TIMED_POLLER,
	SPDK_PAUSED_POLLER,
	SPDK_POLLER_TYPES_COUNT,
};

struct col_desc {
	const char *name;
	uint8_t name_len;
	uint8_t max_data_string;
	bool disabled;
};

struct run_counter_history {
	char *poller_name;
	uint64_t thread_id;
	uint64_t last_run_counter;
	TAILQ_ENTRY(run_counter_history) link;
};

struct core_info {
	uint32_t core;
	uint64_t threads_count;
	uint64_t pollers_count;
	uint64_t idle;
	uint64_t last_idle;
	uint64_t busy;
	uint64_t last_busy;
};

uint8_t g_sleep_time = 1;
uint16_t g_selected_row;
uint16_t g_max_selected_row;
struct rpc_thread_info *g_thread_info[MAX_THREADS];
const char *poller_type_str[SPDK_POLLER_TYPES_COUNT] = {"Active", "Timed", "Paused"};
const char *g_tab_title[NUMBER_OF_TABS] = {"[1] THREADS", "[2] POLLERS", "[3] CORES"};
struct spdk_jsonrpc_client *g_rpc_client;
static TAILQ_HEAD(, run_counter_history) g_run_counter_history = TAILQ_HEAD_INITIALIZER(
			g_run_counter_history);
struct core_info g_cores_history[RPC_MAX_CORES];
WINDOW *g_menu_win, *g_tab_win[NUMBER_OF_TABS], *g_tabs[NUMBER_OF_TABS];
PANEL *g_panels[NUMBER_OF_TABS];
uint16_t g_max_row, g_max_col;
uint16_t g_data_win_size, g_max_data_rows;
uint32_t g_last_threads_count, g_last_pollers_count, g_last_cores_count;
uint8_t g_current_sort_col[NUMBER_OF_TABS] = {0, 0, 0};
bool g_interval_data = true;
static struct col_desc g_col_desc[NUMBER_OF_TABS][TABS_COL_COUNT] = {
	{	{.name = "Thread name", .max_data_string = MAX_THREAD_NAME_LEN},
		{.name = "Core", .max_data_string = MAX_CORE_STR_LEN},
		{.name = "Active pollers", .max_data_string = MAX_POLLER_COUNT_STR_LEN},
		{.name = "Timed pollers", .max_data_string = MAX_POLLER_COUNT_STR_LEN},
		{.name = "Paused pollers", .max_data_string = MAX_POLLER_COUNT_STR_LEN},
		{.name = "Idle [us]", .max_data_string = MAX_TIME_STR_LEN},
		{.name = "Busy [us]", .max_data_string = MAX_TIME_STR_LEN},
		{.name = (char *)NULL}
	},
	{	{.name = "Poller name", .max_data_string = MAX_POLLER_NAME_LEN},
		{.name = "Type", .max_data_string = MAX_POLLER_TYPE_STR_LEN},
		{.name = "On thread", .max_data_string = MAX_THREAD_NAME_LEN},
		{.name = "Run count", .max_data_string = MAX_POLLER_RUN_COUNT},
		{.name = "Period [us]", .max_data_string = MAX_PERIOD_STR_LEN},
		{.name = "Status", .max_data_string = MAX_POLLER_IND_STR_LEN},
		{.name = (char *)NULL}
	},
	{	{.name = "Core", .max_data_string = MAX_CORE_STR_LEN},
		{.name = "Thread count", .max_data_string = MAX_THREAD_COUNT_STR_LEN},
		{.name = "Poller count", .max_data_string = MAX_POLLER_COUNT_STR_LEN},
		{.name = "Idle [us]", .max_data_string = MAX_TIME_STR_LEN},
		{.name = "Busy [us]", .max_data_string = MAX_TIME_STR_LEN},
		{.name = "Frequency [MHz]", .max_data_string = MAX_CORE_FREQ_STR_LEN},
		{.name = (char *)NULL}
	}
};

struct rpc_thread_info {
	char *name;
	uint64_t id;
	uint32_t core_num;
	char *cpumask;
	uint64_t busy;
	uint64_t last_busy;
	uint64_t idle;
	uint64_t last_idle;
	uint64_t active_pollers_count;
	uint64_t timed_pollers_count;
	uint64_t paused_pollers_count;
};

struct rpc_threads {
	uint64_t threads_count;
	struct rpc_thread_info thread_info[RPC_MAX_THREADS];
};

struct rpc_threads_stats {
	uint64_t tick_rate;
	struct rpc_threads threads;
};

struct rpc_poller_info {
	char *name;
	char *state;
	uint64_t run_count;
	uint64_t busy_count;
	uint64_t period_ticks;
	enum spdk_poller_type type;
	char thread_name[MAX_THREAD_NAME];
	uint64_t thread_id;
};

struct rpc_pollers {
	uint64_t pollers_count;
	struct rpc_poller_info pollers[RPC_MAX_POLLERS];
};

struct rpc_poller_thread_info {
	char *name;
	uint64_t id;
	struct rpc_pollers active_pollers;
	struct rpc_pollers timed_pollers;
	struct rpc_pollers paused_pollers;
};

struct rpc_pollers_threads {
	uint64_t threads_count;
	struct rpc_poller_thread_info threads[RPC_MAX_THREADS];
};

struct rpc_pollers_stats {
	uint64_t tick_rate;
	struct rpc_pollers_threads pollers_threads;
};

struct rpc_core_thread_info {
	char *name;
	uint64_t id;
	char *cpumask;
	uint64_t elapsed;
};

struct rpc_core_threads {
	uint64_t threads_count;
	struct rpc_core_thread_info thread[RPC_MAX_THREADS];
};

struct rpc_core_info {
	uint32_t lcore;
	uint64_t busy;
	uint64_t idle;
	uint32_t core_freq;
	struct rpc_core_threads threads;
};

struct rpc_cores {
	uint64_t cores_count;
	struct rpc_core_info core[RPC_MAX_CORES];
};

struct rpc_cores_stats {
	uint64_t tick_rate;
	struct rpc_cores cores;
};

struct rpc_threads_stats g_threads_stats;
struct rpc_pollers_stats g_pollers_stats;
struct rpc_cores_stats g_cores_stats;
struct rpc_poller_info g_pollers_history[RPC_MAX_POLLERS];
struct rpc_thread_info g_thread_history[RPC_MAX_THREADS];

static void
init_str_len(void)
{
	int i, j;

	for (i = 0; i < NUMBER_OF_TABS; i++) {
		for (j = 0; g_col_desc[i][j].name != NULL; j++) {
			g_col_desc[i][j].name_len = strlen(g_col_desc[i][j].name);
		}
	}
}

static void
free_rpc_threads_stats(struct rpc_threads_stats *req)
{
	uint64_t i;

	for (i = 0; i < req->threads.threads_count; i++) {
		free(req->threads.thread_info[i].name);
		req->threads.thread_info[i].name = NULL;
		free(req->threads.thread_info[i].cpumask);
		req->threads.thread_info[i].cpumask = NULL;
	}
}

static const struct spdk_json_object_decoder rpc_thread_info_decoders[] = {
	{"name", offsetof(struct rpc_thread_info, name), spdk_json_decode_string},
	{"id", offsetof(struct rpc_thread_info, id), spdk_json_decode_uint64},
	{"cpumask", offsetof(struct rpc_thread_info, cpumask), spdk_json_decode_string},
	{"busy", offsetof(struct rpc_thread_info, busy), spdk_json_decode_uint64},
	{"idle", offsetof(struct rpc_thread_info, idle), spdk_json_decode_uint64},
	{"active_pollers_count", offsetof(struct rpc_thread_info, active_pollers_count), spdk_json_decode_uint64},
	{"timed_pollers_count", offsetof(struct rpc_thread_info, timed_pollers_count), spdk_json_decode_uint64},
	{"paused_pollers_count", offsetof(struct rpc_thread_info, paused_pollers_count), spdk_json_decode_uint64},
};

static int
rpc_decode_threads_object(const struct spdk_json_val *val, void *out)
{
	struct rpc_thread_info *info = out;

	return spdk_json_decode_object(val, rpc_thread_info_decoders,
				       SPDK_COUNTOF(rpc_thread_info_decoders), info);
}

static int
rpc_decode_threads_array(const struct spdk_json_val *val, void *out)
{
	struct rpc_threads *threads = out;

	return spdk_json_decode_array(val, rpc_decode_threads_object, threads->thread_info, RPC_MAX_THREADS,
				      &threads->threads_count, sizeof(struct rpc_thread_info));
}

static const struct spdk_json_object_decoder rpc_threads_stats_decoders[] = {
	{"tick_rate", offsetof(struct rpc_threads_stats, tick_rate), spdk_json_decode_uint64},
	{"threads", offsetof(struct rpc_threads_stats, threads), rpc_decode_threads_array},
};

static void
free_rpc_poller(struct rpc_poller_info *poller)
{
	free(poller->name);
	poller->name = NULL;
	free(poller->state);
	poller->state = NULL;
}

static void
free_rpc_pollers_stats(struct rpc_pollers_stats *req)
{
	struct rpc_poller_thread_info *thread;
	uint64_t i, j;

	for (i = 0; i < req->pollers_threads.threads_count; i++) {
		thread = &req->pollers_threads.threads[i];

		for (j = 0; j < thread->active_pollers.pollers_count; j++) {
			free_rpc_poller(&thread->active_pollers.pollers[j]);
		}

		for (j = 0; j < thread->timed_pollers.pollers_count; j++) {
			free_rpc_poller(&thread->timed_pollers.pollers[j]);
		}

		for (j = 0; j < thread->paused_pollers.pollers_count; j++) {
			free_rpc_poller(&thread->paused_pollers.pollers[j]);
		}

		free(thread->name);
		thread->name = NULL;
	}
}

static void
free_rpc_cores_stats(struct rpc_cores_stats *req)
{
	struct rpc_core_info *core;
	struct rpc_core_thread_info *thread;
	uint64_t i, j;

	for (i = 0; i < req->cores.cores_count; i++) {
		core = &req->cores.core[i];

		for (j = 0; j < core->threads.threads_count; j++) {
			thread = &core->threads.thread[j];

			free(thread->name);
			free(thread->cpumask);
		}
	}
}

static const struct spdk_json_object_decoder rpc_pollers_decoders[] = {
	{"name", offsetof(struct rpc_poller_info, name), spdk_json_decode_string},
	{"state", offsetof(struct rpc_poller_info, state), spdk_json_decode_string},
	{"run_count", offsetof(struct rpc_poller_info, run_count), spdk_json_decode_uint64},
	{"busy_count", offsetof(struct rpc_poller_info, busy_count), spdk_json_decode_uint64},
	{"period_ticks", offsetof(struct rpc_poller_info, period_ticks), spdk_json_decode_uint64, true},
};

static int
rpc_decode_pollers_object(const struct spdk_json_val *val, void *out)
{
	struct rpc_poller_info *info = out;

	return spdk_json_decode_object(val, rpc_pollers_decoders, SPDK_COUNTOF(rpc_pollers_decoders), info);
}

static int
rpc_decode_pollers_array(const struct spdk_json_val *val, void *out)
{
	struct rpc_pollers *pollers = out;

	return spdk_json_decode_array(val, rpc_decode_pollers_object, pollers->pollers, RPC_MAX_THREADS,
				      &pollers->pollers_count, sizeof(struct rpc_poller_info));
}

static const struct spdk_json_object_decoder rpc_pollers_threads_decoders[] = {
	{"name", offsetof(struct rpc_poller_thread_info, name), spdk_json_decode_string},
	{"id", offsetof(struct rpc_poller_thread_info, id), spdk_json_decode_uint64},
	{"active_pollers", offsetof(struct rpc_poller_thread_info, active_pollers), rpc_decode_pollers_array},
	{"timed_pollers", offsetof(struct rpc_poller_thread_info, timed_pollers), rpc_decode_pollers_array},
	{"paused_pollers", offsetof(struct rpc_poller_thread_info, paused_pollers), rpc_decode_pollers_array},
};

static int
rpc_decode_pollers_threads_object(const struct spdk_json_val *val, void *out)
{
	struct rpc_poller_thread_info *info = out;

	return spdk_json_decode_object(val, rpc_pollers_threads_decoders,
				       SPDK_COUNTOF(rpc_pollers_threads_decoders), info);
}

static int
rpc_decode_pollers_threads_array(const struct spdk_json_val *val, void *out)
{
	struct rpc_pollers_threads *pollers_threads = out;

	return spdk_json_decode_array(val, rpc_decode_pollers_threads_object, pollers_threads->threads,
				      RPC_MAX_THREADS, &pollers_threads->threads_count, sizeof(struct rpc_poller_thread_info));
}

static const struct spdk_json_object_decoder rpc_pollers_stats_decoders[] = {
	{"tick_rate", offsetof(struct rpc_pollers_stats, tick_rate), spdk_json_decode_uint64},
	{"threads", offsetof(struct rpc_pollers_stats, pollers_threads), rpc_decode_pollers_threads_array},
};

static const struct spdk_json_object_decoder rpc_core_thread_info_decoders[] = {
	{"name", offsetof(struct rpc_core_thread_info, name), spdk_json_decode_string},
	{"id", offsetof(struct rpc_core_thread_info, id), spdk_json_decode_uint64},
	{"cpumask", offsetof(struct rpc_core_thread_info, cpumask), spdk_json_decode_string},
	{"elapsed", offsetof(struct rpc_core_thread_info, elapsed), spdk_json_decode_uint64},
};

static int
rpc_decode_core_threads_object(const struct spdk_json_val *val, void *out)
{
	struct rpc_core_thread_info *info = out;

	return spdk_json_decode_object(val, rpc_core_thread_info_decoders,
				       SPDK_COUNTOF(rpc_core_thread_info_decoders), info);
}

static int
rpc_decode_cores_lw_threads(const struct spdk_json_val *val, void *out)
{
	struct rpc_core_threads *threads = out;

	return spdk_json_decode_array(val, rpc_decode_core_threads_object, threads->thread, RPC_MAX_THREADS,
				      &threads->threads_count, sizeof(struct rpc_core_thread_info));
}

static const struct spdk_json_object_decoder rpc_core_info_decoders[] = {
	{"lcore", offsetof(struct rpc_core_info, lcore), spdk_json_decode_uint32},
	{"busy", offsetof(struct rpc_core_info, busy), spdk_json_decode_uint64},
	{"idle", offsetof(struct rpc_core_info, idle), spdk_json_decode_uint64},
	{"core_freq", offsetof(struct rpc_core_info, core_freq), spdk_json_decode_uint32, true},
	{"lw_threads", offsetof(struct rpc_core_info, threads), rpc_decode_cores_lw_threads},
};

static int
rpc_decode_core_object(const struct spdk_json_val *val, void *out)
{
	struct rpc_core_info *info = out;

	return spdk_json_decode_object(val, rpc_core_info_decoders,
				       SPDK_COUNTOF(rpc_core_info_decoders), info);
}

static int
rpc_decode_cores_array(const struct spdk_json_val *val, void *out)
{
	struct rpc_cores *cores = out;

	return spdk_json_decode_array(val, rpc_decode_core_object, cores->core,
				      RPC_MAX_THREADS, &cores->cores_count, sizeof(struct rpc_core_info));
}

static const struct spdk_json_object_decoder rpc_cores_stats_decoders[] = {
	{"tick_rate", offsetof(struct rpc_cores_stats, tick_rate), spdk_json_decode_uint64},
	{"reactors", offsetof(struct rpc_cores_stats, cores), rpc_decode_cores_array},
};


static int
rpc_send_req(char *rpc_name, struct spdk_jsonrpc_client_response **resp)
{
	struct spdk_jsonrpc_client_response *json_resp = NULL;
	struct spdk_json_write_ctx *w;
	struct spdk_jsonrpc_client_request *request;
	int rc;

	request = spdk_jsonrpc_client_create_request();
	if (request == NULL) {
		return -ENOMEM;
	}

	w = spdk_jsonrpc_begin_request(request, 1, rpc_name);
	spdk_jsonrpc_end_request(request, w);
	spdk_jsonrpc_client_send_request(g_rpc_client, request);

	do {
		rc = spdk_jsonrpc_client_poll(g_rpc_client, 1);
	} while (rc == 0 || rc == -ENOTCONN);

	if (rc <= 0) {
		return -1;
	}

	json_resp = spdk_jsonrpc_client_get_response(g_rpc_client);
	if (json_resp == NULL) {
		return -1;
	}

	/* Check for error response */
	if (json_resp->error != NULL) {
		return -1;
	}

	assert(json_resp->result);

	*resp = json_resp;

	return 0;
}

static int
get_data(void)
{
	struct spdk_jsonrpc_client_response *json_resp = NULL;
	struct rpc_thread_info *thread_info;
	struct rpc_core_info *core_info;
	uint64_t i, j;
	int rc = 0;

	rc = rpc_send_req("thread_get_stats", &json_resp);
	if (rc) {
		goto end;
	}

	/* Decode json */
	memset(&g_threads_stats, 0, sizeof(g_threads_stats));
	if (spdk_json_decode_object(json_resp->result, rpc_threads_stats_decoders,
				    SPDK_COUNTOF(rpc_threads_stats_decoders), &g_threads_stats)) {
		rc = -EINVAL;
		goto end;
	}

	spdk_jsonrpc_client_free_response(json_resp);

	for (i = 0; i < g_threads_stats.threads.threads_count; i++) {
		thread_info = &g_threads_stats.threads.thread_info[i];
		g_thread_info[thread_info->id] = thread_info;
	}

	rc = rpc_send_req("thread_get_pollers", &json_resp);
	if (rc) {
		goto end;
	}

	/* Decode json */
	memset(&g_pollers_stats, 0, sizeof(g_pollers_stats));
	if (spdk_json_decode_object(json_resp->result, rpc_pollers_stats_decoders,
				    SPDK_COUNTOF(rpc_pollers_stats_decoders), &g_pollers_stats)) {
		rc = -EINVAL;
		goto end;
	}

	spdk_jsonrpc_client_free_response(json_resp);

	rc = rpc_send_req("framework_get_reactors", &json_resp);
	if (rc) {
		goto end;
	}

	/* Decode json */
	memset(&g_cores_stats, 0, sizeof(g_cores_stats));
	if (spdk_json_decode_object(json_resp->result, rpc_cores_stats_decoders,
				    SPDK_COUNTOF(rpc_cores_stats_decoders), &g_cores_stats)) {
		rc = -EINVAL;
		goto end;
	}

	for (i = 0; i < g_cores_stats.cores.cores_count; i++) {
		core_info = &g_cores_stats.cores.core[i];

		for (j = 0; j < core_info->threads.threads_count; j++) {
			g_thread_info[core_info->threads.thread[j].id]->core_num = core_info->lcore;
		}
	}

end:
	spdk_jsonrpc_client_free_response(json_resp);
	return rc;
}

static void
free_data(void)
{
	free_rpc_threads_stats(&g_threads_stats);
	free_rpc_pollers_stats(&g_pollers_stats);
	free_rpc_cores_stats(&g_cores_stats);
}

enum str_alignment {
	ALIGN_LEFT,
	ALIGN_RIGHT,
};

static void
print_max_len(WINDOW *win, int row, uint16_t col, uint16_t max_len, enum str_alignment alignment,
	      const char *string)
{
	const char dots[] = "...";
	int DOTS_STR_LEN = sizeof(dots) / sizeof(dots[0]);
	char tmp_str[MAX_STRING_LEN];
	int len, max_col, max_str, cmp_len;
	int max_row;

	len = strlen(string);
	getmaxyx(win, max_row, max_col);

	if (row > max_row) {
		/* We are in a process of resizing and this may happen */
		return;
	}

	if (max_len != 0 && col + max_len < max_col) {
		max_col = col + max_len;
	}

	max_str = max_col - col;

	if (max_str <= DOTS_STR_LEN + 1) {
		/* No space to print anything, but we have to let a user know about it */
		mvwprintw(win, row, max_col - DOTS_STR_LEN - 1, "...");
		refresh();
		wrefresh(win);
		return;
	}

	if (max_len) {
		if (alignment == ALIGN_LEFT) {
			snprintf(tmp_str, max_str, "%s%*c", string, max_len - len - 1, ' ');
		} else {
			snprintf(tmp_str, max_str, "%*c%s", max_len - len - 1, ' ', string);
		}
		cmp_len = max_len - 1;
	} else {
		snprintf(tmp_str, max_str, "%s", string);
		cmp_len = len;
	}

	if (col + cmp_len > max_col - 1) {
		snprintf(&tmp_str[max_str - DOTS_STR_LEN - 2], DOTS_STR_LEN, "%s", dots);
	}

	mvwprintw(win, row, col, tmp_str);

	refresh();
	wrefresh(win);
}

static void
draw_menu_win(void)
{
	wbkgd(g_menu_win, COLOR_PAIR(2));
	box(g_menu_win, 0, 0);
	print_max_len(g_menu_win, 1, 1, 0, ALIGN_LEFT,
		      "  [q] Quit  |  [1-3] TAB selection  |  [PgUp] Previous page  |  [PgDown] Next page  |  [c] Columns  |  [s] Sorting  |  [r] Refresh rate  |  [Enter] Item details  |  [t] Total/Interval");
}

static void
draw_tab_win(enum tabs tab)
{
	uint16_t col;
	uint8_t white_spaces = TABS_SPACING * NUMBER_OF_TABS;

	wbkgd(g_tab_win[tab], COLOR_PAIR(2));
	box(g_tab_win[tab], 0, 0);

	col = ((g_max_col - white_spaces) / NUMBER_OF_TABS / 2) - (strlen(g_tab_title[tab]) / 2) -
	      TABS_SPACING;
	print_max_len(g_tab_win[tab], 1, col, 0, ALIGN_LEFT, g_tab_title[tab]);
}

static void
draw_tabs(enum tabs tab_index, uint8_t sort_col)
{
	struct col_desc *col_desc = g_col_desc[tab_index];
	WINDOW *tab = g_tabs[tab_index];
	int i, j;
	uint16_t offset, draw_offset;

	for (i = 0; col_desc[i].name != NULL; i++) {
		if (col_desc[i].disabled) {
			continue;
		}

		offset = 1;
		for (j = i; j != 0; j--) {
			if (!col_desc[j - 1].disabled) {
				offset += col_desc[j - 1].max_data_string;
				offset += col_desc[j - 1].name_len % 2 + 1;
			}
		}

		draw_offset = offset + (col_desc[i].max_data_string / 2) - (col_desc[i].name_len / 2);

		if (i == sort_col) {
			wattron(tab, COLOR_PAIR(3));
			print_max_len(tab, 1, draw_offset, 0, ALIGN_LEFT, col_desc[i].name);
			wattroff(tab, COLOR_PAIR(3));
		} else {
			print_max_len(tab, 1, draw_offset, 0, ALIGN_LEFT, col_desc[i].name);
		}

		if (offset != 1) {
			print_max_len(tab, 1, offset - 1, 0, ALIGN_LEFT, "|");
		}
	}

	print_max_len(tab, 2, 1, 0, ALIGN_LEFT, ""); /* Move to next line */
	whline(tab, ACS_HLINE, MAX_STRING_LEN);
	box(tab, 0, 0);
	wrefresh(tab);
}

static void
resize_interface(enum tabs tab)
{
	int i;

	clear();
	wclear(g_menu_win);
	mvwin(g_menu_win, g_max_row - MENU_WIN_SPACING, MENU_WIN_LOCATION_COL);
	wresize(g_menu_win, MENU_WIN_HEIGHT, g_max_col);
	draw_menu_win();

	for (i = 0; i < NUMBER_OF_TABS; i++) {
		wclear(g_tabs[i]);
		wresize(g_tabs[i], g_max_row - MENU_WIN_HEIGHT - TAB_WIN_HEIGHT - 2, g_max_col);
		mvwin(g_tabs[i], TABS_LOCATION_ROW, TABS_LOCATION_COL);
		draw_tabs(i, g_current_sort_col[i]);
	}

	draw_tabs(tab, g_current_sort_col[tab]);

	for (i = 0; i < NUMBER_OF_TABS; i++) {
		wclear(g_tab_win[i]);
		wresize(g_tab_win[i], TAB_WIN_HEIGHT,
			(g_max_col - (TABS_SPACING * NUMBER_OF_TABS)) / NUMBER_OF_TABS);
		mvwin(g_tab_win[i], TAB_WIN_LOCATION_ROW, 1 + (g_max_col / NUMBER_OF_TABS) * i);
		draw_tab_win(i);
	}

	update_panels();
	doupdate();
}

static void
switch_tab(enum tabs tab)
{
	top_panel(g_panels[tab]);
	update_panels();
	doupdate();
}

static void
get_time_str(uint64_t ticks, char *time_str)
{
	uint64_t time;

	time = ticks * SPDK_SEC_TO_USEC / g_cores_stats.tick_rate;
	snprintf(time_str, MAX_TIME_STR_LEN, "%" PRIu64, time);
}

static int
sort_threads(const void *p1, const void *p2)
{
	const struct rpc_thread_info *thread_info1 = *(struct rpc_thread_info **)p1;
	const struct rpc_thread_info *thread_info2 = *(struct rpc_thread_info **)p2;
	uint64_t count1, count2;

	/* thread IDs may not be allocated contiguously, so we need
	 * to account for NULL thread_info pointers */
	if (thread_info1 == NULL && thread_info2 == NULL) {
		return 0;
	} else if (thread_info1 == NULL) {
		return 1;
	} else if (thread_info2 == NULL) {
		return -1;
	}

	switch (g_current_sort_col[THREADS_TAB]) {
	case 0: /* Sort by name */
		return strcmp(thread_info1->name, thread_info2->name);
	case 1: /* Sort by core */
		count2 = thread_info1->core_num;
		count1 = thread_info2->core_num;
		break;
	case 2: /* Sort by active pollers number */
		count1 = thread_info1->active_pollers_count;
		count2 = thread_info2->active_pollers_count;
		break;
	case 3: /* Sort by timed pollers number */
		count1 = thread_info1->timed_pollers_count;
		count2 = thread_info2->timed_pollers_count;
		break;
	case 4: /* Sort by paused pollers number */
		count1 = thread_info1->paused_pollers_count;
		count2 = thread_info2->paused_pollers_count;
		break;
	case 5: /* Sort by idle time */
		count1 = thread_info1->idle - thread_info1->last_idle;
		count2 = thread_info2->idle - thread_info2->last_idle;
		break;
	case 6: /* Sort by busy time */
		count1 = thread_info1->busy - thread_info1->last_busy;
		count2 = thread_info2->busy - thread_info2->last_busy;
		break;
	default:
		return 0;
	}

	if (count2 > count1) {
		return 1;
	} else if (count2 < count1) {
		return -1;
	} else {
		return 0;
	}
}

static void
draw_row_background(uint8_t item_index, uint8_t tab)
{
	int k;

	if (item_index == g_selected_row) {
		wattron(g_tabs[tab], COLOR_PAIR(2));
	}
	for (k = 1; k < g_max_col - 1; k++) {
		mvwprintw(g_tabs[tab], TABS_DATA_START_ROW + item_index, k, " ");
	}
}

static uint8_t
refresh_threads_tab(uint8_t current_page)
{
	struct col_desc *col_desc = g_col_desc[THREADS_TAB];
	uint64_t i, threads_count;
	uint16_t j, k;
	uint16_t col;
	uint8_t max_pages, item_index;
	static uint8_t last_page = 0;
	char pollers_number[MAX_POLLER_COUNT_STR_LEN], idle_time[MAX_TIME_STR_LEN],
	     busy_time[MAX_TIME_STR_LEN], core_str[MAX_CORE_MASK_STR_LEN];
	struct rpc_thread_info *thread_info[g_threads_stats.threads.threads_count];

	threads_count = g_threads_stats.threads.threads_count;

	/* Clear screen if number of threads changed */
	if (g_last_threads_count != threads_count) {
		for (i = TABS_DATA_START_ROW; i < g_data_win_size; i++) {
			for (j = 1; j < (uint64_t)g_max_col - 1; j++) {
				mvwprintw(g_tabs[THREADS_TAB], i, j, " ");
			}
		}

		g_last_threads_count = threads_count;
	}

	/* From g_thread_info copy to thread_info without null elements.
	 * The index of g_thread_info equals to Thread IDs, so it starts from '1'. */
	for (i = 0, j = 1; i <  g_threads_stats.threads.threads_count; i++) {
		while (g_thread_info[j] == NULL) {
			j++;
		}
		memcpy(&thread_info[i], &g_thread_info[j], sizeof(struct rpc_thread_info *));
		j++;
	}

	if (last_page != current_page) {
		for (i = 0; i < threads_count; i++) {
			/* Thread IDs start from 1, so we have to do i + 1 */
			g_threads_stats.threads.thread_info[i].last_idle = g_thread_info[i + 1]->idle;
			g_threads_stats.threads.thread_info[i].last_busy = g_thread_info[i + 1]->busy;
		}

		last_page = current_page;
	}

	max_pages = (threads_count + g_max_data_rows - 1) / g_max_data_rows;

	qsort(thread_info, threads_count, sizeof(thread_info[0]), sort_threads);

	for (k = 0; k < threads_count; k++) {
		g_thread_history[thread_info[k]->id].busy = thread_info[k]->busy - thread_info[k]->last_busy;
		g_thread_history[thread_info[k]->id].idle = thread_info[k]->idle - thread_info[k]->last_idle;
	}

	for (i = current_page * g_max_data_rows;
	     i < spdk_min(threads_count, (uint64_t)((current_page + 1) * g_max_data_rows));
	     i++) {
		item_index = i - (current_page * g_max_data_rows);

		col = TABS_DATA_START_COL;

		draw_row_background(item_index, THREADS_TAB);

		if (!col_desc[0].disabled) {
			print_max_len(g_tabs[THREADS_TAB], TABS_DATA_START_ROW + item_index, col,
				      col_desc[0].max_data_string, ALIGN_LEFT, thread_info[i]->name);
			col += col_desc[0].max_data_string;
		}

		if (!col_desc[1].disabled) {
			snprintf(core_str, MAX_CORE_STR_LEN, "%d", thread_info[i]->core_num);
			print_max_len(g_tabs[THREADS_TAB], TABS_DATA_START_ROW + item_index,
				      col, col_desc[1].max_data_string, ALIGN_RIGHT, core_str);
			col += col_desc[1].max_data_string + 2;
		}

		if (!col_desc[2].disabled) {
			snprintf(pollers_number, MAX_POLLER_COUNT_STR_LEN, "%ld", thread_info[i]->active_pollers_count);
			print_max_len(g_tabs[THREADS_TAB], TABS_DATA_START_ROW + item_index,
				      col + (col_desc[2].name_len / 2), col_desc[2].max_data_string, ALIGN_LEFT, pollers_number);
			col += col_desc[2].max_data_string + 2;
		}

		if (!col_desc[3].disabled) {
			snprintf(pollers_number, MAX_POLLER_COUNT_STR_LEN, "%ld", thread_info[i]->timed_pollers_count);
			print_max_len(g_tabs[THREADS_TAB], TABS_DATA_START_ROW + item_index,
				      col + (col_desc[3].name_len / 2), col_desc[3].max_data_string, ALIGN_LEFT, pollers_number);
			col += col_desc[3].max_data_string + 1;
		}

		if (!col_desc[4].disabled) {
			snprintf(pollers_number, MAX_POLLER_COUNT_STR_LEN, "%ld", thread_info[i]->paused_pollers_count);
			print_max_len(g_tabs[THREADS_TAB], TABS_DATA_START_ROW + item_index,
				      col + (col_desc[4].name_len / 2), col_desc[4].max_data_string, ALIGN_LEFT, pollers_number);
			col += col_desc[4].max_data_string + 2;
		}

		g_thread_history[thread_info[i]->id].idle = thread_info[i]->idle - thread_info[i]->last_idle;
		if (!col_desc[5].disabled) {
			if (g_interval_data == true) {
				get_time_str(thread_info[i]->idle - thread_info[i]->last_idle, idle_time);
			} else {
				get_time_str(thread_info[i]->idle, idle_time);
			}
			print_max_len(g_tabs[THREADS_TAB], TABS_DATA_START_ROW + item_index, col,
				      col_desc[5].max_data_string, ALIGN_RIGHT, idle_time);
			col += col_desc[5].max_data_string;
		}

		g_thread_history[thread_info[i]->id].busy = thread_info[i]->busy - thread_info[i]->last_busy;
		if (!col_desc[6].disabled) {
			if (g_interval_data == true) {
				get_time_str(thread_info[i]->busy - thread_info[i]->last_busy, busy_time);
			} else {
				get_time_str(thread_info[i]->busy, busy_time);
			}
			print_max_len(g_tabs[THREADS_TAB], TABS_DATA_START_ROW + item_index, col,
				      col_desc[6].max_data_string, ALIGN_RIGHT, busy_time);
		}

		if (item_index == g_selected_row) {
			wattroff(g_tabs[THREADS_TAB], COLOR_PAIR(2));
		}
	}

	for (k = 0; k < threads_count; k++) {
		thread_info[k]->last_idle = thread_info[k]->idle;
		thread_info[k]->last_busy = thread_info[k]->busy;
	}

	g_max_selected_row = i - current_page * g_max_data_rows - 1;

	return max_pages;
}

static uint64_t *
get_last_run_counter(const char *poller_name, uint64_t thread_id)
{
	struct run_counter_history *history;

	TAILQ_FOREACH(history, &g_run_counter_history, link) {
		if (!strcmp(history->poller_name, poller_name) && history->thread_id == thread_id) {
			return &history->last_run_counter;
		}
	}

	return NULL;
}

static void
store_last_run_counter(const char *poller_name, uint64_t thread_id, uint64_t last_run_counter)
{
	struct run_counter_history *history;

	TAILQ_FOREACH(history, &g_run_counter_history, link) {
		if (!strcmp(history->poller_name, poller_name) && history->thread_id == thread_id) {
			history->last_run_counter = last_run_counter;
			return;
		}
	}

	history = calloc(1, sizeof(*history));
	if (history == NULL) {
		fprintf(stderr, "Unable to allocate a history object in store_last_run_counter.\n");
		return;
	}
	history->poller_name = strdup(poller_name);
	history->thread_id = thread_id;
	history->last_run_counter = last_run_counter;

	TAILQ_INSERT_TAIL(&g_run_counter_history, history, link);
}

enum sort_type {
	BY_NAME,
	USE_GLOBAL,
};

static int
#ifdef __FreeBSD__
sort_pollers(void *arg, const void *p1, const void *p2)
#else
sort_pollers(const void *p1, const void *p2, void *arg)
#endif
{
	const struct rpc_poller_info *poller1 = *(struct rpc_poller_info **)p1;
	const struct rpc_poller_info *poller2 = *(struct rpc_poller_info **)p2;
	enum sort_type sorting = *(enum sort_type *)arg;
	uint64_t count1, count2;
	uint64_t *last_run_counter;

	if (sorting == BY_NAME) {
		/* Sorting by name requested explicitly */
		return strcmp(poller1->name, poller2->name);
	} else {
		/* Use globaly set sorting */
		switch (g_current_sort_col[POLLERS_TAB]) {
		case 0: /* Sort by name */
			return strcmp(poller1->name, poller2->name);
		case 1: /* Sort by type */
			return poller1->type - poller2->type;
		case 2: /* Sort by thread */
			return strcmp(poller1->thread_name, poller2->thread_name);
		case 3: /* Sort by run counter */
			last_run_counter = get_last_run_counter(poller1->name, poller1->thread_id);
			assert(last_run_counter != NULL);
			count1 = poller1->run_count - *last_run_counter;
			last_run_counter = get_last_run_counter(poller2->name, poller2->thread_id);
			assert(last_run_counter != NULL);
			count2 = poller2->run_count - *last_run_counter;
			break;
		case 4: /* Sort by period */
			count1 = poller1->period_ticks;
			count2 = poller2->period_ticks;
			break;
		default:
			return 0;
		}
	}

	if (count2 > count1) {
		return 1;
	} else if (count2 < count1) {
		return -1;
	} else {
		return 0;
	}
}

static void
copy_pollers(struct rpc_pollers *pollers, uint64_t pollers_count, enum spdk_poller_type type,
	     struct rpc_poller_thread_info *thread, uint64_t *current_count, bool reset_last_counter,
	     struct rpc_poller_info **pollers_info)
{
	uint64_t *last_run_counter;
	uint64_t i;

	for (i = 0; i < pollers_count; i++) {
		if (reset_last_counter) {
			last_run_counter = get_last_run_counter(pollers->pollers[i].name, thread->id);
			if (last_run_counter == NULL) {
				store_last_run_counter(pollers->pollers[i].name, thread->id, pollers->pollers[i].run_count);
				last_run_counter = get_last_run_counter(pollers->pollers[i].name, thread->id);
			}

			assert(last_run_counter != NULL);
			*last_run_counter = pollers->pollers[i].run_count;
		}
		pollers_info[*current_count] = &pollers->pollers[i];
		snprintf(pollers_info[*current_count]->thread_name, MAX_POLLER_NAME - 1, "%s", thread->name);
		pollers_info[*current_count]->thread_id = thread->id;
		pollers_info[(*current_count)++]->type = type;
	}
}

static void
store_pollers_last_stats(uint64_t poller, uint64_t run_counter, uint64_t period_ticks_counter)
{
	g_pollers_history[poller].run_count = run_counter;
	g_pollers_history[poller].period_ticks = period_ticks_counter;
}

static uint8_t
prepare_poller_data(uint8_t current_page, struct rpc_poller_info **pollers,
		    uint64_t *count, uint8_t last_page)
{
	struct rpc_poller_thread_info *thread;
	uint64_t i;
	bool reset_last_counter = false;
	enum sort_type sorting;

	for (i = 0; i < g_pollers_stats.pollers_threads.threads_count; i++) {
		thread = &g_pollers_stats.pollers_threads.threads[i];
		if (last_page != current_page) {
			reset_last_counter = true;
		}

		copy_pollers(&thread->active_pollers, thread->active_pollers.pollers_count, SPDK_ACTIVE_POLLER,
			     thread, count, reset_last_counter, pollers);
		copy_pollers(&thread->timed_pollers, thread->timed_pollers.pollers_count, SPDK_TIMED_POLLER, thread,
			     count, reset_last_counter, pollers);
		copy_pollers(&thread->paused_pollers, thread->paused_pollers.pollers_count, SPDK_PAUSED_POLLER,
			     thread, count, reset_last_counter, pollers);
	}

	if (last_page != current_page) {
		last_page = current_page;
	}

	/* Timed pollers can switch their possition on a list because of how they work.
	 * Let's sort them by name first so that they won't switch on data refresh */
	sorting = BY_NAME;
	qsort_r(pollers, *count, sizeof(pollers[0]), sort_pollers, (void *)&sorting);
	sorting = USE_GLOBAL;
	qsort_r(pollers, *count, sizeof(pollers[0]), sort_pollers, (void *)&sorting);

	return last_page;
}

static uint8_t
refresh_pollers_tab(uint8_t current_page)
{
	struct col_desc *col_desc = g_col_desc[POLLERS_TAB];
	uint64_t *last_run_counter;
	uint64_t i, count = 0;
	uint16_t col, j;
	uint8_t max_pages, item_index;
	static uint8_t g_last_page = 0xF;
	/* Init g_last_page with value != 0 to force store_last_run_counter() call in copy_pollers()
	 * so that initial values for run_counter are stored in g_run_counter_history */
	char run_count[MAX_TIME_STR_LEN], period_ticks[MAX_PERIOD_STR_LEN];
	struct rpc_poller_info *pollers[RPC_MAX_POLLERS];

	g_last_page = prepare_poller_data(current_page, pollers, &count, g_last_page);

	max_pages = (count + g_max_data_rows - 1) / g_max_data_rows;

	/* Clear screen if number of pollers changed */
	if (g_last_pollers_count != count) {
		for (i = TABS_DATA_START_ROW; i < g_data_win_size; i++) {
			for (j = 1; j < (uint64_t)g_max_col - 1; j++) {
				mvwprintw(g_tabs[POLLERS_TAB], i, j, " ");
			}
		}

		g_last_pollers_count = count;

		/* We need to run store_last_run_counter() again, so the easiest way is to call this function
		 * again with changed g_last_page value */
		g_last_page = 0xF;
		refresh_pollers_tab(current_page);
		return max_pages;
	}

	/* Display info */
	for (i = current_page * g_max_data_rows;
	     i < spdk_min(count, (uint64_t)((current_page + 1) * g_max_data_rows));
	     i++) {
		item_index = i - (current_page * g_max_data_rows);

		col = TABS_DATA_START_COL;

		draw_row_background(item_index, POLLERS_TAB);

		last_run_counter = get_last_run_counter(pollers[i]->name, pollers[i]->thread_id);
		assert(last_run_counter != NULL);

		if (!col_desc[0].disabled) {
			print_max_len(g_tabs[POLLERS_TAB], TABS_DATA_START_ROW + item_index, col + 1,
				      col_desc[0].max_data_string, ALIGN_LEFT, pollers[i]->name);
			col += col_desc[0].max_data_string + 2;
		}

		if (!col_desc[1].disabled) {
			print_max_len(g_tabs[POLLERS_TAB], TABS_DATA_START_ROW + item_index, col,
				      col_desc[1].max_data_string, ALIGN_LEFT, poller_type_str[pollers[i]->type]);
			col += col_desc[1].max_data_string + 2;
		}

		if (!col_desc[2].disabled) {
			print_max_len(g_tabs[POLLERS_TAB], TABS_DATA_START_ROW + item_index, col,
				      col_desc[2].max_data_string, ALIGN_LEFT, pollers[i]->thread_name);
			col += col_desc[2].max_data_string + 1;
		}

		store_pollers_last_stats(i, pollers[i]->run_count - *last_run_counter, pollers[i]->period_ticks);
		if (!col_desc[3].disabled) {
			if (g_interval_data == true) {
				snprintf(run_count, MAX_TIME_STR_LEN, "%" PRIu64, pollers[i]->run_count - *last_run_counter);
			} else {
				snprintf(run_count, MAX_TIME_STR_LEN, "%" PRIu64, pollers[i]->run_count);
			}
			print_max_len(g_tabs[POLLERS_TAB], TABS_DATA_START_ROW + item_index, col,
				      col_desc[3].max_data_string, ALIGN_RIGHT, run_count);
			col += col_desc[3].max_data_string;
		}

		if (!col_desc[4].disabled) {
			if (pollers[i]->period_ticks != 0) {
				get_time_str(pollers[i]->period_ticks, period_ticks);
				print_max_len(g_tabs[POLLERS_TAB], TABS_DATA_START_ROW + item_index, col,
					      col_desc[4].max_data_string, ALIGN_RIGHT, period_ticks);
			}
			col += col_desc[3].max_data_string + 4;
		}

		store_last_run_counter(pollers[i]->name, pollers[i]->thread_id, pollers[i]->run_count);

		if (!col_desc[5].disabled) {
			if (pollers[i]->busy_count > 0) {
				if (item_index != g_selected_row) {
					wattron(g_tabs[POLLERS_TAB], COLOR_PAIR(6));
					print_max_len(g_tabs[POLLERS_TAB], TABS_DATA_START_ROW + item_index, col,
						      col_desc[5].max_data_string, ALIGN_RIGHT, "Busy");
					wattroff(g_tabs[POLLERS_TAB], COLOR_PAIR(6));
				} else {
					wattron(g_tabs[POLLERS_TAB], COLOR_PAIR(8));
					print_max_len(g_tabs[POLLERS_TAB], TABS_DATA_START_ROW + item_index, col,
						      col_desc[5].max_data_string, ALIGN_RIGHT, "Busy");
					wattroff(g_tabs[POLLERS_TAB], COLOR_PAIR(8));
				}
			} else {
				if (item_index != g_selected_row) {
					wattron(g_tabs[POLLERS_TAB], COLOR_PAIR(7));
					print_max_len(g_tabs[POLLERS_TAB], TABS_DATA_START_ROW + item_index, col,
						      col_desc[5].max_data_string, ALIGN_RIGHT, "Idle");
					wattroff(g_tabs[POLLERS_TAB], COLOR_PAIR(7));
				} else {
					wattron(g_tabs[POLLERS_TAB], COLOR_PAIR(9));
					print_max_len(g_tabs[POLLERS_TAB], TABS_DATA_START_ROW + item_index, col,
						      col_desc[5].max_data_string, ALIGN_RIGHT, "Idle");
					wattroff(g_tabs[POLLERS_TAB], COLOR_PAIR(9));
				}
			}
		}

		if (item_index == g_selected_row) {
			wattroff(g_tabs[POLLERS_TAB], COLOR_PAIR(2));
		}
	}

	g_max_selected_row = i - current_page * g_max_data_rows - 1;

	return max_pages;
}

static int
sort_cores(const void *p1, const void *p2)
{
	const struct core_info core_info1 = *(struct core_info *)p1;
	const struct core_info core_info2 = *(struct core_info *)p2;
	uint64_t count1, count2;

	switch (g_current_sort_col[CORES_TAB]) {
	case 0: /* Sort by core */
		count1 = core_info2.core;
		count2 = core_info1.core;
		break;
	case 1: /* Sort by threads number */
		count1 = core_info1.threads_count;
		count2 = core_info2.threads_count;
		break;
	case 2: /* Sort by pollers number */
		count1 = core_info1.pollers_count;
		count2 = core_info2.pollers_count;
		break;
	case 3: /* Sort by idle time */
		count2 = g_cores_history[core_info1.core].last_idle - core_info1.idle;
		count1 = g_cores_history[core_info2.core].last_idle - core_info2.idle;
		break;
	case 4: /* Sort by busy time */
		count2 = g_cores_history[core_info1.core].last_busy - core_info1.busy;
		count1 = g_cores_history[core_info2.core].last_busy - core_info2.busy;
		break;
	default:
		return 0;
	}

	if (count2 > count1) {
		return 1;
	} else if (count2 < count1) {
		return -1;
	} else {
		return 0;
	}
}

static void
store_core_last_stats(uint32_t core, uint64_t idle, uint64_t busy)
{
	g_cores_history[core].last_idle = idle;
	g_cores_history[core].last_busy = busy;
}

static void
get_core_last_stats(uint32_t core, uint64_t *idle, uint64_t *busy)
{
	*idle = g_cores_history[core].last_idle;
	*busy = g_cores_history[core].last_busy;
}

static void
store_core_stats(uint32_t core, uint64_t threads, uint64_t pollers, uint64_t idle, uint64_t busy)
{
	g_cores_history[core].threads_count = threads;
	g_cores_history[core].pollers_count = pollers;
	g_cores_history[core].idle = idle;
	g_cores_history[core].busy = busy;
}

static uint8_t
refresh_cores_tab(uint8_t current_page)
{
	struct col_desc *col_desc = g_col_desc[CORES_TAB];
	uint64_t i;
	uint32_t core_num;
	uint16_t offset, count = 0;
	uint8_t max_pages, item_index;
	static uint8_t last_page = 0;
	char core[MAX_CORE_STR_LEN], threads_number[MAX_THREAD_COUNT_STR_LEN],
	     pollers_number[MAX_POLLER_COUNT_STR_LEN], idle_time[MAX_TIME_STR_LEN],
	     busy_time[MAX_TIME_STR_LEN], core_freq[MAX_CORE_FREQ_STR_LEN];
	struct core_info cores[RPC_MAX_CORES];

	memset(&cores, 0, sizeof(cores));

	for (i = 0; i < g_threads_stats.threads.threads_count; i++) {
		core_num = g_threads_stats.threads.thread_info[i].core_num;
		cores[core_num].threads_count++;
		cores[core_num].pollers_count += g_threads_stats.threads.thread_info[i].active_pollers_count +
						 g_threads_stats.threads.thread_info[i].timed_pollers_count +
						 g_threads_stats.threads.thread_info[i].paused_pollers_count;
	}

	count = g_cores_stats.cores.cores_count;

	for (i = 0; i < count; i++) {
		core_num = g_cores_stats.cores.core[i].lcore;
		cores[core_num].core = core_num;
		cores[core_num].busy = g_cores_stats.cores.core[i].busy;
		cores[core_num].idle = g_cores_stats.cores.core[i].idle;
		if (last_page != current_page) {
			store_core_last_stats(cores[core_num].core, cores[core_num].idle, cores[core_num].busy);
		}
	}

	if (last_page != current_page) {
		last_page = current_page;
	}

	max_pages = (count + g_max_row - WINDOW_HEADER - 1) / (g_max_row - WINDOW_HEADER);

	qsort(&cores, count, sizeof(cores[0]), sort_cores);

	for (i = current_page * g_max_data_rows;
	     i < spdk_min(count, (uint64_t)((current_page + 1) * g_max_data_rows));
	     i++) {
		item_index = i - (current_page * g_max_data_rows);

		core_num = g_cores_stats.cores.core[i].lcore;

		snprintf(threads_number, MAX_THREAD_COUNT_STR_LEN, "%ld", cores[core_num].threads_count);
		snprintf(pollers_number, MAX_POLLER_COUNT_STR_LEN, "%ld", cores[core_num].pollers_count);
		get_core_last_stats(cores[core_num].core, &cores[core_num].last_idle, &cores[core_num].last_busy);

		offset = 1;

		draw_row_background(item_index, CORES_TAB);

		if (!col_desc[0].disabled) {
			snprintf(core, MAX_CORE_STR_LEN, "%d", cores[core_num].core);
			print_max_len(g_tabs[CORES_TAB], TABS_DATA_START_ROW + item_index, offset,
				      col_desc[0].max_data_string, ALIGN_RIGHT, core);
			offset += col_desc[0].max_data_string + 2;
		}

		if (!col_desc[1].disabled) {
			print_max_len(g_tabs[CORES_TAB], TABS_DATA_START_ROW + item_index,
				      offset + (col_desc[1].name_len / 2), col_desc[1].max_data_string, ALIGN_LEFT, threads_number);
			offset += col_desc[1].max_data_string + 2;
		}

		if (!col_desc[2].disabled) {
			print_max_len(g_tabs[CORES_TAB], TABS_DATA_START_ROW + item_index,
				      offset + (col_desc[2].name_len / 2), col_desc[2].max_data_string, ALIGN_LEFT, pollers_number);
			offset += col_desc[2].max_data_string;
		}

		if (!col_desc[3].disabled) {
			if (g_interval_data == true) {
				get_time_str(cores[core_num].idle - cores[core_num].last_idle, idle_time);
			} else {
				get_time_str(cores[core_num].idle, idle_time);
			}
			print_max_len(g_tabs[CORES_TAB], TABS_DATA_START_ROW + item_index, offset,
				      col_desc[3].max_data_string, ALIGN_RIGHT, idle_time);
			offset += col_desc[3].max_data_string + 2;
		}

		if (!col_desc[4].disabled) {
			if (g_interval_data == true) {
				get_time_str(cores[core_num].busy - cores[core_num].last_busy, busy_time);
			} else {
				get_time_str(cores[core_num].busy, busy_time);
			}
			print_max_len(g_tabs[CORES_TAB], TABS_DATA_START_ROW + item_index, offset,
				      col_desc[4].max_data_string, ALIGN_RIGHT, busy_time);
			offset += col_desc[4].max_data_string + 2;
		}

		if (!col_desc[5].disabled) {
			if (!g_cores_stats.cores.core[core_num].core_freq) {
				snprintf(core_freq,  MAX_CORE_FREQ_STR_LEN, "%s", "N/A");
			} else {
				snprintf(core_freq, MAX_CORE_FREQ_STR_LEN, "%" PRIu32,
					 g_cores_stats.cores.core[core_num].core_freq);
			}
			print_max_len(g_tabs[CORES_TAB], TABS_DATA_START_ROW + item_index, offset,
				      col_desc[5].max_data_string, ALIGN_RIGHT, core_freq);
		}

		store_core_last_stats(cores[core_num].core, cores[core_num].idle, cores[core_num].busy);
		store_core_stats(cores[core_num].core, cores[core_num].threads_count, cores[core_num].pollers_count,
				 cores[core_num].idle - cores[core_num].last_idle, cores[core_num].busy - cores[core_num].last_busy);

		if (item_index == g_selected_row) {
			wattroff(g_tabs[CORES_TAB], COLOR_PAIR(2));
		}
	}

	g_max_selected_row = i - current_page * g_max_data_rows - 1;

	return max_pages;
}

static uint8_t
refresh_tab(enum tabs tab, uint8_t current_page)
{
	uint8_t (*refresh_function[NUMBER_OF_TABS])(uint8_t current_page) = {refresh_threads_tab, refresh_pollers_tab, refresh_cores_tab};
	int color_pair[NUMBER_OF_TABS] = {COLOR_PAIR(2), COLOR_PAIR(2), COLOR_PAIR(2)};
	int i;
	uint8_t max_pages = 0;

	color_pair[tab] = COLOR_PAIR(1);

	for (i = 0; i < NUMBER_OF_TABS; i++) {
		wbkgd(g_tab_win[i], color_pair[i]);
	}

	max_pages = (*refresh_function[tab])(current_page);
	refresh();

	for (i = 0; i < NUMBER_OF_TABS; i++) {
		wrefresh(g_tab_win[i]);
	}
	draw_menu_win();

	return max_pages;
}

static void
print_in_middle(WINDOW *win, int starty, int startx, int width, char *string, chtype color)
{
	int length, temp;

	length = strlen(string);
	temp = (width - length) / 2;
	wattron(win, color);
	mvwprintw(win, starty, startx + temp, "%s", string);
	wattroff(win, color);
	refresh();
}

static void
print_left(WINDOW *win, int starty, int startx, int width, char *string, chtype color)
{
	wattron(win, color);
	mvwprintw(win, starty, startx, "%s", string);
	wattroff(win, color);
	refresh();
}

static void
apply_filters(enum tabs tab)
{
	wclear(g_tabs[tab]);
	draw_tabs(tab, g_current_sort_col[tab]);
}

static ITEM **
draw_filtering_menu(uint8_t position, WINDOW *filter_win, uint8_t tab, MENU **my_menu)
{
	const int ADDITIONAL_ELEMENTS = 3;
	const int ROW_PADDING = 6;
	const int WINDOW_START_X = 1;
	const int WINDOW_START_Y = 3;
	const int WINDOW_COLUMNS = 2;
	struct col_desc *col_desc = g_col_desc[tab];
	ITEM **my_items;
	MENU *menu;
	int i, elements;
	uint8_t len = 0;

	for (i = 0; col_desc[i].name != NULL; ++i) {
		len = spdk_max(col_desc[i].name_len, len);
	}

	elements = i;

	my_items = (ITEM **)calloc(elements * WINDOW_COLUMNS + ADDITIONAL_ELEMENTS, sizeof(ITEM *));
	if (my_items == NULL) {
		fprintf(stderr, "Unable to allocate an item list in draw_filtering_menu.\n");
		return NULL;
	}

	for (i = 0; i < elements * 2; i++) {
		my_items[i] = new_item(col_desc[i / WINDOW_COLUMNS].name, NULL);
		i++;
		my_items[i] = new_item(col_desc[i / WINDOW_COLUMNS].disabled ? "[ ]" : "[*]", NULL);
	}

	my_items[i] = new_item("     CLOSE", NULL);
	set_item_userptr(my_items[i], apply_filters);

	menu = new_menu((ITEM **)my_items);

	menu_opts_off(menu, O_SHOWDESC);
	set_menu_format(menu, elements + 1, WINDOW_COLUMNS);

	set_menu_win(menu, filter_win);
	set_menu_sub(menu, derwin(filter_win, elements + 1, len + ROW_PADDING, WINDOW_START_Y,
				  WINDOW_START_X));

	*my_menu = menu;

	post_menu(menu);
	refresh();
	wrefresh(filter_win);

	for (i = 0; i < position / WINDOW_COLUMNS; i++) {
		menu_driver(menu, REQ_DOWN_ITEM);
	}

	return my_items;
}

static void
delete_filtering_menu(MENU *my_menu, ITEM **my_items, uint8_t elements)
{
	int i;

	unpost_menu(my_menu);
	free_menu(my_menu);
	for (i = 0; i < elements * 2 + 2; ++i) {
		free_item(my_items[i]);
	}
	free(my_items);
}

static ITEM **
refresh_filtering_menu(MENU **my_menu, WINDOW *filter_win, uint8_t tab, ITEM **my_items,
		       uint8_t elements, uint8_t position)
{
	delete_filtering_menu(*my_menu, my_items, elements);
	return draw_filtering_menu(position, filter_win, tab, my_menu);
}

static void
filter_columns(uint8_t tab)
{
	const int WINDOW_HEADER_LEN = 5;
	const int WINDOW_BORDER_LEN = 8;
	const int WINDOW_HEADER_END_LINE = 2;
	const int WINDOW_COLUMNS = 2;
	struct col_desc *col_desc = g_col_desc[tab];
	PANEL *filter_panel;
	WINDOW *filter_win;
	ITEM **my_items;
	MENU *my_menu = NULL;
	int i, c, elements;
	bool stop_loop = false;
	ITEM *cur;
	void (*p)(enum tabs tab);
	uint8_t current_index, len = 0;
	bool disabled[TABS_COL_COUNT];

	for (i = 0; col_desc[i].name != NULL; ++i) {
		len = spdk_max(col_desc[i].name_len, len);
	}

	elements = i;

	filter_win = newwin(elements + WINDOW_HEADER_LEN, len + WINDOW_BORDER_LEN,
			    (g_max_row - elements - 1) / 2, (g_max_col - len) / 2);
	assert(filter_win != NULL);
	keypad(filter_win, TRUE);
	filter_panel = new_panel(filter_win);
	assert(filter_panel != NULL);

	top_panel(filter_panel);
	update_panels();
	doupdate();

	box(filter_win, 0, 0);

	print_in_middle(filter_win, 1, 0, len + WINDOW_BORDER_LEN, "Filtering", COLOR_PAIR(3));
	mvwaddch(filter_win, WINDOW_HEADER_END_LINE, 0, ACS_LTEE);
	mvwhline(filter_win, WINDOW_HEADER_END_LINE, 1, ACS_HLINE, len + WINDOW_BORDER_LEN - 2);
	mvwaddch(filter_win, WINDOW_HEADER_END_LINE, len + WINDOW_BORDER_LEN - 1, ACS_RTEE);

	my_items = draw_filtering_menu(0, filter_win, tab, &my_menu);
	if (my_items == NULL || my_menu == NULL) {
		goto fail;
	}

	for (int i = 0; i < TABS_COL_COUNT; i++) {
		disabled[i] = col_desc[i].disabled;
	}

	while (!stop_loop) {
		c = wgetch(filter_win);

		switch (c) {
		case KEY_DOWN:
			menu_driver(my_menu, REQ_DOWN_ITEM);
			break;
		case KEY_UP:
			menu_driver(my_menu, REQ_UP_ITEM);
			break;
		case 27: /* ESC */
		case 'q':
			for (int i = 0; i < TABS_COL_COUNT; i++) {
				cur = current_item(my_menu);
				col_desc[i].disabled = disabled[i];

				my_items = refresh_filtering_menu(&my_menu, filter_win, tab, my_items, elements,
								  item_index(cur) + 1);
				if (my_items == NULL || my_menu == NULL) {
					goto fail;
				}
			}

			stop_loop = true;
			break;
		case ' ': /* Space */
			cur = current_item(my_menu);
			current_index = item_index(cur) / WINDOW_COLUMNS;
			col_desc[current_index].disabled = !col_desc[current_index].disabled;
			my_items = refresh_filtering_menu(&my_menu, filter_win, tab, my_items, elements,
							  item_index(cur) + 1);
			if (my_items == NULL || my_menu == NULL) {
				goto fail;
			}
			break;
		case 10: /* Enter */
			cur = current_item(my_menu);
			current_index = item_index(cur) / WINDOW_COLUMNS;
			if (current_index == elements) {
				stop_loop = true;
				p = item_userptr(cur);
				p(tab);
			} else {
				col_desc[current_index].disabled = !col_desc[current_index].disabled;
				my_items = refresh_filtering_menu(&my_menu, filter_win, tab, my_items, elements,
								  item_index(cur) + 1);
				if (my_items == NULL || my_menu == NULL) {
					goto fail;
				}
			}
			break;
		}
		wrefresh(filter_win);
	}

	delete_filtering_menu(my_menu, my_items, elements);

	del_panel(filter_panel);
	delwin(filter_win);

	wclear(g_menu_win);
	draw_menu_win();
	return;

fail:
	fprintf(stderr, "Unable to filter the columns due to allocation failure.\n");
	assert(false);
}

static void
sort_type(enum tabs tab, int item_index)
{
	g_current_sort_col[tab] = item_index;
	wclear(g_tabs[tab]);
	draw_tabs(tab, g_current_sort_col[tab]);
}

static void
change_sorting(uint8_t tab)
{
	const int WINDOW_HEADER_LEN = 4;
	const int WINDOW_BORDER_LEN = 3;
	const int WINDOW_START_X = 1;
	const int WINDOW_START_Y = 3;
	const int WINDOW_HEADER_END_LINE = 2;
	PANEL *sort_panel;
	WINDOW *sort_win;
	ITEM **my_items;
	MENU *my_menu;
	int i, c, elements;
	bool stop_loop = false;
	ITEM *cur;
	void (*p)(enum tabs tab, int item_index);
	uint8_t len = 0;

	for (i = 0; g_col_desc[tab][i].name != NULL; ++i) {
		len = spdk_max(len, g_col_desc[tab][i].name_len);
	}

	elements = i;

	my_items = (ITEM **)calloc(elements + 1, sizeof(ITEM *));
	if (my_items == NULL) {
		fprintf(stderr, "Unable to allocate an item list in change_sorting.\n");
		return;
	}

	for (i = 0; i < elements; ++i) {
		my_items[i] = new_item(g_col_desc[tab][i].name, NULL);
		set_item_userptr(my_items[i], sort_type);
	}

	my_menu = new_menu((ITEM **)my_items);

	menu_opts_off(my_menu, O_SHOWDESC);

	sort_win = newwin(elements + WINDOW_HEADER_LEN, len + WINDOW_BORDER_LEN, (g_max_row - elements) / 2,
			  (g_max_col - len) / 2);
	assert(sort_win != NULL);
	keypad(sort_win, TRUE);
	sort_panel = new_panel(sort_win);
	assert(sort_panel != NULL);

	top_panel(sort_panel);
	update_panels();
	doupdate();

	set_menu_win(my_menu, sort_win);
	set_menu_sub(my_menu, derwin(sort_win, elements, len + 1, WINDOW_START_Y, WINDOW_START_X));
	box(sort_win, 0, 0);

	print_in_middle(sort_win, 1, 0, len + WINDOW_BORDER_LEN, "Sorting", COLOR_PAIR(3));
	mvwaddch(sort_win, WINDOW_HEADER_END_LINE, 0, ACS_LTEE);
	mvwhline(sort_win, WINDOW_HEADER_END_LINE, 1, ACS_HLINE, len + 1);
	mvwaddch(sort_win, WINDOW_HEADER_END_LINE, len + WINDOW_BORDER_LEN - 1, ACS_RTEE);

	post_menu(my_menu);
	refresh();
	wrefresh(sort_win);

	while (!stop_loop) {
		c = wgetch(sort_win);

		switch (c) {
		case KEY_DOWN:
			menu_driver(my_menu, REQ_DOWN_ITEM);
			break;
		case KEY_UP:
			menu_driver(my_menu, REQ_UP_ITEM);
			break;
		case 27: /* ESC */
			stop_loop = true;
			break;
		case 10: /* Enter */
			stop_loop = true;
			cur = current_item(my_menu);
			p = item_userptr(cur);
			p(tab, item_index(cur));
			break;
		}
		wrefresh(sort_win);
	}

	unpost_menu(my_menu);
	free_menu(my_menu);

	for (i = 0; i < elements; ++i) {
		free_item(my_items[i]);
	}

	free(my_items);

	del_panel(sort_panel);
	delwin(sort_win);

	wclear(g_menu_win);
	draw_menu_win();
}

static void
change_refresh_rate(void)
{
	const int WINDOW_HEADER_END_LINE = 2;
	PANEL *refresh_panel;
	WINDOW *refresh_win;
	int c;
	bool stop_loop = false;
	uint32_t rr_tmp, refresh_rate = 0;
	char refresh_rate_str[MAX_STRING_LEN];

	refresh_win = newwin(RR_WIN_HEIGHT, RR_WIN_WIDTH, (g_max_row - RR_WIN_HEIGHT - 1) / 2,
			     (g_max_col - RR_WIN_WIDTH) / 2);
	assert(refresh_win != NULL);
	keypad(refresh_win, TRUE);
	refresh_panel = new_panel(refresh_win);
	assert(refresh_panel != NULL);

	top_panel(refresh_panel);
	update_panels();
	doupdate();

	box(refresh_win, 0, 0);

	print_in_middle(refresh_win, 1, 0, RR_WIN_WIDTH + 1, "Enter refresh rate value [s]", COLOR_PAIR(3));
	mvwaddch(refresh_win, WINDOW_HEADER_END_LINE, 0, ACS_LTEE);
	mvwhline(refresh_win, WINDOW_HEADER_END_LINE, 1, ACS_HLINE, RR_WIN_WIDTH - 2);
	mvwaddch(refresh_win, WINDOW_HEADER_END_LINE, RR_WIN_WIDTH, ACS_RTEE);
	mvwprintw(refresh_win, WINDOW_HEADER_END_LINE + 1, (RR_WIN_WIDTH - 1) / 2, "%d", refresh_rate);

	refresh();
	wrefresh(refresh_win);

	while (!stop_loop) {
		c = wgetch(refresh_win);

		switch (c) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			rr_tmp = refresh_rate * 10 + c - '0';

			if (rr_tmp <= RR_MAX_VALUE) {
				refresh_rate = rr_tmp;
				snprintf(refresh_rate_str, MAX_STRING_LEN - 1, "%d", refresh_rate);
				mvwprintw(refresh_win, WINDOW_HEADER_END_LINE + 1,
					  (RR_WIN_WIDTH - 1 - strlen(refresh_rate_str)) / 2, "%d", refresh_rate);
				refresh();
				wrefresh(refresh_win);
			}
			break;
		case KEY_BACKSPACE:
		case 127:
		case '\b':
			refresh_rate = refresh_rate / 10;
			snprintf(refresh_rate_str, MAX_STRING_LEN - 1, "%d", refresh_rate);
			mvwprintw(refresh_win, WINDOW_HEADER_END_LINE + 1,
				  (RR_WIN_WIDTH - 1 - strlen(refresh_rate_str) - 2) / 2, "       ");
			mvwprintw(refresh_win, WINDOW_HEADER_END_LINE + 1,
				  (RR_WIN_WIDTH - 1 - strlen(refresh_rate_str)) / 2, "%d", refresh_rate);
			refresh();
			wrefresh(refresh_win);
			break;
		case 27: /* ESC */
		case 'q':
			stop_loop = true;
			break;
		case 10: /* Enter */
			g_sleep_time = refresh_rate;
			stop_loop = true;
			break;
		}
		wrefresh(refresh_win);
	}

	del_panel(refresh_panel);
	delwin(refresh_win);
}

static void
free_resources(void)
{
	struct run_counter_history *history, *tmp;

	TAILQ_FOREACH_SAFE(history, &g_run_counter_history, link, tmp) {
		TAILQ_REMOVE(&g_run_counter_history, history, link);
		free(history->poller_name);
		free(history);
	}
}

static uint64_t
get_position_for_window(uint64_t window_size, uint64_t max_size)
{
	/* This function calculates position for pop-up detail window.
	 * Since horizontal and vertical positions are calculated the same way
	 * there is no need for separate functions. */
	window_size = spdk_min(window_size, max_size);

	return (max_size - window_size) / 2;
}

static void
display_thread(struct rpc_thread_info *thread_info)
{
	PANEL *thread_panel;
	WINDOW *thread_win;
	struct rpc_poller_thread_info *thread;
	struct rpc_pollers *pollers;
	struct rpc_poller_info *poller;
	uint64_t pollers_count, current_row, i, j, time;
	int c;
	bool stop_loop = false;
	char idle_time[MAX_TIME_STR_LEN], busy_time[MAX_TIME_STR_LEN], run_count[MAX_POLLER_COUNT_STR_LEN];

	pollers_count = thread_info->active_pollers_count +
			thread_info->timed_pollers_count +
			thread_info->paused_pollers_count;

	thread_win = newwin(pollers_count + THREAD_WIN_HEIGHT, THREAD_WIN_WIDTH,
			    get_position_for_window(THREAD_WIN_HEIGHT + pollers_count, g_max_row),
			    get_position_for_window(THREAD_WIN_WIDTH, g_max_col));
	keypad(thread_win, TRUE);
	thread_panel = new_panel(thread_win);

	top_panel(thread_panel);
	update_panels();
	doupdate();

	box(thread_win, 0, 0);

	print_in_middle(thread_win, 1, 0, THREAD_WIN_WIDTH, thread_info->name,
			COLOR_PAIR(3));
	mvwhline(thread_win, 2, 1, ACS_HLINE, THREAD_WIN_WIDTH - 2);
	mvwaddch(thread_win, 2, THREAD_WIN_WIDTH, ACS_RTEE);

	print_left(thread_win, 3, THREAD_WIN_FIRST_COL, THREAD_WIN_WIDTH,
		   "Core:                Idle [us]:            Busy [us]:", COLOR_PAIR(5));
	mvwprintw(thread_win, 3, THREAD_WIN_FIRST_COL + 6, "%" PRIu64,
		  thread_info->core_num);

	if (g_interval_data) {
		get_time_str(g_thread_history[thread_info->id].idle, idle_time);
		mvwprintw(thread_win, 3, THREAD_WIN_FIRST_COL + 32, idle_time);
		get_time_str(g_thread_history[thread_info->id].busy, busy_time);
		mvwprintw(thread_win, 3, THREAD_WIN_FIRST_COL + 54, busy_time);
	} else {
		get_time_str(thread_info->idle, idle_time);
		mvwprintw(thread_win, 3, THREAD_WIN_FIRST_COL + 32, idle_time);
		get_time_str(thread_info->busy, busy_time);
		mvwprintw(thread_win, 3, THREAD_WIN_FIRST_COL + 54, busy_time);
	}

	print_left(thread_win, 4, THREAD_WIN_FIRST_COL, THREAD_WIN_WIDTH,
		   "Active pollers:      Timed pollers:        Paused pollers:", COLOR_PAIR(5));
	mvwprintw(thread_win, 4, THREAD_WIN_FIRST_COL + 17, "%" PRIu64,
		  thread_info->active_pollers_count);
	mvwprintw(thread_win, 4, THREAD_WIN_FIRST_COL + 36, "%" PRIu64,
		  thread_info->timed_pollers_count);
	mvwprintw(thread_win, 4, THREAD_WIN_FIRST_COL + 59, "%" PRIu64,
		  thread_info->paused_pollers_count);

	mvwhline(thread_win, 5, 1, ACS_HLINE, THREAD_WIN_WIDTH - 2);

	print_in_middle(thread_win, 6, 0, THREAD_WIN_WIDTH,
			"Pollers                          Type    Total run count   Period", COLOR_PAIR(5));

	mvwhline(thread_win, 7, 1, ACS_HLINE, THREAD_WIN_WIDTH - 2);

	current_row = 8;

	for (i = 0; i < g_pollers_stats.pollers_threads.threads_count; i++) {
		thread = &g_pollers_stats.pollers_threads.threads[i];
		if (thread->id == thread_info->id) {
			pollers = &thread->active_pollers;
			for (j = 0; j < pollers->pollers_count; j++) {
				poller = &pollers->pollers[j];
				mvwprintw(thread_win, current_row, THREAD_WIN_FIRST_COL, "%s", poller->name);
				mvwprintw(thread_win, current_row, THREAD_WIN_FIRST_COL + 33, "Active");
				snprintf(run_count, MAX_POLLER_COUNT_STR_LEN, "%" PRIu64, poller->run_count);
				mvwprintw(thread_win, current_row, THREAD_WIN_FIRST_COL + 41, run_count);
				current_row++;
			}
			pollers = &thread->timed_pollers;
			for (j = 0; j < pollers->pollers_count; j++) {
				poller = &pollers->pollers[j];
				mvwprintw(thread_win, current_row, THREAD_WIN_FIRST_COL, "%s", poller->name);
				mvwprintw(thread_win, current_row, THREAD_WIN_FIRST_COL + 33, "Timed");
				mvwprintw(thread_win, current_row, THREAD_WIN_FIRST_COL + 41, "%" PRIu64, poller->run_count);
				time = poller->period_ticks * SPDK_SEC_TO_USEC / g_cores_stats.tick_rate;
				mvwprintw(thread_win, current_row, THREAD_WIN_FIRST_COL + 59, "%" PRIu64, time);
				current_row++;
			}
			pollers = &thread->paused_pollers;
			for (j = 0; j < pollers->pollers_count; j++) {
				poller = &pollers->pollers[j];
				mvwprintw(thread_win, current_row, THREAD_WIN_FIRST_COL, "%s", poller->name);
				mvwprintw(thread_win, current_row, THREAD_WIN_FIRST_COL + 33, "Paused");
				mvwprintw(thread_win, current_row, THREAD_WIN_FIRST_COL + 41, "%" PRIu64, poller->run_count);
				current_row++;
			}
		}
	}

	refresh();
	wrefresh(thread_win);

	while (!stop_loop) {
		c = wgetch(thread_win);

		switch (c) {
		case 27: /* ESC */
			stop_loop = true;
			break;
		default:
			break;
		}
	}

	del_panel(thread_panel);
	delwin(thread_win);
}

static void
show_thread(uint8_t current_page)
{
	struct rpc_thread_info *thread_info[g_threads_stats.threads.threads_count];
	uint64_t thread_number = current_page * g_max_data_rows + g_selected_row;
	uint64_t i;

	get_data();

	assert(thread_number < g_threads_stats.threads.threads_count);
	for (i = 0; i < g_threads_stats.threads.threads_count; i++) {
		thread_info[i] = &g_threads_stats.threads.thread_info[i];
	}

	qsort(thread_info, g_threads_stats.threads.threads_count, sizeof(thread_info[0]), sort_threads);

	display_thread(thread_info[thread_number]);

	free_data();
}

static void
show_single_thread(uint64_t thread_id)
{
	uint64_t i;

	for (i = 0; i < g_threads_stats.threads.threads_count; i++) {
		if (g_threads_stats.threads.thread_info[i].id == thread_id) {
			display_thread(&g_threads_stats.threads.thread_info[i]);
			break;
		}
	}
}

static void
show_core(uint8_t current_page)
{
	PANEL *core_panel;
	WINDOW *core_win;
	uint64_t core_number = current_page * g_max_data_rows + g_selected_row;
	struct rpc_core_info *core_info[g_cores_stats.cores.cores_count];
	uint64_t threads_count, i, j;
	uint16_t current_threads_row;
	int c;
	char core_win_title[25];
	bool stop_loop = false;
	char idle_time[MAX_TIME_STR_LEN], busy_time[MAX_TIME_STR_LEN];

	get_data();

	assert(core_number < g_cores_stats.cores.cores_count);
	for (i = 0; i < g_cores_stats.cores.cores_count; i++) {
		core_info[i] = &g_cores_stats.cores.core[i];
	}

	threads_count = g_cores_stats.cores.core->threads.threads_count;
	core_win = newwin(threads_count + CORE_WIN_HEIGHT, CORE_WIN_WIDTH,
			  get_position_for_window(CORE_WIN_HEIGHT + threads_count, g_max_row),
			  get_position_for_window(CORE_WIN_WIDTH, g_max_col));

	keypad(core_win, TRUE);
	core_panel = new_panel(core_win);

	top_panel(core_panel);
	update_panels();
	doupdate();

	box(core_win, 0, 0);
	snprintf(core_win_title, sizeof(core_win_title), "Core %" PRIu64 " details", core_number);
	print_in_middle(core_win, 1, 0, CORE_WIN_WIDTH, core_win_title, COLOR_PAIR(3));

	mvwaddch(core_win, -1, 0, ACS_LTEE);
	mvwhline(core_win, 2, 1, ACS_HLINE, CORE_WIN_WIDTH - 2);
	mvwaddch(core_win, 2, CORE_WIN_WIDTH, ACS_RTEE);
	print_in_middle(core_win, 3, 0, CORE_WIN_WIDTH - (CORE_WIN_WIDTH / 3), "Frequency:", COLOR_PAIR(5));
	if (core_info[core_number]->core_freq) {
		mvwprintw(core_win, 3, CORE_WIN_FIRST_COL + 15, "%" PRIu32,
			  core_info[core_number]->core_freq);
	} else {
		mvwprintw(core_win, 3, CORE_WIN_FIRST_COL + 15, "%s", "N/A");
	}

	mvwaddch(core_win, -1, 0, ACS_LTEE);
	mvwhline(core_win, 4, 1, ACS_HLINE, CORE_WIN_WIDTH - 2);
	mvwaddch(core_win, 4, CORE_WIN_WIDTH, ACS_RTEE);
	print_left(core_win, 5, 1, CORE_WIN_WIDTH, "Thread count:          Idle time:", COLOR_PAIR(5));

	mvwprintw(core_win, 5, CORE_WIN_FIRST_COL, "%" PRIu64,
		  g_cores_history[core_number].threads_count);

	if (g_interval_data == true) {
		get_time_str(g_cores_history[core_number].idle, idle_time);
		get_time_str(g_cores_history[core_number].busy, busy_time);
	} else {
		get_time_str(core_info[core_number]->idle, idle_time);
		get_time_str(core_info[core_number]->busy, busy_time);
	}
	mvwprintw(core_win, 5, CORE_WIN_FIRST_COL + 20, idle_time);

	print_left(core_win, 7, 1, CORE_WIN_WIDTH, "Poller count:          Busy time:", COLOR_PAIR(5));
	mvwprintw(core_win, 7, CORE_WIN_FIRST_COL, "%" PRIu64,
		  g_cores_history[core_number].pollers_count);

	mvwprintw(core_win, 7, CORE_WIN_FIRST_COL + 20, busy_time);

	mvwhline(core_win, 6, 1, ACS_HLINE, CORE_WIN_WIDTH - 2);
	mvwhline(core_win, 8, 1, ACS_HLINE, CORE_WIN_WIDTH - 2);
	print_left(core_win, 9, 1, CORE_WIN_WIDTH, "Threads on this core", COLOR_PAIR(5));

	for (j = 0; j < core_info[core_number]->threads.threads_count; j++) {
		mvwprintw(core_win, j + 10, 1, core_info[core_number]->threads.thread[j].name);
	}

	refresh();
	wrefresh(core_win);

	current_threads_row = 0;

	while (!stop_loop) {
		for (j = 0; j < core_info[core_number]->threads.threads_count; j++) {
			if (j != current_threads_row) {
				mvwprintw(core_win, j + 10, 1, core_info[core_number]->threads.thread[j].name);
			} else {
				print_left(core_win, j + 10, 1, CORE_WIN_WIDTH - 2,
					   core_info[core_number]->threads.thread[j].name, COLOR_PAIR(2));
			}
		}

		wrefresh(core_win);

		c = wgetch(core_win);
		switch (c) {
		case 10: /* ENTER */
			show_single_thread(core_info[core_number]->threads.thread[current_threads_row].id);
			break;
		case 27: /* ESC */
			stop_loop = true;
			break;
		case KEY_UP:
			if (current_threads_row != 0) {
				current_threads_row--;
			}
			break;
		case KEY_DOWN:
			if (current_threads_row != core_info[core_number]->threads.threads_count - 1) {
				current_threads_row++;
			}
			break;
		default:
			break;
		}
	}

	del_panel(core_panel);
	delwin(core_win);

	free_data();
}

static void
show_poller(uint8_t current_page)
{
	PANEL *poller_panel;
	WINDOW *poller_win;
	uint64_t count = 0;
	uint64_t poller_number = current_page * g_max_data_rows + g_selected_row;
	struct rpc_poller_info *pollers[RPC_MAX_POLLERS];
	bool stop_loop = false;
	char poller_period[MAX_TIME_STR_LEN];
	int c;

	get_data();

	prepare_poller_data(current_page, pollers, &count, current_page);
	assert(poller_number < count);

	poller_win = newwin(POLLER_WIN_HEIGHT, POLLER_WIN_WIDTH,
			    get_position_for_window(POLLER_WIN_HEIGHT, g_max_row),
			    get_position_for_window(POLLER_WIN_WIDTH, g_max_col));

	keypad(poller_win, TRUE);
	poller_panel = new_panel(poller_win);

	top_panel(poller_panel);
	update_panels();
	doupdate();

	box(poller_win, 0, 0);

	print_in_middle(poller_win, 1, 0, POLLER_WIN_WIDTH, pollers[poller_number]->name, COLOR_PAIR(3));
	mvwhline(poller_win, 2, 1, ACS_HLINE, POLLER_WIN_WIDTH - 2);
	mvwaddch(poller_win, 2, POLLER_WIN_WIDTH, ACS_RTEE);

	print_left(poller_win, 3, 2, POLLER_WIN_WIDTH, "Type:                  On thread:", COLOR_PAIR(5));
	mvwprintw(poller_win, 3, POLLER_WIN_FIRST_COL,
		  poller_type_str[pollers[poller_number]->type]);
	mvwprintw(poller_win, 3, POLLER_WIN_FIRST_COL + 23, pollers[poller_number]->thread_name);

	print_left(poller_win, 4, 2, POLLER_WIN_WIDTH, "Run count:", COLOR_PAIR(5));

	if (g_interval_data) {
		mvwprintw(poller_win, 4, POLLER_WIN_FIRST_COL, "%" PRIu64,
			  g_pollers_history[poller_number].run_count);
	} else {
		mvwprintw(poller_win, 4, POLLER_WIN_FIRST_COL, "%" PRIu64,
			  pollers[poller_number]->run_count);
	}

	if (pollers[poller_number]->period_ticks != 0) {
		print_left(poller_win, 4, 28, POLLER_WIN_WIDTH, "Period:", COLOR_PAIR(5));
		get_time_str(g_pollers_history[poller_number].period_ticks, poller_period);
		mvwprintw(poller_win, 4, POLLER_WIN_FIRST_COL + 23, poller_period);
	}
	mvwhline(poller_win, 5, 1, ACS_HLINE, POLLER_WIN_WIDTH - 2);
	print_in_middle(poller_win, 6, 1, POLLER_WIN_WIDTH - 7, "Status:", COLOR_PAIR(5));

	if (pollers[poller_number]->busy_count > 0) {
		print_in_middle(poller_win, 6, 1, POLLER_WIN_WIDTH + 6, "Busy", COLOR_PAIR(6));
	} else {
		print_in_middle(poller_win, 6, 1, POLLER_WIN_WIDTH + 6, "Idle", COLOR_PAIR(7));
	}

	refresh();
	wrefresh(poller_win);

	while (!stop_loop) {
		c = wgetch(poller_win);
		switch (c) {
		case 27: /* ESC */
			stop_loop = true;
			break;
		default:
			break;
		}
	}

	del_panel(poller_panel);
	delwin(poller_win);

	free_data();
}

static void
show_stats(void)
{
	const int CURRENT_PAGE_STR_LEN = 50;
	const char *refresh_error = "ERROR occurred while getting data";
	long int time_last, time_dif;
	struct timespec time_now;
	int c, rc;
	int max_row, max_col;
	uint8_t active_tab = THREADS_TAB;
	uint8_t current_page = 0;
	uint8_t max_pages = 1;
	uint16_t required_size = WINDOW_HEADER + 1;
	char current_page_str[CURRENT_PAGE_STR_LEN];
	bool force_refresh = true;

	clock_gettime(CLOCK_REALTIME, &time_now);
	time_last = time_now.tv_sec;

	switch_tab(THREADS_TAB);

	while (1) {
		/* Check if interface has to be resized (terminal size changed) */
		getmaxyx(stdscr, max_row, max_col);

		if (max_row != g_max_row || max_col != g_max_col) {
			g_max_row = spdk_max(max_row, required_size);
			g_max_col = max_col;
			g_data_win_size = g_max_row - required_size + 1;
			g_max_data_rows = g_max_row - WINDOW_HEADER;
			resize_interface(active_tab);
		}

		c = getch();
		if (c == 'q') {
			free_resources();
			break;
		}

		force_refresh = true;

		switch (c) {
		case '1':
		case '2':
		case '3':
			active_tab = c - '1';
			current_page = 0;
			g_selected_row = 0;
			switch_tab(active_tab);
			break;
		case '\t':
			if (active_tab < NUMBER_OF_TABS - 1) {
				active_tab++;
			} else {
				active_tab = THREADS_TAB;
			}
			g_selected_row = 0;
			current_page = 0;
			switch_tab(active_tab);
			break;
		case 's':
			change_sorting(active_tab);
			break;
		case 'c':
			filter_columns(active_tab);
			break;
		case 'r':
			change_refresh_rate();
			break;
		case 't':
			g_interval_data = !g_interval_data;
			break;
		case KEY_NPAGE: /* PgDown */
			if (current_page + 1 < max_pages) {
				current_page++;
			}
			wclear(g_tabs[active_tab]);
			g_selected_row = 0;
			draw_tabs(active_tab, g_current_sort_col[active_tab]);
			break;
		case KEY_PPAGE: /* PgUp */
			if (current_page > 0) {
				current_page--;
			}
			wclear(g_tabs[active_tab]);
			g_selected_row = 0;
			draw_tabs(active_tab, g_current_sort_col[active_tab]);
			break;
		case KEY_UP: /* Arrow up */
			if (g_selected_row > 0) {
				g_selected_row--;
			}
			break;
		case KEY_DOWN: /* Arrow down */
			if (g_selected_row < g_max_selected_row) {
				g_selected_row++;
			}
			break;
		case 10: /* Enter */
			if (active_tab == THREADS_TAB) {
				show_thread(current_page);
			} else if (active_tab == CORES_TAB) {
				show_core(current_page);
			} else if (active_tab == POLLERS_TAB) {
				show_poller(current_page);
			}
			break;
		default:
			force_refresh = false;
			break;
		}

		clock_gettime(CLOCK_REALTIME, &time_now);
		time_dif = time_now.tv_sec - time_last;
		if (time_dif < 0) {
			time_dif = g_sleep_time;
		}

		if (time_dif >= g_sleep_time || force_refresh) {
			time_last = time_now.tv_sec;
			rc = get_data();
			if (rc) {
				mvprintw(g_max_row - 1, g_max_col - strlen(refresh_error) - 2, refresh_error);
			}

			max_pages = refresh_tab(active_tab, current_page);

			snprintf(current_page_str, CURRENT_PAGE_STR_LEN - 1, "Page: %d/%d", current_page + 1, max_pages);
			mvprintw(g_max_row - 1, 1, current_page_str);

			free_data();

			refresh();
		}
	}
}

static void
draw_interface(void)
{
	int i;
	uint16_t required_size =  WINDOW_HEADER + 1;

	getmaxyx(stdscr, g_max_row, g_max_col);
	g_max_row = spdk_max(g_max_row, required_size);
	g_data_win_size = g_max_row - required_size;
	g_max_data_rows = g_max_row - WINDOW_HEADER;

	g_menu_win = newwin(MENU_WIN_HEIGHT, g_max_col, g_max_row - MENU_WIN_HEIGHT - 1,
			    MENU_WIN_LOCATION_COL);
	assert(g_menu_win != NULL);
	draw_menu_win();

	for (i = 0; i < NUMBER_OF_TABS; i++) {
		g_tab_win[i] = newwin(TAB_WIN_HEIGHT, g_max_col / NUMBER_OF_TABS - TABS_SPACING,
				      TAB_WIN_LOCATION_ROW, g_max_col / NUMBER_OF_TABS * i + 1);
		assert(g_tab_win[i] != NULL);
		draw_tab_win(i);

		g_tabs[i] = newwin(g_max_row - MENU_WIN_HEIGHT - TAB_WIN_HEIGHT - 2, g_max_col, TABS_LOCATION_ROW,
				   TABS_LOCATION_COL);
		draw_tabs(i, g_current_sort_col[i]);
		g_panels[i] = new_panel(g_tabs[i]);
		assert(g_panels[i] != NULL);
	}

	update_panels();
	doupdate();
}

static void finish(int sig)
{
	/* End ncurses mode */
	endwin();
	spdk_jsonrpc_client_close(g_rpc_client);
	exit(0);
}

static void
setup_ncurses(void)
{
	clear();
	noecho();
	timeout(1);
	curs_set(0);
	keypad(stdscr, TRUE);
	start_color();
	init_pair(1, COLOR_BLACK, COLOR_GREEN);
	init_pair(2, COLOR_BLACK, COLOR_WHITE);
	init_pair(3, COLOR_YELLOW, COLOR_BLACK);
	init_pair(4, COLOR_BLACK, COLOR_YELLOW);
	init_pair(5, COLOR_GREEN, COLOR_BLACK);
	init_pair(6, COLOR_RED, COLOR_BLACK);
	init_pair(7, COLOR_BLUE, COLOR_BLACK);
	init_pair(8, COLOR_RED, COLOR_WHITE);
	init_pair(9, COLOR_BLUE, COLOR_WHITE);

	if (has_colors() == FALSE) {
		endwin();
		printf("Your terminal does not support color\n");
		exit(1);
	}

	/* Handle signals to exit gracfully cleaning up ncurses */
	(void) signal(SIGINT, finish);
	(void) signal(SIGPIPE, finish);
	(void) signal(SIGABRT, finish);
}

static void
usage(const char *program_name)
{
	printf("%s [options]", program_name);
	printf("\n");
	printf("options:\n");
	printf(" -r <path>  RPC connect address (default: /var/tmp/spdk.sock)\n");
	printf(" -h         show this usage\n");
}

static int
wait_init(void)
{
	struct spdk_jsonrpc_client_response *json_resp = NULL;
	char *uninit_log = "Waiting for SPDK target application to initialize...",
	      *uninit_error = "Unable to read SPDK application state!";
	int c, max_col, rc = 0;

	max_col = getmaxx(stdscr);
	print_in_middle(stdscr, FIRST_DATA_ROW, 1, max_col, uninit_log, COLOR_PAIR(5));
	rc = rpc_send_req("framework_wait_init", &json_resp);
	if (rc) {
		spdk_jsonrpc_client_free_response(json_resp);

		while (1) {
			print_in_middle(stdscr, FIRST_DATA_ROW, 1, max_col, uninit_error, COLOR_PAIR(8));
			c = getch();
			if (c == 'q') {
				return -1;
			}
		}
	}

	spdk_jsonrpc_client_free_response(json_resp);
	return 0;
}

int main(int argc, char **argv)
{
	int op, rc;
	char *socket = SPDK_DEFAULT_RPC_ADDR;

	while ((op = getopt(argc, argv, "r:h")) != -1) {
		switch (op) {
		case 'r':
			socket = optarg;
			break;
		default:
			usage(argv[0]);
			return op == 'h' ? 0 : 1;
		}
	}

	g_rpc_client = spdk_jsonrpc_client_connect(socket, AF_UNIX);
	if (!g_rpc_client) {
		fprintf(stderr, "spdk_jsonrpc_client_connect() failed: %d\n", errno);
		return 1;
	}

	initscr();
	init_str_len();
	setup_ncurses();
	draw_interface();

	rc = wait_init();
	if (!rc) {
		show_stats();
	}

	finish(0);

	return (0);
}
