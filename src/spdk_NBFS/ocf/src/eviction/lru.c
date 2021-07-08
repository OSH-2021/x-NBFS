/*
 * Copyright(c) 2012-2020 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "eviction.h"
#include "lru.h"
#include "ops.h"
#include "../utils/utils_cleaner.h"
#include "../utils/utils_cache_line.h"
#include "../concurrency/ocf_concurrency.h"
#include "../mngt/ocf_mngt_common.h"
#include "../engine/engine_zero.h"
#include "../ocf_request.h"

#define OCF_EVICTION_MAX_SCAN 1024

static const ocf_cache_line_t end_marker = (ocf_cache_line_t)-1;

/* Adds the given collision_index to the _head_ of the LRU list */
static void add_lru_head(ocf_cache_t cache,
		struct ocf_lru_list *list,
		unsigned int collision_index)

{
	struct lru_eviction_policy_meta *node;
	unsigned int curr_head_index;

	ENV_BUG_ON(collision_index == end_marker);

	node = &ocf_metadata_get_eviction_policy(cache, collision_index)->lru;

	/* First node to be added/ */
	if (!list->num_nodes)  {
		list->head = collision_index;
		list->tail = collision_index;

		node->next = end_marker;
		node->prev = end_marker;

		list->num_nodes = 1;
	} else {
		struct lru_eviction_policy_meta *curr_head;

		/* Not the first node to be added. */
		curr_head_index = list->head;

		ENV_BUG_ON(curr_head_index == end_marker);

		curr_head = &ocf_metadata_get_eviction_policy(cache,
				curr_head_index)->lru;

		node->next = curr_head_index;
		node->prev = end_marker;
		curr_head->prev = collision_index;

		list->head = collision_index;

		++list->num_nodes;
	}
}

/* Deletes the node with the given collision_index from the lru list */
static void remove_lru_list(ocf_cache_t cache,
		struct ocf_lru_list *list,
		unsigned int collision_index)
{
	int is_head = 0, is_tail = 0;
	uint32_t prev_lru_node, next_lru_node;
	struct lru_eviction_policy_meta *node;

	ENV_BUG_ON(collision_index == end_marker);

	node = &ocf_metadata_get_eviction_policy(cache, collision_index)->lru;

	is_head = (list->head == collision_index);
	is_tail = (list->tail == collision_index);

	/* Set prev and next (even if not existent) */
	next_lru_node = node->next;
	prev_lru_node = node->prev;

	/* Case 1: If we are head AND tail, there is only one node.
	 * So unlink node and set that there is no node left in the list.
	 */
	if (is_head && is_tail) {
		node->next = end_marker;
		node->prev = end_marker;

		list->head = end_marker;
		list->tail = end_marker;
	}

	/* Case 2: else if this collision_index is LRU head, but not tail,
	 * update head and return
	 */
	else if (is_head) {
		struct lru_eviction_policy_meta *next_node;

		ENV_BUG_ON(next_lru_node == end_marker);

		next_node = &ocf_metadata_get_eviction_policy(cache,
				next_lru_node)->lru;

		list->head = next_lru_node;
		node->next = end_marker;
		next_node->prev = end_marker;
	}

	/* Case 3: else if this collision_index is LRU tail, but not head,
	 * update tail and return
	 */
	else if (is_tail) {
		struct lru_eviction_policy_meta *prev_node;

		ENV_BUG_ON(prev_lru_node == end_marker);

		list->tail = prev_lru_node;

		prev_node = &ocf_metadata_get_eviction_policy(cache,
				prev_lru_node)->lru;

		node->prev = end_marker;
		prev_node->next = end_marker;
	}

	/* Case 4: else this collision_index is a middle node. There is no
	 * change to the head and the tail pointers.
	 */
	else {
		struct lru_eviction_policy_meta *prev_node;
		struct lru_eviction_policy_meta *next_node;

		ENV_BUG_ON(next_lru_node == end_marker);
		ENV_BUG_ON(prev_lru_node == end_marker);

		next_node = &ocf_metadata_get_eviction_policy(cache,
				next_lru_node)->lru;
		prev_node = &ocf_metadata_get_eviction_policy(cache,
				prev_lru_node)->lru;

		/* Update prev and next nodes */
		prev_node->next = node->next;
		next_node->prev = node->prev;

		/* Update the given node */
		node->next = end_marker;
		node->prev = end_marker;
	}

	--list->num_nodes;
}

/*-- End of LRU functions*/

void evp_lru_init_cline(ocf_cache_t cache, ocf_cache_line_t cline)
{
	struct lru_eviction_policy_meta *node;

	node = &ocf_metadata_get_eviction_policy(cache, cline)->lru;

	node->prev = end_marker;
	node->next = end_marker;
}

static struct ocf_lru_list *evp_lru_get_list(struct ocf_user_part *part,
		uint32_t evp, bool clean)
{
	return clean ? &part->runtime->eviction[evp].policy.lru.clean :
			&part->runtime->eviction[evp].policy.lru.dirty;
}

static inline struct ocf_lru_list *evp_get_cline_list(ocf_cache_t cache,
		ocf_cache_line_t cline)
{
	ocf_part_id_t part_id = ocf_metadata_get_partition_id(cache, cline);
	struct ocf_user_part *part = &cache->user_parts[part_id];
	uint32_t ev_list = (cline % OCF_NUM_EVICTION_LISTS);

	return evp_lru_get_list(part, ev_list,
			!metadata_test_dirty(cache, cline));
}

/* the caller must hold the metadata lock */
void evp_lru_rm_cline(ocf_cache_t cache, ocf_cache_line_t cline)
{
	struct ocf_lru_list *list;

	list = evp_get_cline_list(cache, cline);
	remove_lru_list(cache, list, cline);
}

static inline void lru_iter_init(struct ocf_lru_iter *iter, ocf_cache_t cache,
		struct ocf_user_part *part, uint32_t start_evp, bool clean)
{
	uint32_t i;

	/* entire iterator implementation depends on gcc builtins for
	   bit operations which works on 64 bit integers at most */
	ENV_BUILD_BUG_ON(OCF_NUM_EVICTION_LISTS > sizeof(iter->evp) * 8);

	iter->cache = cache;
	iter->part = part;
	/* set iterator value to start_evp - 1 modulo OCF_NUM_EVICTION_LISTS */
	iter->evp = (start_evp + OCF_NUM_EVICTION_LISTS - 1) % OCF_NUM_EVICTION_LISTS;
	iter->num_avail_evps = OCF_NUM_EVICTION_LISTS;
	iter->next_avail_evp = ((1ULL << OCF_NUM_EVICTION_LISTS) - 1);

	for (i = 0; i < OCF_NUM_EVICTION_LISTS; i++)
		iter->curr_cline[i] = evp_lru_get_list(part, i, clean)->tail;
}

static inline uint32_t _lru_next_evp(struct ocf_lru_iter *iter)
{
	unsigned increment;

	increment = __builtin_ffsll(iter->next_avail_evp);
	iter->next_avail_evp = ocf_rotate_right(iter->next_avail_evp,
			increment, OCF_NUM_EVICTION_LISTS);
	iter->evp = (iter->evp + increment) % OCF_NUM_EVICTION_LISTS;

	return iter->evp;
}

static inline bool _lru_evp_is_empty(struct ocf_lru_iter *iter)
{
	return !(iter->next_avail_evp & (1ULL << (OCF_NUM_EVICTION_LISTS - 1)));
}

static inline void _lru_evp_set_empty(struct ocf_lru_iter *iter)
{
	iter->next_avail_evp &= ~(1ULL << (OCF_NUM_EVICTION_LISTS - 1));
	iter->num_avail_evps--;
}

static inline bool _lru_evp_all_empty(struct ocf_lru_iter *iter)
{
	return iter->num_avail_evps == 0;
}

/* get next non-empty lru list if available */
static inline ocf_cache_line_t lru_iter_next(struct ocf_lru_iter *iter)
{
	struct lru_eviction_policy_meta *node;
	uint32_t curr_evp;
	ocf_cache_line_t  ret;

	curr_evp = _lru_next_evp(iter);

	while (iter->curr_cline[curr_evp] == end_marker) {
		if (!_lru_evp_is_empty(iter)) {
			/* mark list as empty */
			_lru_evp_set_empty(iter);
		}
		if (_lru_evp_all_empty(iter)) {
			/* all lists empty */
			return end_marker;
		}
		curr_evp = _lru_next_evp(iter);
	}

	node = &ocf_metadata_get_eviction_policy(iter->cache,
			iter->curr_cline[curr_evp])->lru;
	ret = iter->curr_cline[curr_evp];
	iter->curr_cline[curr_evp] = node->prev;

	return ret;
}

static void evp_lru_clean_end(void *private_data, int error)
{
	struct ocf_lru_iter *iter = private_data;

	ocf_refcnt_dec(&iter->part->cleaning);
}

static int evp_lru_clean_getter(ocf_cache_t cache, void *getter_context,
		uint32_t item, ocf_cache_line_t *line)
{
	struct ocf_lru_iter *iter = getter_context;
	ocf_cache_line_t cline;

	while (true) {
		cline = lru_iter_next(iter);

		if (cline == end_marker)
			break;

		/* Prevent evicting already locked items */
		if (ocf_cache_line_is_used(cache, cline)) {
			continue;
		}

		ENV_BUG_ON(!metadata_test_dirty(cache, cline));

		*line = cline;
		return 0;
	}

	return -1;
}

static void evp_lru_clean(ocf_cache_t cache, ocf_queue_t io_queue,
		struct ocf_user_part *part, uint32_t count)
{
	struct ocf_refcnt *counter = &part->cleaning;
	struct ocf_cleaner_attribs attribs = {
		.cache_line_lock = true,
		.do_sort = true,

		.cmpl_context = &part->eviction_clean_iter,
		.cmpl_fn = evp_lru_clean_end,

		.getter = evp_lru_clean_getter,
		.getter_context = &part->eviction_clean_iter,

		.count = count > 32 ? 32 : count,

		.io_queue = io_queue
	};
	int cnt;

	if (ocf_mngt_cache_is_locked(cache))
		return;

	cnt = ocf_refcnt_inc(counter);
	if (!cnt) {
		/* cleaner disabled by management operation */
		return;
	}
	if (cnt > 1) {
		/* cleaning already running for this partition */
		ocf_refcnt_dec(counter);
		return;
	}

	lru_iter_init(&part->eviction_clean_iter, cache, part,
			part->eviction_clean_iter.evp, false);

	ocf_cleaner_fire(cache, &attribs);
}

static void evp_lru_zero_line_complete(struct ocf_request *ocf_req, int error)
{
	env_atomic_dec(&ocf_req->cache->pending_eviction_clines);
}

static void evp_lru_zero_line(ocf_cache_t cache, ocf_queue_t io_queue,
		ocf_cache_line_t line)
{
	struct ocf_request *req;
	ocf_core_id_t id;
	uint64_t addr, core_line;

	ocf_metadata_get_core_info(cache, line, &id, &core_line);
	addr = core_line * ocf_line_size(cache);

	req = ocf_req_new(io_queue, &cache->core[id], addr,
			ocf_line_size(cache), OCF_WRITE);
	if (!req)
		return;

	if (req->d2c) {
		/* cache device is being detached */
		ocf_req_put(req);
		return;
	}

	req->info.internal = true;
	req->complete = evp_lru_zero_line_complete;

	env_atomic_inc(&cache->pending_eviction_clines);

	ocf_engine_zero_line(req);
}

bool evp_lru_can_evict(ocf_cache_t cache)
{
	if (env_atomic_read(&cache->pending_eviction_clines) >=
			OCF_PENDING_EVICTION_LIMIT) {
		return false;
	}

	return true;
}

static bool dirty_pages_present(ocf_cache_t cache, struct ocf_user_part *part)
{
	uint32_t i;

	for (i = 0; i < OCF_NUM_EVICTION_LISTS; i++) {
		if (evp_lru_get_list(part, i, false)->tail != end_marker)
			return true;
	}

	return false;
}

/* the caller must hold the metadata lock */
uint32_t evp_lru_req_clines(ocf_cache_t cache, ocf_queue_t io_queue,
		struct ocf_user_part *part, uint32_t cline_no)
{
	struct ocf_lru_iter iter;
	uint32_t i;
	ocf_cache_line_t cline;

	if (cline_no == 0)
		return 0;

	lru_iter_init(&iter, cache, part, part->next_eviction_list, true);

	i = 0;
	while (i < cline_no) {
		cline = lru_iter_next(&iter);

		if (cline == end_marker)
			break;

		if (!evp_lru_can_evict(cache))
			break;

		/* Prevent evicting already locked items */
		if (ocf_cache_line_is_used(cache, cline))
			continue;

		ENV_BUG_ON(metadata_test_dirty(cache, cline));

		if (ocf_volume_is_atomic(&cache->device->volume)) {
			/* atomic cache, we have to trim cache lines before
			 * eviction
			 */
			evp_lru_zero_line(cache, io_queue, cline);
			continue;
		}

		ocf_metadata_start_collision_shared_access(
				cache, cline);
		set_cache_line_invalid_no_flush(cache, 0,
				ocf_line_end_sector(cache),
				cline);
		ocf_metadata_end_collision_shared_access(
				cache, cline);
		++i;
	}

	part->next_eviction_list = iter.evp;

	if (i < cline_no && dirty_pages_present(cache, part))
		evp_lru_clean(cache, io_queue, part, cline_no - i);

	/* Return number of clines that were really evicted */
	return i;
}

/* the caller must hold the metadata lock */
void evp_lru_hot_cline(ocf_cache_t cache, ocf_cache_line_t cline)
{
	struct lru_eviction_policy_meta *node;
	struct ocf_lru_list *list;

	node = &ocf_metadata_get_eviction_policy(cache, cline)->lru;
	list = evp_get_cline_list(cache, cline);

	if (node->next != end_marker ||
			node->prev != end_marker ||
			list->head == cline || list->tail == cline) {
		remove_lru_list(cache, list, cline);
	}

	/* Update LRU */
	add_lru_head(cache, list, cline);
}

static inline void _lru_init(struct ocf_lru_list *list)
{
	list->num_nodes = 0;
	list->head = end_marker;
	list->tail = end_marker;
}

void evp_lru_init_evp(ocf_cache_t cache, struct ocf_user_part *part)
{
	struct ocf_lru_list *clean_list;
	struct ocf_lru_list *dirty_list;
	uint32_t i;

	for (i = 0; i < OCF_NUM_EVICTION_LISTS; i++) {
		clean_list = evp_lru_get_list(part, i, true);
		dirty_list = evp_lru_get_list(part, i, false);

		_lru_init(clean_list);
		_lru_init(dirty_list);
	}
}

void evp_lru_clean_cline(ocf_cache_t cache, struct ocf_user_part *part,
		uint32_t cline)
{
	uint32_t ev_list = (cline % OCF_NUM_EVICTION_LISTS);
	struct ocf_lru_list *clean_list;
	struct ocf_lru_list *dirty_list;

	clean_list = evp_lru_get_list(part, ev_list, true);
	dirty_list = evp_lru_get_list(part, ev_list, false);

	OCF_METADATA_EVICTION_LOCK(cline);
	remove_lru_list(cache, dirty_list, cline);
	add_lru_head(cache, clean_list, cline);
	OCF_METADATA_EVICTION_UNLOCK(cline);
}

void evp_lru_dirty_cline(ocf_cache_t cache, struct ocf_user_part *part,
		uint32_t cline)
{
	uint32_t ev_list = (cline % OCF_NUM_EVICTION_LISTS);
	struct ocf_lru_list *clean_list;
	struct ocf_lru_list *dirty_list;

	clean_list = evp_lru_get_list(part, ev_list, true);
	dirty_list = evp_lru_get_list(part, ev_list, false);

	OCF_METADATA_EVICTION_LOCK(cline);
	remove_lru_list(cache, clean_list, cline);
	add_lru_head(cache, dirty_list, cline);
	OCF_METADATA_EVICTION_UNLOCK(cline);
}

