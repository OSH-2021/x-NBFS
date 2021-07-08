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

#include "spdk_cunit.h"
#include "common/lib/test_env.c"
#include "event/reactor.c"
#include "spdk/thread.h"
#include "event/scheduler_static.c"
#include "event/scheduler_dynamic.c"

DEFINE_STUB(_spdk_get_app_thread, struct spdk_thread *, (void), NULL);

static void
test_create_reactor(void)
{
	struct spdk_reactor reactor = {};

	g_reactors = &reactor;
	g_reactor_count = 1;

	reactor_construct(&reactor, 0);

	CU_ASSERT(spdk_reactor_get(0) == &reactor);

	spdk_ring_free(reactor.events);
	reactor_interrupt_fini(&reactor);
	g_reactors = NULL;
}

static void
test_init_reactors(void)
{
	uint32_t core;

	MOCK_SET(spdk_env_get_current_core, 0);

	allocate_cores(3);

	CU_ASSERT(spdk_reactors_init() == 0);

	CU_ASSERT(g_reactor_state == SPDK_REACTOR_STATE_INITIALIZED);
	for (core = 0; core < 3; core++) {
		CU_ASSERT(spdk_reactor_get(core) != NULL);
	}

	spdk_reactors_fini();

	free_cores();

	MOCK_CLEAR(spdk_env_get_current_core);
}

static void
ut_event_fn(void *arg1, void *arg2)
{
	uint8_t *test1 = arg1;
	uint8_t *test2 = arg2;

	*test1 = 1;
	*test2 = 0xFF;
}

static void
test_event_call(void)
{
	uint8_t test1 = 0, test2 = 0;
	struct spdk_event *evt;
	struct spdk_reactor *reactor;

	MOCK_SET(spdk_env_get_current_core, 0);

	allocate_cores(1);

	CU_ASSERT(spdk_reactors_init() == 0);

	evt = spdk_event_allocate(0, ut_event_fn, &test1, &test2);
	CU_ASSERT(evt != NULL);

	MOCK_SET(spdk_env_get_current_core, 0);

	spdk_event_call(evt);

	reactor = spdk_reactor_get(0);
	CU_ASSERT(reactor != NULL);

	CU_ASSERT(event_queue_run_batch(reactor) == 1);
	CU_ASSERT(test1 == 1);
	CU_ASSERT(test2 == 0xFF);

	MOCK_CLEAR(spdk_env_get_current_core);

	spdk_reactors_fini();

	free_cores();

	MOCK_CLEAR(spdk_env_get_current_core);
}

static void
test_schedule_thread(void)
{
	struct spdk_cpuset cpuset = {};
	struct spdk_thread *thread;
	struct spdk_reactor *reactor;
	struct spdk_lw_thread *lw_thread;

	MOCK_SET(spdk_env_get_current_core, 0);

	allocate_cores(5);

	CU_ASSERT(spdk_reactors_init() == 0);

	spdk_cpuset_set_cpu(&cpuset, 3, true);
	g_next_core = 4;

	MOCK_SET(spdk_env_get_current_core, 3);

	/* _reactor_schedule_thread() will be called in spdk_thread_create()
	 * at its end because it is passed to SPDK thread library by
	 * spdk_thread_lib_init().
	 */
	thread = spdk_thread_create(NULL, &cpuset);
	CU_ASSERT(thread != NULL);

	reactor = spdk_reactor_get(3);
	CU_ASSERT(reactor != NULL);

	CU_ASSERT(event_queue_run_batch(reactor) == 1);

	MOCK_CLEAR(spdk_env_get_current_core);

	lw_thread = TAILQ_FIRST(&reactor->threads);
	CU_ASSERT(lw_thread != NULL);
	CU_ASSERT(spdk_thread_get_from_ctx(lw_thread) == thread);

	TAILQ_REMOVE(&reactor->threads, lw_thread, link);
	reactor->thread_count--;
	spdk_set_thread(thread);
	spdk_thread_exit(thread);
	while (!spdk_thread_is_exited(thread)) {
		spdk_thread_poll(thread, 0, 0);
	}
	spdk_thread_destroy(thread);
	spdk_set_thread(NULL);

	spdk_reactors_fini();

	free_cores();
}

static void
test_reschedule_thread(void)
{
	struct spdk_cpuset cpuset = {};
	struct spdk_thread *thread;
	struct spdk_reactor *reactor;
	struct spdk_lw_thread *lw_thread;

	MOCK_SET(spdk_env_get_current_core, 0);

	allocate_cores(3);

	CU_ASSERT(spdk_reactors_init() == 0);

	spdk_cpuset_set_cpu(&g_reactor_core_mask, 0, true);
	spdk_cpuset_set_cpu(&g_reactor_core_mask, 1, true);
	spdk_cpuset_set_cpu(&g_reactor_core_mask, 2, true);
	g_next_core = 0;

	MOCK_SET(spdk_env_get_current_core, 1);
	/* Create and schedule the thread to core 1. */
	spdk_cpuset_set_cpu(&cpuset, 1, true);

	thread = spdk_thread_create(NULL, &cpuset);
	CU_ASSERT(thread != NULL);
	lw_thread = spdk_thread_get_ctx(thread);

	reactor = spdk_reactor_get(1);
	CU_ASSERT(reactor != NULL);

	CU_ASSERT(event_queue_run_batch(reactor) == 1);
	CU_ASSERT(TAILQ_FIRST(&reactor->threads) == lw_thread);

	spdk_set_thread(thread);

	/* Call spdk_thread_set_cpumask() twice with different cpumask values.
	 * The cpumask of the 2nd call will be used in reschedule operation.
	 */

	spdk_cpuset_zero(&cpuset);
	spdk_cpuset_set_cpu(&cpuset, 0, true);
	CU_ASSERT(spdk_thread_set_cpumask(&cpuset) == 0);

	spdk_cpuset_zero(&cpuset);
	spdk_cpuset_set_cpu(&cpuset, 2, true);
	CU_ASSERT(spdk_thread_set_cpumask(&cpuset) == 0);

	CU_ASSERT(lw_thread->resched == true);

	reactor_run(reactor);

	CU_ASSERT(lw_thread->resched == false);
	CU_ASSERT(TAILQ_EMPTY(&reactor->threads));

	reactor = spdk_reactor_get(0);
	CU_ASSERT(reactor != NULL);
	MOCK_SET(spdk_env_get_current_core, 0);

	CU_ASSERT(event_queue_run_batch(reactor) == 0);

	reactor = spdk_reactor_get(2);
	CU_ASSERT(reactor != NULL);
	MOCK_SET(spdk_env_get_current_core, 2);

	CU_ASSERT(event_queue_run_batch(reactor) == 1);

	CU_ASSERT(TAILQ_FIRST(&reactor->threads) == lw_thread);

	MOCK_CLEAR(spdk_env_get_current_core);

	TAILQ_REMOVE(&reactor->threads, lw_thread, link);
	reactor->thread_count--;
	spdk_set_thread(thread);
	spdk_thread_exit(thread);
	while (!spdk_thread_is_exited(thread)) {
		spdk_thread_poll(thread, 0, 0);
	}
	spdk_thread_destroy(thread);
	spdk_set_thread(NULL);

	spdk_reactors_fini();

	free_cores();
}

static void
for_each_reactor_done(void *arg1, void *arg2)
{
	uint32_t *count = arg1;
	bool *done = arg2;

	(*count)++;
	*done = true;
}

static void
for_each_reactor_cb(void *arg1, void *arg2)
{
	uint32_t *count = arg1;

	(*count)++;
}

static void
test_for_each_reactor(void)
{
	uint32_t count = 0, i;
	bool done = false;
	struct spdk_reactor *reactor;

	MOCK_SET(spdk_env_get_current_core, 0);

	allocate_cores(5);

	CU_ASSERT(spdk_reactors_init() == 0);

	spdk_for_each_reactor(for_each_reactor_cb, &count, &done, for_each_reactor_done);

	MOCK_CLEAR(spdk_env_get_current_core);

	/* We have not processed any event yet, so count and done should be 0 and false,
	 *  respectively.
	 */
	CU_ASSERT(count == 0);

	/* Poll each reactor to verify the event is passed to each */
	for (i = 0; i < 5; i++) {
		reactor = spdk_reactor_get(i);
		CU_ASSERT(reactor != NULL);
		MOCK_SET(spdk_env_get_current_core, i);

		event_queue_run_batch(reactor);
		CU_ASSERT(count == (i + 1));
		CU_ASSERT(done == false);
		MOCK_CLEAR(spdk_env_get_current_core);
	}

	MOCK_SET(spdk_env_get_current_core, 0);
	/* After each reactor is called, the completion calls it one more time. */
	reactor = spdk_reactor_get(0);
	CU_ASSERT(reactor != NULL);

	event_queue_run_batch(reactor);
	CU_ASSERT(count == 6);
	CU_ASSERT(done == true);
	MOCK_CLEAR(spdk_env_get_current_core);

	spdk_reactors_fini();

	free_cores();
}

static int
poller_run_idle(void *ctx)
{
	uint64_t delay_us = (uint64_t)ctx;

	spdk_delay_us(delay_us);

	return 0;
}

static int
poller_run_busy(void *ctx)
{
	uint64_t delay_us = (uint64_t)ctx;

	spdk_delay_us(delay_us);

	return 1;
}

static void
test_reactor_stats(void)
{
	struct spdk_cpuset cpuset = {};
	struct spdk_thread *thread1, *thread2;
	struct spdk_reactor *reactor;
	struct spdk_poller *busy1, *idle1, *busy2, *idle2;
	struct spdk_thread_stats stats;
	int rc __attribute__((unused));

	/* Test case is the following:
	 * Create a reactor on CPU core0.
	 * Create thread1 and thread2 simultaneously on reactor0 at TSC = 100.
	 * Reactor runs
	 * - thread1 for 100 with busy
	 * - thread2 for 200 with idle
	 * - thread1 for 300 with idle
	 * - thread2 for 400 with busy.
	 * Then,
	 * - both elapsed TSC of thread1 and thread2 should be 1000 (= 100 + 900).
	 * - busy TSC of reactor should be 500 (= 100 + 400).
	 * - idle TSC of reactor should be 500 (= 200 + 300).
	 */

	MOCK_SET(spdk_env_get_current_core, 0);

	allocate_cores(1);

	CU_ASSERT(spdk_reactors_init() == 0);

	spdk_cpuset_set_cpu(&cpuset, 0, true);

	MOCK_SET(spdk_get_ticks, 100);

	thread1 = spdk_thread_create(NULL, &cpuset);
	SPDK_CU_ASSERT_FATAL(thread1 != NULL);

	thread2 = spdk_thread_create(NULL, &cpuset);
	SPDK_CU_ASSERT_FATAL(thread2 != NULL);

	reactor = spdk_reactor_get(0);
	SPDK_CU_ASSERT_FATAL(reactor != NULL);

	reactor->tsc_last = 100;

	spdk_set_thread(thread1);
	busy1 = spdk_poller_register(poller_run_busy, (void *)100, 0);
	CU_ASSERT(busy1 != NULL);

	spdk_set_thread(thread2);
	idle2 = spdk_poller_register(poller_run_idle, (void *)300, 0);
	CU_ASSERT(idle2 != NULL);

	_reactor_run(reactor);

	spdk_set_thread(thread1);
	CU_ASSERT(spdk_thread_get_last_tsc(thread1) == 200);
	CU_ASSERT(spdk_thread_get_stats(&stats) == 0);
	CU_ASSERT(stats.busy_tsc == 100);
	CU_ASSERT(stats.idle_tsc == 0);
	spdk_set_thread(thread2);
	CU_ASSERT(spdk_thread_get_last_tsc(thread2) == 500);
	CU_ASSERT(spdk_thread_get_stats(&stats) == 0);
	CU_ASSERT(stats.busy_tsc == 0);
	CU_ASSERT(stats.idle_tsc == 300);

	CU_ASSERT(reactor->busy_tsc == 100);
	CU_ASSERT(reactor->idle_tsc == 300);

	spdk_set_thread(thread1);
	spdk_poller_unregister(&busy1);
	idle1 = spdk_poller_register(poller_run_idle, (void *)200, 0);
	CU_ASSERT(idle1 != NULL);

	spdk_set_thread(thread2);
	spdk_poller_unregister(&idle2);
	busy2 = spdk_poller_register(poller_run_busy, (void *)400, 0);
	CU_ASSERT(busy2 != NULL);

	_reactor_run(reactor);

	spdk_set_thread(thread1);
	CU_ASSERT(spdk_thread_get_last_tsc(thread1) == 700);
	CU_ASSERT(spdk_thread_get_stats(&stats) == 0);
	CU_ASSERT(stats.busy_tsc == 100);
	CU_ASSERT(stats.idle_tsc == 200);
	spdk_set_thread(thread2);
	CU_ASSERT(spdk_thread_get_last_tsc(thread2) == 1100);
	CU_ASSERT(spdk_thread_get_stats(&stats) == 0);
	CU_ASSERT(stats.busy_tsc == 400);
	CU_ASSERT(stats.idle_tsc == 300);

	CU_ASSERT(reactor->busy_tsc == 500);
	CU_ASSERT(reactor->idle_tsc == 500);

	spdk_set_thread(thread1);
	spdk_poller_unregister(&idle1);
	spdk_thread_exit(thread1);

	spdk_set_thread(thread2);
	spdk_poller_unregister(&busy2);
	spdk_thread_exit(thread2);

	_reactor_run(reactor);

	CU_ASSERT(TAILQ_EMPTY(&reactor->threads));

	spdk_reactors_fini();

	free_cores();

	MOCK_CLEAR(spdk_env_get_current_core);
}

static void
test_scheduler(void)
{
	struct spdk_cpuset cpuset = {};
	struct spdk_thread *thread[3];
	struct spdk_lw_thread *lw_thread;
	struct spdk_reactor *reactor;
	struct spdk_poller *busy, *idle;
	int i;

	MOCK_SET(spdk_env_get_current_core, 0);

	allocate_cores(3);

	CU_ASSERT(spdk_reactors_init() == 0);

	_spdk_scheduler_set("dynamic");

	for (i = 0; i < 3; i++) {
		spdk_cpuset_set_cpu(&g_reactor_core_mask, i, true);
	}
	g_next_core = 0;

	/* Create threads. */
	for (i = 0; i < 3; i++) {
		spdk_cpuset_set_cpu(&cpuset, i, true);
		thread[i] = spdk_thread_create(NULL, &cpuset);
		CU_ASSERT(thread[i] != NULL);
	}

	for (i = 0; i < 3; i++) {
		reactor = spdk_reactor_get(i);
		CU_ASSERT(reactor != NULL);
		MOCK_SET(spdk_env_get_current_core, i);
		CU_ASSERT(event_queue_run_batch(reactor) == 1);
		CU_ASSERT(!TAILQ_EMPTY(&reactor->threads));
	}

	g_reactor_state = SPDK_REACTOR_STATE_RUNNING;

	MOCK_SET(spdk_env_get_current_core, 0);

	/* Init threads stats (low load) */
	for (i = 0; i < 3; i++) {
		spdk_set_thread(thread[i]);
		busy = spdk_poller_register(poller_run_busy, (void *)10, 0);
		idle = spdk_poller_register(poller_run_idle, (void *)90, 0);
		reactor = spdk_reactor_get(i);
		CU_ASSERT(reactor != NULL);
		reactor->tsc_last = 100;
		_reactor_run(reactor);
		spdk_poller_unregister(&busy);
		spdk_poller_unregister(&idle);

		/* Update last stats so that we don't have to call scheduler twice */
		lw_thread = spdk_thread_get_ctx(thread[i]);
		lw_thread->last_stats.busy_tsc = UINT32_MAX;
		lw_thread->last_stats.idle_tsc = UINT32_MAX;
	}

	reactor = spdk_reactor_get(0);
	CU_ASSERT(reactor != NULL);

	_reactors_scheduler_gather_metrics(NULL, NULL);

	/* Gather metrics for all cores */
	reactor = spdk_reactor_get(1);
	CU_ASSERT(reactor != NULL);
	MOCK_SET(spdk_env_get_current_core, 1);
	CU_ASSERT(event_queue_run_batch(reactor) == 1);
	reactor = spdk_reactor_get(2);
	CU_ASSERT(reactor != NULL);
	MOCK_SET(spdk_env_get_current_core, 2);
	CU_ASSERT(event_queue_run_batch(reactor) == 1);
	reactor = spdk_reactor_get(0);
	CU_ASSERT(reactor != NULL);
	MOCK_SET(spdk_env_get_current_core, 0);
	CU_ASSERT(event_queue_run_batch(reactor) == 1);

	/* Threads were idle, so all of them should be placed on core 0 */
	for (i = 0; i < 3; i++) {
		reactor = spdk_reactor_get(i);
		CU_ASSERT(reactor != NULL);
		_reactor_run(reactor);
	}

	/* 2 threads should be scheduled to core 0 */
	reactor = spdk_reactor_get(0);
	CU_ASSERT(reactor != NULL);
	MOCK_SET(spdk_env_get_current_core, 0);
	CU_ASSERT(event_queue_run_batch(reactor) == 2);

	reactor = spdk_reactor_get(0);
	CU_ASSERT(reactor != NULL);
	CU_ASSERT(!TAILQ_EMPTY(&reactor->threads));
	reactor = spdk_reactor_get(1);
	CU_ASSERT(reactor != NULL);
	CU_ASSERT(TAILQ_EMPTY(&reactor->threads));
	reactor = spdk_reactor_get(2);
	CU_ASSERT(reactor != NULL);
	CU_ASSERT(TAILQ_EMPTY(&reactor->threads));

	/* Make threads busy */
	reactor = spdk_reactor_get(0);
	CU_ASSERT(reactor != NULL);
	reactor->tsc_last = 100;

	for (i = 0; i < 3; i++) {
		spdk_set_thread(thread[i]);
		busy = spdk_poller_register(poller_run_busy, (void *)100, 0);
		_reactor_run(reactor);
		spdk_poller_unregister(&busy);
	}

	reactor->busy_tsc = 0;
	reactor->idle_tsc = UINT32_MAX;

	/* Run scheduler again, this time all threads are busy */
	_reactors_scheduler_gather_metrics(NULL, NULL);

	/* Gather metrics for all cores */
	reactor = spdk_reactor_get(1);
	CU_ASSERT(reactor != NULL);
	MOCK_SET(spdk_env_get_current_core, 1);
	CU_ASSERT(event_queue_run_batch(reactor) == 1);
	reactor = spdk_reactor_get(2);
	CU_ASSERT(reactor != NULL);
	MOCK_SET(spdk_env_get_current_core, 2);
	CU_ASSERT(event_queue_run_batch(reactor) == 1);
	reactor = spdk_reactor_get(0);
	CU_ASSERT(reactor != NULL);
	MOCK_SET(spdk_env_get_current_core, 0);
	CU_ASSERT(event_queue_run_batch(reactor) == 1);

	/* Threads were busy, so they should be distributed evenly across cores */
	for (i = 0; i < 3; i++) {
		MOCK_SET(spdk_env_get_current_core, i);
		reactor = spdk_reactor_get(i);
		CU_ASSERT(reactor != NULL);
		_reactor_run(reactor);
	}

	for (i = 0; i < 3; i++) {
		reactor = spdk_reactor_get(i);
		CU_ASSERT(reactor != NULL);
		CU_ASSERT(!TAILQ_EMPTY(&reactor->threads));
	}

	g_reactor_state = SPDK_REACTOR_STATE_INITIALIZED;

	/* Destroy threads */
	for (i = 0; i < 3; i++) {
		reactor = spdk_reactor_get(i);
		CU_ASSERT(reactor != NULL);
		reactor_run(reactor);
	}

	spdk_set_thread(NULL);

	MOCK_CLEAR(spdk_env_get_current_core);

	spdk_reactors_fini();

	free_cores();
}

uint8_t g_curr_freq;

static int
core_freq_up(uint32_t lcore)
{
	if (g_curr_freq != UINT8_MAX) {
		g_curr_freq++;
	}

	return 0;
}

static int
core_freq_down(uint32_t lcore)
{
	if (g_curr_freq != 0) {
		g_curr_freq--;
	}

	return 0;
}

static int
core_freq_max(uint32_t lcore)
{
	g_curr_freq = UINT8_MAX;

	return 0;
}

static struct spdk_governor governor = {
	.name = "dpdk_governor",
	.get_core_freqs = NULL,
	.get_core_curr_freq = NULL,
	.set_core_freq = NULL,
	.core_freq_up = core_freq_up,
	.core_freq_down = core_freq_down,
	.set_core_freq_max = core_freq_max,
	.set_core_freq_min = NULL,
	.get_core_turbo_status = NULL,
	.enable_core_turbo = NULL,
	.disable_core_turbo = NULL,
	.get_core_capabilities = NULL,
	.init_core = NULL,
	.deinit_core = NULL,
	.init = NULL,
	.deinit = NULL,
};

static void
test_governor(void)
{
	struct spdk_cpuset cpuset = {};
	struct spdk_thread *thread[2];
	struct spdk_lw_thread *lw_thread;
	struct spdk_reactor *reactor;
	struct spdk_poller *busy, *idle;
	uint8_t last_freq = 100;
	int i;

	MOCK_SET(spdk_env_get_current_core, 0);

	g_curr_freq = last_freq;
	_spdk_governor_list_add(&governor);

	allocate_cores(2);

	CU_ASSERT(spdk_reactors_init() == 0);

	_spdk_scheduler_set("dynamic");

	for (i = 0; i < 2; i++) {
		spdk_cpuset_set_cpu(&g_reactor_core_mask, i, true);
	}

	/* Create threads. */
	for (i = 0; i < 2; i++) {
		spdk_cpuset_set_cpu(&cpuset, i, true);
		thread[i] = spdk_thread_create(NULL, &cpuset);
		CU_ASSERT(thread[i] != NULL);
	}

	for (i = 0; i < 2; i++) {
		reactor = spdk_reactor_get(i);
		CU_ASSERT(reactor != NULL);
		MOCK_SET(spdk_env_get_current_core, i);
		CU_ASSERT(event_queue_run_batch(reactor) == 1);
		CU_ASSERT(!TAILQ_EMPTY(&reactor->threads));
	}

	reactor = spdk_reactor_get(0);
	CU_ASSERT(reactor != NULL);
	MOCK_SET(spdk_env_get_current_core, 0);

	g_reactor_state = SPDK_REACTOR_STATE_RUNNING;

	/* TEST 1 */
	/* Init thread stats (low load) */
	MOCK_SET(spdk_get_ticks, 100);
	reactor->tsc_last = 100;

	for (i = 0; i < 2; i++) {
		spdk_set_thread(thread[i]);
		idle = spdk_poller_register(poller_run_idle, (void *)200, 0);
		reactor = spdk_reactor_get(i);
		CU_ASSERT(reactor != NULL);
		MOCK_SET(spdk_env_get_current_core, i);
		_reactor_run(reactor);
		spdk_poller_unregister(&idle);

		/* Update last stats so that we don't have to call scheduler twice */
		lw_thread = spdk_thread_get_ctx(thread[i]);
		lw_thread->last_stats.idle_tsc = 1;
	}

	reactor = spdk_reactor_get(0);
	CU_ASSERT(reactor != NULL);
	MOCK_SET(spdk_env_get_current_core, 0);

	_reactors_scheduler_gather_metrics(NULL, NULL);

	/* Gather metrics for cores */
	reactor = spdk_reactor_get(1);
	CU_ASSERT(reactor != NULL);
	MOCK_SET(spdk_env_get_current_core, 1);
	CU_ASSERT(event_queue_run_batch(reactor) == 1);
	reactor = spdk_reactor_get(0);
	CU_ASSERT(reactor != NULL);
	MOCK_SET(spdk_env_get_current_core, 0);
	CU_ASSERT(event_queue_run_batch(reactor) == 1);

	/* Threads were idle, so all of them should be placed on core 0 */
	for (i = 0; i < 2; i++) {
		reactor = spdk_reactor_get(i);
		CU_ASSERT(reactor != NULL);
		_reactor_run(reactor);
	}

	/* 1 thread should be scheduled to core 0 */
	reactor = spdk_reactor_get(0);
	CU_ASSERT(reactor != NULL);
	MOCK_SET(spdk_env_get_current_core, 0);
	CU_ASSERT(event_queue_run_batch(reactor) == 1);

	/* Main core should be busy less than 50% time now - frequency should be lowered */
	CU_ASSERT(g_curr_freq == last_freq - 1);

	last_freq = g_curr_freq;

	/* TEST 2 */
	/* Make first threads busy - both threads will be still on core 0, but frequency will have to be raised */
	spdk_set_thread(thread[0]);
	busy = spdk_poller_register(poller_run_busy, (void *)1000, 0);
	_reactor_run(reactor);
	spdk_poller_unregister(&busy);

	spdk_set_thread(thread[1]);
	idle = spdk_poller_register(poller_run_idle, (void *)100, 0);
	_reactor_run(reactor);
	spdk_poller_unregister(&idle);

	/* Run scheduler again */
	_reactors_scheduler_gather_metrics(NULL, NULL);

	/* Gather metrics */
	reactor = spdk_reactor_get(1);
	CU_ASSERT(reactor != NULL);
	MOCK_SET(spdk_env_get_current_core, 1);
	CU_ASSERT(event_queue_run_batch(reactor) == 1);
	reactor = spdk_reactor_get(0);
	CU_ASSERT(reactor != NULL);
	MOCK_SET(spdk_env_get_current_core, 0);
	CU_ASSERT(event_queue_run_batch(reactor) == 1);

	/* Main core should be busy more than 50% time now - frequency should be raised */
	CU_ASSERT(g_curr_freq == last_freq + 1);

	/* TEST 3 */
	/* Make second thread very busy so that it will be moved to second core */
	spdk_set_thread(thread[1]);
	busy = spdk_poller_register(poller_run_busy, (void *)1000, 0);
	_reactor_run(reactor);
	spdk_poller_unregister(&busy);

	/* Update first thread stats */
	spdk_set_thread(thread[0]);
	idle = spdk_poller_register(poller_run_idle, (void *)100, 0);
	_reactor_run(reactor);
	spdk_poller_unregister(&idle);

	/* Run scheduler again */
	_reactors_scheduler_gather_metrics(NULL, NULL);

	/* Gather metrics */
	reactor = spdk_reactor_get(1);
	CU_ASSERT(reactor != NULL);
	MOCK_SET(spdk_env_get_current_core, 1);
	CU_ASSERT(event_queue_run_batch(reactor) == 1);
	reactor = spdk_reactor_get(0);
	CU_ASSERT(reactor != NULL);
	MOCK_SET(spdk_env_get_current_core, 0);
	CU_ASSERT(event_queue_run_batch(reactor) == 1);

	for (i = 0; i < 2; i++) {
		reactor = spdk_reactor_get(i);
		CU_ASSERT(reactor != NULL);
		_reactor_run(reactor);
	}

	/* Main core frequency should be set to max when we have busy threads on other cores */
	CU_ASSERT(g_curr_freq == UINT8_MAX);

	g_reactor_state = SPDK_REACTOR_STATE_INITIALIZED;

	/* Destroy threads */
	for (i = 0; i < 2; i++) {
		reactor = spdk_reactor_get(i);
		CU_ASSERT(reactor != NULL);
		reactor_run(reactor);
	}

	spdk_set_thread(NULL);

	MOCK_CLEAR(spdk_env_get_current_core);

	spdk_reactors_fini();

	free_cores();
}

int
main(int argc, char **argv)
{
	CU_pSuite suite = NULL;
	unsigned int num_failures;

	CU_set_error_action(CUEA_ABORT);
	CU_initialize_registry();

	suite = CU_add_suite("app_suite", NULL, NULL);

	CU_ADD_TEST(suite, test_create_reactor);
	CU_ADD_TEST(suite, test_init_reactors);
	CU_ADD_TEST(suite, test_event_call);
	CU_ADD_TEST(suite, test_schedule_thread);
	CU_ADD_TEST(suite, test_reschedule_thread);
	CU_ADD_TEST(suite, test_for_each_reactor);
	CU_ADD_TEST(suite, test_reactor_stats);
	CU_ADD_TEST(suite, test_scheduler);
	CU_ADD_TEST(suite, test_governor);

	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	num_failures = CU_get_number_of_failures();
	CU_cleanup_registry();

	return num_failures;
}
