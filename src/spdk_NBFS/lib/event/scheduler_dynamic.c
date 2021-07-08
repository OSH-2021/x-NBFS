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
#include "spdk/likely.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/env.h"

#include "spdk/thread.h"
#include "spdk_internal/event.h"

static uint32_t g_next_lcore = SPDK_ENV_LCORE_ID_ANY;
static uint32_t g_main_lcore;
static bool g_core_mngmnt_available;
uint64_t g_last_main_core_busy, g_last_main_core_idle;

#define SCHEDULER_THREAD_BUSY 100
#define SCHEDULER_LOAD_LIMIT 50

static uint32_t
_get_next_target_core(void)
{
	uint32_t target_lcore;

	if (g_next_lcore == SPDK_ENV_LCORE_ID_ANY) {
		g_next_lcore = spdk_env_get_first_core();
	}

	target_lcore = g_next_lcore;
	g_next_lcore = spdk_env_get_next_core(g_next_lcore);

	return target_lcore;
}

static uint8_t
_get_thread_load(struct spdk_lw_thread *lw_thread)
{
	uint64_t busy, idle;

	busy = lw_thread->snapshot_stats.busy_tsc - lw_thread->last_stats.busy_tsc;
	idle = lw_thread->snapshot_stats.idle_tsc - lw_thread->last_stats.idle_tsc;

	lw_thread->last_stats.busy_tsc = lw_thread->snapshot_stats.busy_tsc;
	lw_thread->last_stats.idle_tsc = lw_thread->snapshot_stats.idle_tsc;

	/* return percentage of time thread was busy */
	return busy  * 100 / (busy + idle);
}

static int
init(struct spdk_governor *governor)
{
	int rc;

	g_main_lcore = spdk_env_get_current_core();

	rc = _spdk_governor_set("dpdk_governor");
	g_core_mngmnt_available = !rc;

	g_last_main_core_busy = 0;
	g_last_main_core_idle = 0;

	return 0;
}

static int
deinit(struct spdk_governor *governor)
{
	uint32_t i;
	int rc = 0;

	if (!g_core_mngmnt_available) {
		return 0;
	}

	if (governor->deinit_core) {
		SPDK_ENV_FOREACH_CORE(i) {
			rc = governor->deinit_core(i);
			if (rc != 0) {
				SPDK_ERRLOG("Failed to deinitialize governor for core %d\n", i);
			}
		}
	}

	if (governor->deinit) {
		rc = governor->deinit();
	}

	return rc;
}

static void
balance(struct spdk_scheduler_core_info *cores_info, int cores_count,
	struct spdk_governor *governor)
{
	struct spdk_reactor *reactor;
	struct spdk_lw_thread *lw_thread;
	struct spdk_thread *thread;
	struct spdk_scheduler_core_info *core;
	struct spdk_cpuset *cpumask;
	uint64_t main_core_busy;
	uint64_t main_core_idle;
	uint64_t thread_busy;
	uint32_t target_lcore;
	uint32_t i, j, k;
	int rc;
	uint8_t load;
	bool busy_threads_present = false;

	main_core_busy = cores_info[g_main_lcore].core_busy_tsc - g_last_main_core_busy;
	main_core_idle = cores_info[g_main_lcore].core_idle_tsc - g_last_main_core_idle;
	g_last_main_core_busy = cores_info[g_main_lcore].core_busy_tsc;
	g_last_main_core_idle = cores_info[g_main_lcore].core_idle_tsc;

	SPDK_ENV_FOREACH_CORE(i) {
		cores_info[i].pending_threads_count = cores_info[i].threads_count;
	}

	/* Distribute active threads across all cores and move idle threads to main core */
	SPDK_ENV_FOREACH_CORE(i) {
		core = &cores_info[i];

		for (j = 0; j < core->threads_count; j++) {
			lw_thread = core->threads[j];
			lw_thread->new_lcore = lw_thread->lcore;
			thread = spdk_thread_get_from_ctx(lw_thread);
			cpumask = spdk_thread_get_cpumask(thread);

			if (lw_thread->last_stats.busy_tsc + lw_thread->last_stats.idle_tsc == 0) {
				lw_thread->last_stats.busy_tsc = lw_thread->snapshot_stats.busy_tsc;
				lw_thread->last_stats.idle_tsc = lw_thread->snapshot_stats.idle_tsc;

				if (i != g_main_lcore) {
					busy_threads_present = true;
				}

				continue;
			}

			thread_busy = lw_thread->snapshot_stats.busy_tsc - lw_thread->last_stats.busy_tsc;

			load = _get_thread_load(lw_thread);

			if (i == g_main_lcore && load >= SCHEDULER_LOAD_LIMIT) {
				/* This thread is active and on the main core, we need to pick a core to move it to */
				for (k = 0; k < spdk_env_get_core_count(); k++) {
					target_lcore = _get_next_target_core();

					/* Do not use main core if it is too busy for new thread */
					if (target_lcore == g_main_lcore && thread_busy > main_core_idle) {
						continue;
					}

					if (spdk_cpuset_get_cpu(cpumask, target_lcore)) {
						lw_thread->new_lcore = target_lcore;
						cores_info[target_lcore].pending_threads_count++;
						core->pending_threads_count--;

						if (target_lcore != g_main_lcore) {
							busy_threads_present = true;
							main_core_idle += spdk_min(UINT64_MAX - main_core_idle, thread_busy);
							main_core_busy -= spdk_min(main_core_busy, thread_busy);
						}

						break;
					}
				}
			} else if (i != g_main_lcore && load < SCHEDULER_LOAD_LIMIT) {
				/* This thread is idle but not on the main core, so we need to move it to the main core */
				lw_thread->new_lcore = g_main_lcore;
				cores_info[g_main_lcore].pending_threads_count++;
				core->pending_threads_count--;

				main_core_busy += spdk_min(UINT64_MAX - main_core_busy, thread_busy);
				main_core_idle -= spdk_min(main_core_idle, thread_busy);
			} else {
				/* Move busy thread only if cpumask does not match current core (except main core) */
				if (i != g_main_lcore) {
					if (!spdk_cpuset_get_cpu(cpumask, i)) {
						for (k = 0; k < spdk_env_get_core_count(); k++) {
							target_lcore = _get_next_target_core();

							if (spdk_cpuset_get_cpu(cpumask, target_lcore)) {
								lw_thread->new_lcore = target_lcore;
								cores_info[target_lcore].pending_threads_count++;
								core->pending_threads_count--;

								if (target_lcore == g_main_lcore) {
									main_core_busy += spdk_min(UINT64_MAX - main_core_busy, thread_busy);
									main_core_idle -= spdk_min(main_core_idle, thread_busy);
								}
								break;
							}
						}
					}

					busy_threads_present = true;
				}
			}
		}
	}

	/* Switch unused cores to interrupt mode and switch cores to polled mode
	 * if they will be used after rebalancing */
	SPDK_ENV_FOREACH_CORE(i) {
		reactor = spdk_reactor_get(i);
		core = &cores_info[i];
		/* We can switch mode only if reactor already does not have any threads */
		if (core->pending_threads_count == 0 && TAILQ_EMPTY(&reactor->threads)) {
			core->interrupt_mode = true;
		} else if (core->pending_threads_count != 0) {
			core->interrupt_mode = false;
		}
	}

	if (!g_core_mngmnt_available) {
		return;
	}

	/* Change main core frequency if needed */
	if (busy_threads_present) {
		rc = governor->set_core_freq_max(g_main_lcore);
		if (rc < 0) {
			SPDK_ERRLOG("setting default frequency for core %u failed\n", g_main_lcore);
		}
	} else if (main_core_busy > main_core_idle) {
		rc = governor->core_freq_up(g_main_lcore);
		if (rc < 0) {
			SPDK_ERRLOG("increasing frequency for core %u failed\n", g_main_lcore);
		}
	} else {
		rc = governor->core_freq_down(g_main_lcore);
		if (rc < 0) {
			SPDK_ERRLOG("lowering frequency for core %u failed\n", g_main_lcore);
		}
	}
}

static struct spdk_scheduler scheduler_dynamic = {
	.name = "dynamic",
	.init = init,
	.deinit = deinit,
	.balance = balance,
};

SPDK_SCHEDULER_REGISTER(scheduler_dynamic);
