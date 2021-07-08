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
#include "spdk/thread.h"
#include "spdk/bdev_module.h"
#include "spdk/bdev_module.h"

#include "common/lib/ut_multithread.c"

#include "bdev/nvme/bdev_nvme.c"
#include "bdev/nvme/common.c"

#include "unit/lib/json_mock.c"

static void *g_accel_p = (void *)0xdeadbeaf;

DEFINE_STUB(spdk_nvme_probe_async, struct spdk_nvme_probe_ctx *,
	    (const struct spdk_nvme_transport_id *trid, void *cb_ctx,
	     spdk_nvme_probe_cb probe_cb, spdk_nvme_attach_cb attach_cb,
	     spdk_nvme_remove_cb remove_cb), NULL);

DEFINE_STUB_V(spdk_nvme_trid_populate_transport, (struct spdk_nvme_transport_id *trid,
		enum spdk_nvme_transport_type trtype));

DEFINE_STUB(spdk_nvme_transport_id_trtype_str, const char *, (enum spdk_nvme_transport_type trtype),
	    NULL);

DEFINE_STUB(spdk_nvme_transport_id_adrfam_str, const char *, (enum spdk_nvmf_adrfam adrfam), NULL);

DEFINE_STUB_V(spdk_nvme_ctrlr_get_default_ctrlr_opts, (struct spdk_nvme_ctrlr_opts *opts,
		size_t opts_size));

DEFINE_STUB(spdk_nvme_ctrlr_set_trid, int, (struct spdk_nvme_ctrlr *ctrlr,
		struct spdk_nvme_transport_id *trid), 0);

DEFINE_STUB_V(spdk_nvme_ctrlr_set_remove_cb, (struct spdk_nvme_ctrlr *ctrlr,
		spdk_nvme_remove_cb remove_cb, void *remove_ctx));

DEFINE_STUB(spdk_nvme_ctrlr_get_flags, uint64_t, (struct spdk_nvme_ctrlr *ctrlr), 0);

DEFINE_STUB(accel_engine_create_cb, int, (void *io_device, void *ctx_buf), 0);
DEFINE_STUB_V(accel_engine_destroy_cb, (void *io_device, void *ctx_buf));

struct spdk_io_channel *
spdk_accel_engine_get_io_channel(void)
{
	return spdk_get_io_channel(g_accel_p);
}

void
spdk_nvme_ctrlr_get_default_io_qpair_opts(struct spdk_nvme_ctrlr *ctrlr,
		struct spdk_nvme_io_qpair_opts *opts, size_t opts_size)
{
	/* Avoid warning that opts is used uninitialised */
	memset(opts, 0, opts_size);
}

DEFINE_STUB(spdk_nvme_ctrlr_get_max_xfer_size, uint32_t,
	    (const struct spdk_nvme_ctrlr *ctrlr), 0);

DEFINE_STUB(spdk_nvme_ctrlr_get_transport_id, const struct spdk_nvme_transport_id *,
	    (struct spdk_nvme_ctrlr *ctrlr), NULL);

DEFINE_STUB_V(spdk_nvme_ctrlr_register_aer_callback, (struct spdk_nvme_ctrlr *ctrlr,
		spdk_nvme_aer_cb aer_cb_fn, void *aer_cb_arg));

DEFINE_STUB_V(spdk_nvme_ctrlr_register_timeout_callback, (struct spdk_nvme_ctrlr *ctrlr,
		uint64_t timeout_us, spdk_nvme_timeout_cb cb_fn, void *cb_arg));

DEFINE_STUB(spdk_nvme_ctrlr_is_ocssd_supported, bool, (struct spdk_nvme_ctrlr *ctrlr), false);

DEFINE_STUB(spdk_nvme_ctrlr_cmd_abort, int, (struct spdk_nvme_ctrlr *ctrlr,
		struct spdk_nvme_qpair *qpair, uint16_t cid, spdk_nvme_cmd_cb cb_fn, void *cb_arg), 0);

DEFINE_STUB(spdk_nvme_ctrlr_cmd_io_raw, int, (struct spdk_nvme_ctrlr *ctrlr,
		struct spdk_nvme_qpair *qpair, struct spdk_nvme_cmd *cmd, void *buf,
		uint32_t len, spdk_nvme_cmd_cb cb_fn, void *cb_arg), 0);

DEFINE_STUB(spdk_nvme_ctrlr_cmd_io_raw_with_md, int, (struct spdk_nvme_ctrlr *ctrlr,
		struct spdk_nvme_qpair *qpair, struct spdk_nvme_cmd *cmd, void *buf,
		uint32_t len, void *md_buf, spdk_nvme_cmd_cb cb_fn, void *cb_arg), 0);

DEFINE_STUB(spdk_nvme_ns_get_max_io_xfer_size, uint32_t, (struct spdk_nvme_ns *ns), 0);

DEFINE_STUB(spdk_nvme_ns_get_extended_sector_size, uint32_t, (struct spdk_nvme_ns *ns), 0);

DEFINE_STUB(spdk_nvme_ns_get_pi_type, enum spdk_nvme_pi_type, (struct spdk_nvme_ns *ns), 0);

DEFINE_STUB(spdk_nvme_ns_supports_compare, bool, (struct spdk_nvme_ns *ns), false);

DEFINE_STUB(spdk_nvme_ns_get_md_size, uint32_t, (struct spdk_nvme_ns *ns), 0);

DEFINE_STUB(spdk_nvme_ns_get_dealloc_logical_block_read_value,
	    enum spdk_nvme_dealloc_logical_block_read_value, (struct spdk_nvme_ns *ns), 0);

DEFINE_STUB(spdk_nvme_ns_get_optimal_io_boundary, uint32_t, (struct spdk_nvme_ns *ns), 0);

DEFINE_STUB(spdk_nvme_ns_get_uuid, const struct spdk_uuid *,
	    (const struct spdk_nvme_ns *ns), NULL);

DEFINE_STUB(spdk_nvme_ns_get_csi, enum spdk_nvme_csi,
	    (const struct spdk_nvme_ns *ns), 0);

DEFINE_STUB(spdk_nvme_cuse_get_ns_name, int, (struct spdk_nvme_ctrlr *ctrlr, uint32_t nsid,
		char *name, size_t *size), 0);

DEFINE_STUB(spdk_nvme_zns_ns_get_zone_size_sectors, uint64_t,
	    (struct spdk_nvme_ns *ns), 0);

DEFINE_STUB(spdk_nvme_zns_ctrlr_get_max_zone_append_size, uint32_t,
	    (const struct spdk_nvme_ctrlr *ctrlr), 0);

DEFINE_STUB(spdk_nvme_zns_ns_get_max_open_zones, uint32_t,
	    (struct spdk_nvme_ns *ns), 0);

DEFINE_STUB(spdk_nvme_zns_ns_get_max_active_zones, uint32_t,
	    (struct spdk_nvme_ns *ns), 0);

DEFINE_STUB(spdk_nvme_zns_ns_get_num_zones, uint64_t,
	    (struct spdk_nvme_ns *ns), 0);

DEFINE_STUB(spdk_nvme_zns_zone_append_with_md, int,
	    (struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair, void *buffer, void *metadata,
	     uint64_t zslba, uint32_t lba_count, spdk_nvme_cmd_cb cb_fn, void *cb_arg,
	     uint32_t io_flags, uint16_t apptag_mask, uint16_t apptag), 0);

DEFINE_STUB(spdk_nvme_zns_zone_appendv_with_md, int,
	    (struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair, uint64_t zslba,
	     uint32_t lba_count, spdk_nvme_cmd_cb cb_fn, void *cb_arg, uint32_t io_flags,
	     spdk_nvme_req_reset_sgl_cb reset_sgl_fn, spdk_nvme_req_next_sge_cb next_sge_fn,
	     void *metadata, uint16_t apptag_mask, uint16_t apptag), 0);

DEFINE_STUB(spdk_nvme_zns_report_zones, int,
	    (struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
	     void *payload, uint32_t payload_size, uint64_t slba,
	     enum spdk_nvme_zns_zra_report_opts report_opts, bool partial_report,
	     spdk_nvme_cmd_cb cb_fn, void *cb_arg), 0);

DEFINE_STUB(spdk_nvme_zns_close_zone, int,
	    (struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair, uint64_t slba,
	     bool select_all, spdk_nvme_cmd_cb cb_fn, void *cb_arg), 0);

DEFINE_STUB(spdk_nvme_zns_finish_zone, int,
	    (struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair, uint64_t slba,
	     bool select_all, spdk_nvme_cmd_cb cb_fn, void *cb_arg), 0);

DEFINE_STUB(spdk_nvme_zns_open_zone, int,
	    (struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair, uint64_t slba,
	     bool select_all, spdk_nvme_cmd_cb cb_fn, void *cb_arg), 0);

DEFINE_STUB(spdk_nvme_zns_reset_zone, int,
	    (struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair, uint64_t slba,
	     bool select_all, spdk_nvme_cmd_cb cb_fn, void *cb_arg), 0);

DEFINE_STUB(spdk_nvme_zns_offline_zone, int,
	    (struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair, uint64_t slba,
	     bool select_all, spdk_nvme_cmd_cb cb_fn, void *cb_arg), 0);

DEFINE_STUB_V(spdk_bdev_module_finish_done, (void));

DEFINE_STUB_V(spdk_bdev_module_list_add, (struct spdk_bdev_module *bdev_module));

DEFINE_STUB(spdk_opal_dev_construct, struct spdk_opal_dev *, (struct spdk_nvme_ctrlr *ctrlr), NULL);

DEFINE_STUB_V(spdk_opal_dev_destruct, (struct spdk_opal_dev *dev));

DEFINE_STUB_V(bdev_ocssd_populate_namespace, (struct nvme_bdev_ctrlr *nvme_bdev_ctrlr,
		struct nvme_bdev_ns *nvme_ns, struct nvme_async_probe_ctx *ctx));

DEFINE_STUB_V(bdev_ocssd_depopulate_namespace, (struct nvme_bdev_ns *nvme_ns));

DEFINE_STUB_V(bdev_ocssd_namespace_config_json, (struct spdk_json_write_ctx *w,
		struct nvme_bdev_ns *nvme_ns));

DEFINE_STUB(bdev_ocssd_create_io_channel, int, (struct nvme_io_channel *ioch), 0);

DEFINE_STUB_V(bdev_ocssd_destroy_io_channel, (struct nvme_io_channel *ioch));

DEFINE_STUB(bdev_ocssd_init_ctrlr, int, (struct nvme_bdev_ctrlr *nvme_bdev_ctrlr), 0);

DEFINE_STUB_V(bdev_ocssd_fini_ctrlr, (struct nvme_bdev_ctrlr *nvme_bdev_ctrlr));

DEFINE_STUB_V(bdev_ocssd_handle_chunk_notification, (struct nvme_bdev_ctrlr *nvme_bdev_ctrlr));

DEFINE_STUB(spdk_accel_submit_crc32cv, int, (struct spdk_io_channel *ch, uint32_t *dst,
		struct iovec *iov,
		uint32_t iov_cnt, uint32_t seed, spdk_accel_completion_cb cb_fn, void *cb_arg), 0);


struct ut_nvme_req {
	uint16_t			opc;
	spdk_nvme_cmd_cb		cb_fn;
	void				*cb_arg;
	struct spdk_nvme_cpl		cpl;
	TAILQ_ENTRY(ut_nvme_req)	tailq;
};

struct spdk_nvme_ns {
	struct spdk_nvme_ctrlr		*ctrlr;
	uint32_t			id;
	bool				is_active;
};

struct spdk_nvme_qpair {
	struct spdk_nvme_ctrlr		*ctrlr;
	bool				is_connected;
	TAILQ_HEAD(, ut_nvme_req)	outstanding_reqs;
	uint32_t			num_outstanding_reqs;
	TAILQ_ENTRY(spdk_nvme_qpair)	poll_group_tailq;
	struct spdk_nvme_poll_group	*poll_group;
	TAILQ_ENTRY(spdk_nvme_qpair)	tailq;
};

struct spdk_nvme_ctrlr {
	uint32_t			num_ns;
	struct spdk_nvme_ns		*ns;
	struct spdk_nvme_ns_data	*nsdata;
	struct spdk_nvme_qpair		adminq;
	struct spdk_nvme_ctrlr_data	cdata;
	bool				attached;
	bool				is_failed;
	bool				fail_reset;
	struct spdk_nvme_transport_id	trid;
	TAILQ_HEAD(, spdk_nvme_qpair)	active_io_qpairs;
	TAILQ_ENTRY(spdk_nvme_ctrlr)	tailq;
	struct spdk_nvme_ctrlr_opts	opts;
};

struct spdk_nvme_poll_group {
	void				*ctx;
	struct spdk_nvme_accel_fn_table	accel_fn_table;
	TAILQ_HEAD(, spdk_nvme_qpair)	qpairs;
};

struct spdk_nvme_probe_ctx {
	struct spdk_nvme_transport_id	trid;
	void				*cb_ctx;
	spdk_nvme_attach_cb		attach_cb;
	struct spdk_nvme_ctrlr		*init_ctrlr;
};

static TAILQ_HEAD(, spdk_nvme_ctrlr) g_ut_init_ctrlrs = TAILQ_HEAD_INITIALIZER(g_ut_init_ctrlrs);
static TAILQ_HEAD(, spdk_nvme_ctrlr) g_ut_attached_ctrlrs = TAILQ_HEAD_INITIALIZER(
			g_ut_attached_ctrlrs);
static int g_ut_attach_ctrlr_status;
static size_t g_ut_attach_bdev_count;
static int g_ut_register_bdev_status;

static void
ut_init_trid(struct spdk_nvme_transport_id *trid)
{
	trid->trtype = SPDK_NVME_TRANSPORT_TCP;
	snprintf(trid->subnqn, SPDK_NVMF_NQN_MAX_LEN, "%s", "nqn.2016-06.io.spdk:cnode1");
	snprintf(trid->traddr, SPDK_NVMF_TRADDR_MAX_LEN, "%s", "192.168.100.8");
	snprintf(trid->trsvcid, SPDK_NVMF_TRSVCID_MAX_LEN, "%s", "4420");
}

static void
ut_init_trid2(struct spdk_nvme_transport_id *trid)
{
	trid->trtype = SPDK_NVME_TRANSPORT_TCP;
	snprintf(trid->subnqn, SPDK_NVMF_NQN_MAX_LEN, "%s", "nqn.2016-06.io.spdk:cnode1");
	snprintf(trid->traddr, SPDK_NVMF_TRADDR_MAX_LEN, "%s", "192.168.100.9");
	snprintf(trid->trsvcid, SPDK_NVMF_TRSVCID_MAX_LEN, "%s", "4420");
}

static void
ut_init_trid3(struct spdk_nvme_transport_id *trid)
{
	trid->trtype = SPDK_NVME_TRANSPORT_TCP;
	snprintf(trid->subnqn, SPDK_NVMF_NQN_MAX_LEN, "%s", "nqn.2016-06.io.spdk:cnode1");
	snprintf(trid->traddr, SPDK_NVMF_TRADDR_MAX_LEN, "%s", "192.168.100.10");
	snprintf(trid->trsvcid, SPDK_NVMF_TRSVCID_MAX_LEN, "%s", "4420");
}

static int
cmp_int(int a, int b)
{
	return a - b;
}

int
spdk_nvme_transport_id_compare(const struct spdk_nvme_transport_id *trid1,
			       const struct spdk_nvme_transport_id *trid2)
{
	int cmp;

	/* We assume trtype is TCP for now. */
	CU_ASSERT(trid1->trtype == SPDK_NVME_TRANSPORT_TCP);

	cmp = cmp_int(trid1->trtype, trid2->trtype);
	if (cmp) {
		return cmp;
	}

	cmp = strcasecmp(trid1->traddr, trid2->traddr);
	if (cmp) {
		return cmp;
	}

	cmp = cmp_int(trid1->adrfam, trid2->adrfam);
	if (cmp) {
		return cmp;
	}

	cmp = strcasecmp(trid1->trsvcid, trid2->trsvcid);
	if (cmp) {
		return cmp;
	}

	cmp = strcmp(trid1->subnqn, trid2->subnqn);
	if (cmp) {
		return cmp;
	}

	return 0;
}

static struct spdk_nvme_ctrlr *
ut_attach_ctrlr(const struct spdk_nvme_transport_id *trid, uint32_t num_ns)
{
	struct spdk_nvme_ctrlr *ctrlr;
	uint32_t i;

	TAILQ_FOREACH(ctrlr, &g_ut_init_ctrlrs, tailq) {
		if (spdk_nvme_transport_id_compare(&ctrlr->trid, trid) == 0) {
			/* There is a ctrlr whose trid matches. */
			return NULL;
		}
	}

	ctrlr = calloc(1, sizeof(*ctrlr));
	if (ctrlr == NULL) {
		return NULL;
	}

	ctrlr->attached = true;
	ctrlr->adminq.ctrlr = ctrlr;
	TAILQ_INIT(&ctrlr->adminq.outstanding_reqs);

	if (num_ns != 0) {
		ctrlr->num_ns = num_ns;
		ctrlr->ns = calloc(num_ns, sizeof(struct spdk_nvme_ns));
		if (ctrlr->ns == NULL) {
			free(ctrlr);
			return NULL;
		}

		ctrlr->nsdata = calloc(num_ns, sizeof(struct spdk_nvme_ns_data));
		if (ctrlr->nsdata == NULL) {
			free(ctrlr->ns);
			free(ctrlr);
			return NULL;
		}

		for (i = 0; i < num_ns; i++) {
			ctrlr->ns[i].id = i + 1;
			ctrlr->ns[i].ctrlr = ctrlr;
			ctrlr->ns[i].is_active = true;
			ctrlr->nsdata[i].nsze = 1024;
		}
	}

	ctrlr->trid = *trid;
	TAILQ_INIT(&ctrlr->active_io_qpairs);

	TAILQ_INSERT_TAIL(&g_ut_init_ctrlrs, ctrlr, tailq);

	return ctrlr;
}

static void
ut_detach_ctrlr(struct spdk_nvme_ctrlr *ctrlr)
{
	CU_ASSERT(TAILQ_EMPTY(&ctrlr->active_io_qpairs));

	TAILQ_REMOVE(&g_ut_attached_ctrlrs, ctrlr, tailq);
	free(ctrlr->nsdata);
	free(ctrlr->ns);
	free(ctrlr);
}

static int
ut_submit_nvme_request(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
		       uint16_t opc, spdk_nvme_cmd_cb cb_fn, void *cb_arg)
{
	struct ut_nvme_req *req;

	req = calloc(1, sizeof(*req));
	if (req == NULL) {
		return -ENOMEM;
	}

	req->opc = opc;
	req->cb_fn = cb_fn;
	req->cb_arg = cb_arg;

	req->cpl.status.sc = SPDK_NVME_SC_SUCCESS;
	req->cpl.status.sct = SPDK_NVME_SCT_GENERIC;

	TAILQ_INSERT_TAIL(&qpair->outstanding_reqs, req, tailq);
	qpair->num_outstanding_reqs++;

	return 0;
}

static void
ut_bdev_io_set_buf(struct spdk_bdev_io *bdev_io)
{
	bdev_io->u.bdev.iovs = &bdev_io->iov;
	bdev_io->u.bdev.iovcnt = 1;

	bdev_io->iov.iov_base = (void *)0xFEEDBEEF;
	bdev_io->iov.iov_len = 4096;
}

static void
nvme_ctrlr_poll_internal(struct spdk_nvme_ctrlr *ctrlr, struct spdk_nvme_probe_ctx *probe_ctx)
{
	if (ctrlr->is_failed) {
		free(ctrlr);
		return;
	}

	TAILQ_INSERT_TAIL(&g_ut_attached_ctrlrs, ctrlr, tailq);

	if (probe_ctx->attach_cb) {
		probe_ctx->attach_cb(probe_ctx->cb_ctx, &ctrlr->trid, ctrlr, &ctrlr->opts);
	}
}

int
spdk_nvme_probe_poll_async(struct spdk_nvme_probe_ctx *probe_ctx)
{
	struct spdk_nvme_ctrlr *ctrlr, *tmp;

	TAILQ_FOREACH_SAFE(ctrlr, &g_ut_init_ctrlrs, tailq, tmp) {
		if (spdk_nvme_transport_id_compare(&ctrlr->trid, &probe_ctx->trid) != 0) {
			continue;
		}
		TAILQ_REMOVE(&g_ut_init_ctrlrs, ctrlr, tailq);
		nvme_ctrlr_poll_internal(ctrlr, probe_ctx);
	}

	free(probe_ctx);

	return 0;
}

struct spdk_nvme_probe_ctx *
spdk_nvme_connect_async(const struct spdk_nvme_transport_id *trid,
			const struct spdk_nvme_ctrlr_opts *opts,
			spdk_nvme_attach_cb attach_cb)
{
	struct spdk_nvme_probe_ctx *probe_ctx;

	if (trid == NULL) {
		return NULL;
	}

	probe_ctx = calloc(1, sizeof(*probe_ctx));
	if (probe_ctx == NULL) {
		return NULL;
	}

	probe_ctx->trid = *trid;
	probe_ctx->cb_ctx = (void *)opts;
	probe_ctx->attach_cb = attach_cb;

	return probe_ctx;
}

int
spdk_nvme_detach(struct spdk_nvme_ctrlr *ctrlr)
{
	if (ctrlr->attached) {
		ut_detach_ctrlr(ctrlr);
	}

	return 0;
}

const struct spdk_nvme_ctrlr_data *
spdk_nvme_ctrlr_get_data(struct spdk_nvme_ctrlr *ctrlr)
{
	return &ctrlr->cdata;
}

uint32_t
spdk_nvme_ctrlr_get_num_ns(struct spdk_nvme_ctrlr *ctrlr)
{
	return ctrlr->num_ns;
}

struct spdk_nvme_ns *
spdk_nvme_ctrlr_get_ns(struct spdk_nvme_ctrlr *ctrlr, uint32_t nsid)
{
	if (nsid < 1 || nsid > ctrlr->num_ns) {
		return NULL;
	}

	return &ctrlr->ns[nsid - 1];
}

bool
spdk_nvme_ctrlr_is_active_ns(struct spdk_nvme_ctrlr *ctrlr, uint32_t nsid)
{
	if (nsid < 1 || nsid > ctrlr->num_ns) {
		return false;
	}

	return ctrlr->ns[nsid - 1].is_active;
}

union spdk_nvme_csts_register
	spdk_nvme_ctrlr_get_regs_csts(struct spdk_nvme_ctrlr *ctrlr)
{
	union spdk_nvme_csts_register csts;

	csts.raw = 0;

	return csts;
}

union spdk_nvme_vs_register
	spdk_nvme_ctrlr_get_regs_vs(struct spdk_nvme_ctrlr *ctrlr)
{
	union spdk_nvme_vs_register vs;

	vs.raw = 0;

	return vs;
}

struct spdk_nvme_qpair *
spdk_nvme_ctrlr_alloc_io_qpair(struct spdk_nvme_ctrlr *ctrlr,
			       const struct spdk_nvme_io_qpair_opts *user_opts,
			       size_t opts_size)
{
	struct spdk_nvme_qpair *qpair;

	qpair = calloc(1, sizeof(*qpair));
	if (qpair == NULL) {
		return NULL;
	}

	qpair->ctrlr = ctrlr;
	TAILQ_INIT(&qpair->outstanding_reqs);
	TAILQ_INSERT_TAIL(&ctrlr->active_io_qpairs, qpair, tailq);

	return qpair;
}

int
spdk_nvme_ctrlr_connect_io_qpair(struct spdk_nvme_ctrlr *ctrlr,
				 struct spdk_nvme_qpair *qpair)
{
	if (qpair->is_connected) {
		return -EISCONN;
	}

	qpair->is_connected = true;

	return 0;
}

int
spdk_nvme_ctrlr_reconnect_io_qpair(struct spdk_nvme_qpair *qpair)
{
	struct spdk_nvme_ctrlr *ctrlr;

	ctrlr = qpair->ctrlr;

	if (ctrlr->is_failed) {
		return -ENXIO;
	}
	qpair->is_connected = true;

	return 0;
}

void
spdk_nvme_ctrlr_disconnect_io_qpair(struct spdk_nvme_qpair *qpair)
{
	qpair->is_connected = false;
}

int
spdk_nvme_ctrlr_free_io_qpair(struct spdk_nvme_qpair *qpair)
{
	SPDK_CU_ASSERT_FATAL(qpair->ctrlr != NULL);

	qpair->is_connected = false;

	if (qpair->poll_group != NULL) {
		spdk_nvme_poll_group_remove(qpair->poll_group, qpair);
	}

	TAILQ_REMOVE(&qpair->ctrlr->active_io_qpairs, qpair, tailq);

	CU_ASSERT(qpair->num_outstanding_reqs == 0);

	free(qpair);

	return 0;
}

int
spdk_nvme_ctrlr_reset(struct spdk_nvme_ctrlr *ctrlr)
{
	if (ctrlr->fail_reset) {
		return -EIO;
	}

	ctrlr->is_failed = false;

	return 0;
}

void
spdk_nvme_ctrlr_fail(struct spdk_nvme_ctrlr *ctrlr)
{
	ctrlr->is_failed = true;
}

int
spdk_nvme_ctrlr_cmd_admin_raw(struct spdk_nvme_ctrlr *ctrlr,
			      struct spdk_nvme_cmd *cmd, void *buf, uint32_t len,
			      spdk_nvme_cmd_cb cb_fn, void *cb_arg)
{
	return ut_submit_nvme_request(NULL, &ctrlr->adminq, cmd->opc, cb_fn, cb_arg);
}

int
spdk_nvme_ctrlr_cmd_abort_ext(struct spdk_nvme_ctrlr *ctrlr, struct spdk_nvme_qpair *qpair,
			      void *cmd_cb_arg,
			      spdk_nvme_cmd_cb cb_fn, void *cb_arg)
{
	struct ut_nvme_req *req = NULL, *abort_req;

	if (qpair == NULL) {
		qpair = &ctrlr->adminq;
	}

	abort_req = calloc(1, sizeof(*abort_req));
	if (abort_req == NULL) {
		return -ENOMEM;
	}

	TAILQ_FOREACH(req, &qpair->outstanding_reqs, tailq) {
		if (req->cb_arg == cmd_cb_arg) {
			break;
		}
	}

	if (req == NULL) {
		free(abort_req);
		return -ENOENT;
	}

	req->cpl.status.sc = SPDK_NVME_SC_ABORTED_BY_REQUEST;
	req->cpl.status.sct = SPDK_NVME_SCT_GENERIC;

	abort_req->opc = SPDK_NVME_OPC_ABORT;
	abort_req->cb_fn = cb_fn;
	abort_req->cb_arg = cb_arg;

	abort_req->cpl.status.sc = SPDK_NVME_SC_SUCCESS;
	abort_req->cpl.status.sct = SPDK_NVME_SCT_GENERIC;
	abort_req->cpl.cdw0 = 0;

	TAILQ_INSERT_TAIL(&ctrlr->adminq.outstanding_reqs, abort_req, tailq);
	ctrlr->adminq.num_outstanding_reqs++;

	return 0;
}

int32_t
spdk_nvme_ctrlr_process_admin_completions(struct spdk_nvme_ctrlr *ctrlr)
{
	return spdk_nvme_qpair_process_completions(&ctrlr->adminq, 0);
}

uint32_t
spdk_nvme_ns_get_id(struct spdk_nvme_ns *ns)
{
	return ns->id;
}

struct spdk_nvme_ctrlr *
spdk_nvme_ns_get_ctrlr(struct spdk_nvme_ns *ns)
{
	return ns->ctrlr;
}

static inline struct spdk_nvme_ns_data *
_nvme_ns_get_data(struct spdk_nvme_ns *ns)
{
	return &ns->ctrlr->nsdata[ns->id - 1];
}

const struct spdk_nvme_ns_data *
spdk_nvme_ns_get_data(struct spdk_nvme_ns *ns)
{
	return _nvme_ns_get_data(ns);
}

uint64_t
spdk_nvme_ns_get_num_sectors(struct spdk_nvme_ns *ns)
{
	return _nvme_ns_get_data(ns)->nsze;
}
int
spdk_nvme_ns_cmd_read_with_md(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair, void *buffer,
			      void *metadata, uint64_t lba, uint32_t lba_count,
			      spdk_nvme_cmd_cb cb_fn, void *cb_arg,
			      uint32_t io_flags, uint16_t apptag_mask, uint16_t apptag)
{
	return ut_submit_nvme_request(ns, qpair, SPDK_NVME_OPC_READ, cb_fn, cb_arg);
}

int
spdk_nvme_ns_cmd_write_with_md(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
			       void *buffer, void *metadata, uint64_t lba,
			       uint32_t lba_count, spdk_nvme_cmd_cb cb_fn, void *cb_arg,
			       uint32_t io_flags, uint16_t apptag_mask, uint16_t apptag)
{
	return ut_submit_nvme_request(ns, qpair, SPDK_NVME_OPC_WRITE, cb_fn, cb_arg);
}

int
spdk_nvme_ns_cmd_readv_with_md(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
			       uint64_t lba, uint32_t lba_count,
			       spdk_nvme_cmd_cb cb_fn, void *cb_arg, uint32_t io_flags,
			       spdk_nvme_req_reset_sgl_cb reset_sgl_fn,
			       spdk_nvme_req_next_sge_cb next_sge_fn, void *metadata,
			       uint16_t apptag_mask, uint16_t apptag)
{
	return ut_submit_nvme_request(ns, qpair, SPDK_NVME_OPC_READ, cb_fn, cb_arg);
}

int
spdk_nvme_ns_cmd_writev_with_md(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
				uint64_t lba, uint32_t lba_count,
				spdk_nvme_cmd_cb cb_fn, void *cb_arg, uint32_t io_flags,
				spdk_nvme_req_reset_sgl_cb reset_sgl_fn,
				spdk_nvme_req_next_sge_cb next_sge_fn, void *metadata,
				uint16_t apptag_mask, uint16_t apptag)
{
	return ut_submit_nvme_request(ns, qpair, SPDK_NVME_OPC_WRITE, cb_fn, cb_arg);
}

int
spdk_nvme_ns_cmd_comparev_with_md(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
				  uint64_t lba, uint32_t lba_count,
				  spdk_nvme_cmd_cb cb_fn, void *cb_arg, uint32_t io_flags,
				  spdk_nvme_req_reset_sgl_cb reset_sgl_fn,
				  spdk_nvme_req_next_sge_cb next_sge_fn,
				  void *metadata, uint16_t apptag_mask, uint16_t apptag)
{
	return ut_submit_nvme_request(ns, qpair, SPDK_NVME_OPC_COMPARE, cb_fn, cb_arg);
}

int
spdk_nvme_ns_cmd_dataset_management(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
				    uint32_t type, const struct spdk_nvme_dsm_range *ranges, uint16_t num_ranges,
				    spdk_nvme_cmd_cb cb_fn, void *cb_arg)
{
	return ut_submit_nvme_request(ns, qpair, SPDK_NVME_OPC_DATASET_MANAGEMENT, cb_fn, cb_arg);
}

struct spdk_nvme_poll_group *
spdk_nvme_poll_group_create(void *ctx, struct spdk_nvme_accel_fn_table *table)
{
	struct spdk_nvme_poll_group *group;

	group = calloc(1, sizeof(*group));
	if (group == NULL) {
		return NULL;
	}

	group->ctx = ctx;
	if (table != NULL) {
		group->accel_fn_table = *table;
	}
	TAILQ_INIT(&group->qpairs);

	return group;
}

int
spdk_nvme_poll_group_destroy(struct spdk_nvme_poll_group *group)
{
	if (!TAILQ_EMPTY(&group->qpairs)) {
		return -EBUSY;
	}

	free(group);

	return 0;
}

int32_t
spdk_nvme_qpair_process_completions(struct spdk_nvme_qpair *qpair,
				    uint32_t max_completions)
{
	struct ut_nvme_req *req, *tmp;
	uint32_t num_completions = 0;

	TAILQ_FOREACH_SAFE(req, &qpair->outstanding_reqs, tailq, tmp) {
		TAILQ_REMOVE(&qpair->outstanding_reqs, req, tailq);
		qpair->num_outstanding_reqs--;

		req->cb_fn(req->cb_arg, &req->cpl);

		free(req);
		num_completions++;
	}

	return num_completions;
}

int64_t
spdk_nvme_poll_group_process_completions(struct spdk_nvme_poll_group *group,
		uint32_t completions_per_qpair,
		spdk_nvme_disconnected_qpair_cb disconnected_qpair_cb)
{
	struct spdk_nvme_qpair *qpair, *tmp_qpair;
	int64_t local_completions = 0, error_reason = 0, num_completions = 0;

	SPDK_CU_ASSERT_FATAL(completions_per_qpair == 0);

	if (disconnected_qpair_cb == NULL) {
		return -EINVAL;
	}

	TAILQ_FOREACH_SAFE(qpair, &group->qpairs, poll_group_tailq, tmp_qpair) {
		if (qpair->is_connected) {
			local_completions = spdk_nvme_qpair_process_completions(qpair,
					    completions_per_qpair);
			if (local_completions < 0 && error_reason == 0) {
				error_reason = local_completions;
			} else {
				num_completions += local_completions;
				assert(num_completions >= 0);
			}
		}
	}

	TAILQ_FOREACH_SAFE(qpair, &group->qpairs, poll_group_tailq, tmp_qpair) {
		if (!qpair->is_connected) {
			disconnected_qpair_cb(qpair, group->ctx);
		}
	}

	return error_reason ? error_reason : num_completions;
}

int
spdk_nvme_poll_group_add(struct spdk_nvme_poll_group *group,
			 struct spdk_nvme_qpair *qpair)
{
	CU_ASSERT(!qpair->is_connected);

	qpair->poll_group = group;
	TAILQ_INSERT_TAIL(&group->qpairs, qpair, poll_group_tailq);

	return 0;
}

int
spdk_nvme_poll_group_remove(struct spdk_nvme_poll_group *group,
			    struct spdk_nvme_qpair *qpair)
{
	CU_ASSERT(!qpair->is_connected);

	TAILQ_REMOVE(&group->qpairs, qpair, poll_group_tailq);

	return 0;
}

int
spdk_bdev_register(struct spdk_bdev *bdev)
{
	return g_ut_register_bdev_status;
}

void
spdk_bdev_unregister(struct spdk_bdev *bdev, spdk_bdev_unregister_cb cb_fn, void *cb_arg)
{
	int rc;

	rc = bdev->fn_table->destruct(bdev->ctxt);
	if (rc <= 0 && cb_fn != NULL) {
		cb_fn(cb_arg, rc);
	}
}

int
spdk_bdev_notify_blockcnt_change(struct spdk_bdev *bdev, uint64_t size)
{
	bdev->blockcnt = size;

	return 0;
}

struct spdk_io_channel *
spdk_bdev_io_get_io_channel(struct spdk_bdev_io *bdev_io)
{
	return (struct spdk_io_channel *)bdev_io->internal.ch;
}

void
spdk_bdev_io_complete(struct spdk_bdev_io *bdev_io, enum spdk_bdev_io_status status)
{
	bdev_io->internal.status = status;
	bdev_io->internal.in_submit_request = false;
}

void
spdk_bdev_io_complete_nvme_status(struct spdk_bdev_io *bdev_io, uint32_t cdw0, int sct, int sc)
{
	if (sct == SPDK_NVME_SCT_GENERIC && sc == SPDK_NVME_SC_SUCCESS) {
		bdev_io->internal.status = SPDK_BDEV_IO_STATUS_SUCCESS;
	} else if (sct == SPDK_NVME_SCT_GENERIC && sc == SPDK_NVME_SC_ABORTED_BY_REQUEST) {
		bdev_io->internal.status = SPDK_BDEV_IO_STATUS_ABORTED;
	} else {
		bdev_io->internal.status = SPDK_BDEV_IO_STATUS_NVME_ERROR;
	}

	bdev_io->internal.error.nvme.cdw0 = cdw0;
	bdev_io->internal.error.nvme.sct = sct;
	bdev_io->internal.error.nvme.sc = sc;

	spdk_bdev_io_complete(bdev_io, bdev_io->internal.status);
}

void
spdk_bdev_io_get_buf(struct spdk_bdev_io *bdev_io, spdk_bdev_io_get_buf_cb cb, uint64_t len)
{
	struct spdk_io_channel *ch = spdk_bdev_io_get_io_channel(bdev_io);

	ut_bdev_io_set_buf(bdev_io);

	cb(ch, bdev_io, true);
}

static void
test_create_ctrlr(void)
{
	struct spdk_nvme_transport_id trid = {};
	struct spdk_nvme_ctrlr ctrlr = {};
	int rc;

	ut_init_trid(&trid);

	nvme_bdev_ctrlr_create(&ctrlr, "nvme0", &trid, 0, NULL);

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") != NULL);

	rc = bdev_nvme_delete("nvme0", NULL);
	CU_ASSERT(rc == 0);

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") != NULL);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == NULL);
}

static void
test_reset_ctrlr(void)
{
	struct spdk_nvme_transport_id trid = {};
	struct spdk_nvme_ctrlr ctrlr = {};
	struct nvme_bdev_ctrlr *nvme_bdev_ctrlr = NULL;
	struct nvme_bdev_ctrlr_trid *curr_trid;
	struct spdk_io_channel *ch1, *ch2;
	struct nvme_io_channel *nvme_ch1, *nvme_ch2;
	int rc;

	ut_init_trid(&trid);
	TAILQ_INIT(&ctrlr.active_io_qpairs);

	set_thread(0);

	nvme_bdev_ctrlr_create(&ctrlr, "nvme0", &trid, 0, NULL);

	nvme_bdev_ctrlr = nvme_bdev_ctrlr_get_by_name("nvme0");
	SPDK_CU_ASSERT_FATAL(nvme_bdev_ctrlr != NULL);

	curr_trid = TAILQ_FIRST(&nvme_bdev_ctrlr->trids);
	SPDK_CU_ASSERT_FATAL(curr_trid != NULL);

	ch1 = spdk_get_io_channel(nvme_bdev_ctrlr);
	SPDK_CU_ASSERT_FATAL(ch1 != NULL);

	nvme_ch1 = spdk_io_channel_get_ctx(ch1);
	CU_ASSERT(nvme_ch1->qpair != NULL);

	set_thread(1);

	ch2 = spdk_get_io_channel(nvme_bdev_ctrlr);
	SPDK_CU_ASSERT_FATAL(ch2 != NULL);

	nvme_ch2 = spdk_io_channel_get_ctx(ch2);
	CU_ASSERT(nvme_ch2->qpair != NULL);

	/* Reset starts from thread 1. */
	set_thread(1);

	/* Case 1: ctrlr is already being destructed. */
	nvme_bdev_ctrlr->destruct = true;

	rc = _bdev_nvme_reset(nvme_bdev_ctrlr);
	CU_ASSERT(rc == -EBUSY);

	/* Case 2: reset is in progress. */
	nvme_bdev_ctrlr->destruct = false;
	nvme_bdev_ctrlr->resetting = true;

	rc = _bdev_nvme_reset(nvme_bdev_ctrlr);
	CU_ASSERT(rc == -EAGAIN);

	/* Case 3: reset completes successfully. */
	nvme_bdev_ctrlr->resetting = false;
	curr_trid->is_failed = true;
	ctrlr.is_failed = true;

	rc = _bdev_nvme_reset(nvme_bdev_ctrlr);
	CU_ASSERT(rc == 0);
	CU_ASSERT(nvme_bdev_ctrlr->resetting == true);
	CU_ASSERT(nvme_ch1->qpair != NULL);
	CU_ASSERT(nvme_ch2->qpair != NULL);

	poll_thread_times(0, 1);
	CU_ASSERT(nvme_ch1->qpair == NULL);
	CU_ASSERT(nvme_ch2->qpair != NULL);

	poll_thread_times(1, 1);
	CU_ASSERT(nvme_ch1->qpair == NULL);
	CU_ASSERT(nvme_ch2->qpair == NULL);
	CU_ASSERT(ctrlr.is_failed == true);

	poll_thread_times(1, 1);
	CU_ASSERT(ctrlr.is_failed == false);

	poll_thread_times(0, 1);
	CU_ASSERT(nvme_ch1->qpair != NULL);
	CU_ASSERT(nvme_ch2->qpair == NULL);

	poll_thread_times(1, 1);
	CU_ASSERT(nvme_ch1->qpair != NULL);
	CU_ASSERT(nvme_ch2->qpair != NULL);
	CU_ASSERT(nvme_bdev_ctrlr->resetting == true);
	CU_ASSERT(curr_trid->is_failed == true);

	poll_thread_times(1, 1);
	CU_ASSERT(nvme_bdev_ctrlr->resetting == false);
	CU_ASSERT(curr_trid->is_failed == false);

	spdk_put_io_channel(ch2);

	set_thread(0);

	spdk_put_io_channel(ch1);

	poll_threads();

	rc = bdev_nvme_delete("nvme0", NULL);
	CU_ASSERT(rc == 0);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == NULL);
}

static void
test_race_between_reset_and_destruct_ctrlr(void)
{
	struct spdk_nvme_transport_id trid = {};
	struct spdk_nvme_ctrlr ctrlr = {};
	struct nvme_bdev_ctrlr *nvme_bdev_ctrlr;
	struct spdk_io_channel *ch1, *ch2;
	int rc;

	ut_init_trid(&trid);
	TAILQ_INIT(&ctrlr.active_io_qpairs);

	set_thread(0);

	nvme_bdev_ctrlr_create(&ctrlr, "nvme0", &trid, 0, NULL);

	nvme_bdev_ctrlr = nvme_bdev_ctrlr_get_by_name("nvme0");
	SPDK_CU_ASSERT_FATAL(nvme_bdev_ctrlr != NULL);

	ch1 = spdk_get_io_channel(nvme_bdev_ctrlr);
	SPDK_CU_ASSERT_FATAL(ch1 != NULL);

	set_thread(1);

	ch2 = spdk_get_io_channel(nvme_bdev_ctrlr);
	SPDK_CU_ASSERT_FATAL(ch2 != NULL);

	/* Reset starts from thread 1. */
	set_thread(1);

	rc = _bdev_nvme_reset(nvme_bdev_ctrlr);
	CU_ASSERT(rc == 0);
	CU_ASSERT(nvme_bdev_ctrlr->resetting == true);

	/* Try destructing ctrlr while ctrlr is being reset, but it will be deferred. */
	set_thread(0);

	rc = bdev_nvme_delete("nvme0", NULL);
	CU_ASSERT(rc == 0);
	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == nvme_bdev_ctrlr);
	CU_ASSERT(nvme_bdev_ctrlr->destruct == true);
	CU_ASSERT(nvme_bdev_ctrlr->resetting == true);

	poll_threads();

	/* Reset completed but ctrlr is not still destructed yet. */
	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == nvme_bdev_ctrlr);
	CU_ASSERT(nvme_bdev_ctrlr->destruct == true);
	CU_ASSERT(nvme_bdev_ctrlr->resetting == false);

	/* New reset request is rejected. */
	rc = _bdev_nvme_reset(nvme_bdev_ctrlr);
	CU_ASSERT(rc == -EBUSY);

	/* Additional polling called spdk_io_device_unregister() to ctrlr,
	 * However there are two channels and destruct is not completed yet.
	 */
	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == nvme_bdev_ctrlr);

	set_thread(0);

	spdk_put_io_channel(ch1);

	set_thread(1);

	spdk_put_io_channel(ch2);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == NULL);
}

static void
test_failover_ctrlr(void)
{
	struct spdk_nvme_transport_id trid1 = {}, trid2 = {};
	struct spdk_nvme_ctrlr ctrlr = {};
	struct nvme_bdev_ctrlr *nvme_bdev_ctrlr = NULL;
	struct nvme_bdev_ctrlr_trid *curr_trid, *next_trid;
	struct spdk_io_channel *ch1, *ch2;
	int rc;

	ut_init_trid(&trid1);
	ut_init_trid2(&trid2);
	TAILQ_INIT(&ctrlr.active_io_qpairs);

	set_thread(0);

	nvme_bdev_ctrlr_create(&ctrlr, "nvme0", &trid1, 0, NULL);

	nvme_bdev_ctrlr = nvme_bdev_ctrlr_get_by_name("nvme0");
	SPDK_CU_ASSERT_FATAL(nvme_bdev_ctrlr != NULL);

	ch1 = spdk_get_io_channel(nvme_bdev_ctrlr);
	SPDK_CU_ASSERT_FATAL(ch1 != NULL);

	set_thread(1);

	ch2 = spdk_get_io_channel(nvme_bdev_ctrlr);
	SPDK_CU_ASSERT_FATAL(ch2 != NULL);

	/* First, test one trid case. */
	curr_trid = TAILQ_FIRST(&nvme_bdev_ctrlr->trids);
	SPDK_CU_ASSERT_FATAL(curr_trid != NULL);

	/* Failover starts from thread 1. */
	set_thread(1);

	/* Case 1: ctrlr is already being destructed. */
	nvme_bdev_ctrlr->destruct = true;

	rc = bdev_nvme_failover(nvme_bdev_ctrlr, false);
	CU_ASSERT(rc == 0);
	CU_ASSERT(curr_trid->is_failed == false);

	/* Case 2: reset is in progress. */
	nvme_bdev_ctrlr->destruct = false;
	nvme_bdev_ctrlr->resetting = true;

	rc = bdev_nvme_failover(nvme_bdev_ctrlr, false);
	CU_ASSERT(rc == 0);

	/* Case 3: failover is in progress. */
	nvme_bdev_ctrlr->failover_in_progress = true;

	rc = bdev_nvme_failover(nvme_bdev_ctrlr, false);
	CU_ASSERT(rc == 0);
	CU_ASSERT(curr_trid->is_failed == false);

	/* Case 4: reset completes successfully. */
	nvme_bdev_ctrlr->resetting = false;
	nvme_bdev_ctrlr->failover_in_progress = false;

	rc = bdev_nvme_failover(nvme_bdev_ctrlr, false);
	CU_ASSERT(rc == 0);

	CU_ASSERT(nvme_bdev_ctrlr->resetting == true);
	CU_ASSERT(curr_trid->is_failed == true);

	poll_threads();

	curr_trid = TAILQ_FIRST(&nvme_bdev_ctrlr->trids);
	SPDK_CU_ASSERT_FATAL(curr_trid != NULL);

	CU_ASSERT(nvme_bdev_ctrlr->resetting == false);
	CU_ASSERT(curr_trid->is_failed == false);

	set_thread(0);

	/* Second, test two trids case. */
	bdev_nvme_add_secondary_trid(nvme_bdev_ctrlr, &ctrlr, &trid2, NULL);

	curr_trid = TAILQ_FIRST(&nvme_bdev_ctrlr->trids);
	SPDK_CU_ASSERT_FATAL(curr_trid != NULL);
	CU_ASSERT(&curr_trid->trid == nvme_bdev_ctrlr->connected_trid);
	CU_ASSERT(spdk_nvme_transport_id_compare(&curr_trid->trid, &trid1) == 0);

	/* Failover starts from thread 1. */
	set_thread(1);

	/* Case 5: reset is in progress. */
	nvme_bdev_ctrlr->resetting = true;

	rc = bdev_nvme_failover(nvme_bdev_ctrlr, false);
	CU_ASSERT(rc == -EAGAIN);

	/* Case 5: failover is in progress. */
	nvme_bdev_ctrlr->failover_in_progress = true;

	rc = bdev_nvme_failover(nvme_bdev_ctrlr, false);
	CU_ASSERT(rc == 0);

	/* Case 6: failover completes successfully. */
	nvme_bdev_ctrlr->resetting = false;
	nvme_bdev_ctrlr->failover_in_progress = false;

	rc = bdev_nvme_failover(nvme_bdev_ctrlr, false);
	CU_ASSERT(rc == 0);

	CU_ASSERT(nvme_bdev_ctrlr->resetting == true);
	CU_ASSERT(nvme_bdev_ctrlr->failover_in_progress == true);

	next_trid = TAILQ_FIRST(&nvme_bdev_ctrlr->trids);
	SPDK_CU_ASSERT_FATAL(next_trid != NULL);
	CU_ASSERT(next_trid != curr_trid);
	CU_ASSERT(&next_trid->trid == nvme_bdev_ctrlr->connected_trid);
	CU_ASSERT(spdk_nvme_transport_id_compare(&next_trid->trid, &trid2) == 0);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr->resetting == false);
	CU_ASSERT(nvme_bdev_ctrlr->failover_in_progress == false);

	spdk_put_io_channel(ch2);

	set_thread(0);

	spdk_put_io_channel(ch1);

	poll_threads();

	rc = bdev_nvme_delete("nvme0", NULL);
	CU_ASSERT(rc == 0);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == NULL);
}

static void
attach_ctrlr_done(void *cb_ctx, size_t bdev_count, int rc)
{
	CU_ASSERT(rc == g_ut_attach_ctrlr_status);
	CU_ASSERT(bdev_count == g_ut_attach_bdev_count);
}

static void
test_pending_reset(void)
{
	struct spdk_nvme_transport_id trid = {};
	struct spdk_nvme_host_id hostid = {};
	struct spdk_nvme_ctrlr *ctrlr;
	struct nvme_bdev_ctrlr *nvme_bdev_ctrlr = NULL;
	const int STRING_SIZE = 32;
	const char *attached_names[STRING_SIZE];
	struct spdk_bdev_io *first_bdev_io, *second_bdev_io;
	struct nvme_bdev_io *first_bio, *second_bio;
	struct spdk_io_channel *ch1, *ch2;
	struct nvme_io_channel *nvme_ch1, *nvme_ch2;
	int rc;

	memset(attached_names, 0, sizeof(char *) * STRING_SIZE);
	ut_init_trid(&trid);

	first_bdev_io = calloc(1, sizeof(struct spdk_bdev_io) + sizeof(struct nvme_bdev_io));
	SPDK_CU_ASSERT_FATAL(first_bdev_io != NULL);
	first_bdev_io->internal.status = SPDK_BDEV_IO_STATUS_FAILED;
	first_bio = (struct nvme_bdev_io *)first_bdev_io->driver_ctx;

	second_bdev_io = calloc(1, sizeof(struct spdk_bdev_io) + sizeof(struct nvme_bdev_io));
	SPDK_CU_ASSERT_FATAL(second_bdev_io != NULL);
	second_bdev_io->internal.status = SPDK_BDEV_IO_STATUS_FAILED;
	second_bio = (struct nvme_bdev_io *)second_bdev_io->driver_ctx;

	set_thread(0);

	ctrlr = ut_attach_ctrlr(&trid, 0);
	SPDK_CU_ASSERT_FATAL(ctrlr != NULL);

	g_ut_attach_ctrlr_status = 0;
	g_ut_attach_bdev_count = 0;

	rc = bdev_nvme_create(&trid, &hostid, "nvme0", attached_names, STRING_SIZE, NULL, 0,
			      attach_ctrlr_done, NULL, NULL);
	CU_ASSERT(rc == 0);

	spdk_delay_us(1000);
	poll_threads();

	nvme_bdev_ctrlr = nvme_bdev_ctrlr_get_by_name("nvme0");
	SPDK_CU_ASSERT_FATAL(nvme_bdev_ctrlr != NULL);

	ch1 = spdk_get_io_channel(nvme_bdev_ctrlr);
	SPDK_CU_ASSERT_FATAL(ch1 != NULL);

	nvme_ch1 = spdk_io_channel_get_ctx(ch1);

	set_thread(1);

	ch2 = spdk_get_io_channel(nvme_bdev_ctrlr);
	SPDK_CU_ASSERT_FATAL(ch2 != NULL);

	nvme_ch2 = spdk_io_channel_get_ctx(ch2);

	/* The first reset request is submitted on thread 1, and the second reset request
	 * is submitted on thread 0 while processing the first request.
	 */
	rc = bdev_nvme_reset(nvme_ch2, first_bio);
	CU_ASSERT(rc == 0);
	CU_ASSERT(nvme_bdev_ctrlr->resetting == true);
	CU_ASSERT(TAILQ_EMPTY(&nvme_ch2->pending_resets));

	set_thread(0);

	rc = bdev_nvme_reset(nvme_ch1, second_bio);
	CU_ASSERT(rc == 0);
	CU_ASSERT(TAILQ_FIRST(&nvme_ch1->pending_resets) == second_bdev_io);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr->resetting == false);
	CU_ASSERT(first_bdev_io->internal.status == SPDK_BDEV_IO_STATUS_SUCCESS);
	CU_ASSERT(second_bdev_io->internal.status == SPDK_BDEV_IO_STATUS_SUCCESS);

	/* The first reset request is submitted on thread 1, and the second reset request
	 * is submitted on thread 0 while processing the first request.
	 *
	 * The difference from the above scenario is that the controller is removed while
	 * processing the first request. Hence both reset requests should fail.
	 */
	set_thread(1);

	rc = bdev_nvme_reset(nvme_ch2, first_bio);
	CU_ASSERT(rc == 0);
	CU_ASSERT(nvme_bdev_ctrlr->resetting == true);
	CU_ASSERT(TAILQ_EMPTY(&nvme_ch2->pending_resets));

	set_thread(0);

	rc = bdev_nvme_reset(nvme_ch1, second_bio);
	CU_ASSERT(rc == 0);
	CU_ASSERT(TAILQ_FIRST(&nvme_ch1->pending_resets) == second_bdev_io);

	ctrlr->fail_reset = true;

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr->resetting == false);
	CU_ASSERT(first_bdev_io->internal.status == SPDK_BDEV_IO_STATUS_FAILED);
	CU_ASSERT(second_bdev_io->internal.status == SPDK_BDEV_IO_STATUS_FAILED);

	spdk_put_io_channel(ch1);

	set_thread(1);

	spdk_put_io_channel(ch2);

	poll_threads();

	set_thread(0);

	rc = bdev_nvme_delete("nvme0", NULL);
	CU_ASSERT(rc == 0);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == NULL);

	free(first_bdev_io);
	free(second_bdev_io);
}

static void
test_attach_ctrlr(void)
{
	struct spdk_nvme_transport_id trid = {};
	struct spdk_nvme_host_id hostid = {};
	struct spdk_nvme_ctrlr *ctrlr;
	struct nvme_bdev_ctrlr *nvme_bdev_ctrlr;
	const int STRING_SIZE = 32;
	const char *attached_names[STRING_SIZE];
	struct nvme_bdev *nbdev;
	int rc;

	set_thread(0);

	memset(attached_names, 0, sizeof(char *) * STRING_SIZE);
	ut_init_trid(&trid);

	/* If ctrlr fails, no nvme_bdev_ctrlr is created. Failed ctrlr is removed
	 * by probe polling.
	 */
	ctrlr = ut_attach_ctrlr(&trid, 0);
	SPDK_CU_ASSERT_FATAL(ctrlr != NULL);

	ctrlr->is_failed = true;
	g_ut_attach_ctrlr_status = -EIO;
	g_ut_attach_bdev_count = 0;

	rc = bdev_nvme_create(&trid, &hostid, "nvme0", attached_names, STRING_SIZE, NULL, 0,
			      attach_ctrlr_done, NULL, NULL);
	CU_ASSERT(rc == 0);

	spdk_delay_us(1000);
	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == NULL);

	/* If ctrlr has no namespace, one nvme_bdev_ctrlr with no namespace is created */
	ctrlr = ut_attach_ctrlr(&trid, 0);
	SPDK_CU_ASSERT_FATAL(ctrlr != NULL);

	g_ut_attach_ctrlr_status = 0;

	rc = bdev_nvme_create(&trid, &hostid, "nvme0", attached_names, STRING_SIZE, NULL, 0,
			      attach_ctrlr_done, NULL, NULL);
	CU_ASSERT(rc == 0);

	spdk_delay_us(1000);
	poll_threads();

	nvme_bdev_ctrlr = nvme_bdev_ctrlr_get_by_name("nvme0");
	SPDK_CU_ASSERT_FATAL(nvme_bdev_ctrlr != NULL);
	CU_ASSERT(nvme_bdev_ctrlr->ctrlr == ctrlr);
	CU_ASSERT(nvme_bdev_ctrlr->num_ns == 0);

	rc = bdev_nvme_delete("nvme0", NULL);
	CU_ASSERT(rc == 0);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == NULL);

	/* If ctrlr has one namespace, one nvme_bdev_ctrlr with one namespace and
	 * one nvme_bdev is created.
	 */
	ctrlr = ut_attach_ctrlr(&trid, 1);
	SPDK_CU_ASSERT_FATAL(ctrlr != NULL);

	g_ut_attach_bdev_count = 1;

	rc = bdev_nvme_create(&trid, &hostid, "nvme0", attached_names, STRING_SIZE, NULL, 0,
			      attach_ctrlr_done, NULL, NULL);
	CU_ASSERT(rc == 0);

	spdk_delay_us(1000);
	poll_threads();

	nvme_bdev_ctrlr = nvme_bdev_ctrlr_get_by_name("nvme0");
	SPDK_CU_ASSERT_FATAL(nvme_bdev_ctrlr != NULL);
	CU_ASSERT(nvme_bdev_ctrlr->ctrlr == ctrlr);
	CU_ASSERT(nvme_bdev_ctrlr->num_ns == 1);

	CU_ASSERT(attached_names[0] != NULL && strcmp(attached_names[0], "nvme0n1") == 0);
	attached_names[0] = NULL;

	nbdev = nvme_bdev_ctrlr->namespaces[0]->bdev;
	SPDK_CU_ASSERT_FATAL(nbdev != NULL);
	CU_ASSERT(bdev_nvme_get_ctrlr(&nbdev->disk) == ctrlr);

	rc = bdev_nvme_delete("nvme0", NULL);
	CU_ASSERT(rc == 0);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == NULL);

	/* Ctrlr has one namespace but one nvme_bdev_ctrlr with no namespace is
	 * created because creating one nvme_bdev failed.
	 */
	ctrlr = ut_attach_ctrlr(&trid, 1);
	SPDK_CU_ASSERT_FATAL(ctrlr != NULL);

	g_ut_register_bdev_status = -EINVAL;
	g_ut_attach_bdev_count = 0;

	rc = bdev_nvme_create(&trid, &hostid, "nvme0", attached_names, STRING_SIZE, NULL, 0,
			      attach_ctrlr_done, NULL, NULL);
	CU_ASSERT(rc == 0);

	spdk_delay_us(1000);
	poll_threads();

	nvme_bdev_ctrlr = nvme_bdev_ctrlr_get_by_name("nvme0");
	SPDK_CU_ASSERT_FATAL(nvme_bdev_ctrlr != NULL);
	CU_ASSERT(nvme_bdev_ctrlr->ctrlr == ctrlr);
	CU_ASSERT(nvme_bdev_ctrlr->num_ns == 1);

	CU_ASSERT(attached_names[0] == NULL);

	rc = bdev_nvme_delete("nvme0", NULL);
	CU_ASSERT(rc == 0);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == NULL);

	g_ut_register_bdev_status = 0;
}

static void
test_reconnect_qpair(void)
{
	struct spdk_nvme_transport_id trid = {};
	struct spdk_nvme_ctrlr ctrlr = {};
	struct nvme_bdev_ctrlr *nvme_bdev_ctrlr = NULL;
	struct spdk_io_channel *ch;
	struct nvme_io_channel *nvme_ch;
	int rc;

	set_thread(0);

	ut_init_trid(&trid);
	TAILQ_INIT(&ctrlr.active_io_qpairs);

	nvme_bdev_ctrlr_create(&ctrlr, "nvme0", &trid, 0, NULL);

	nvme_bdev_ctrlr = nvme_bdev_ctrlr_get_by_name("nvme0");
	SPDK_CU_ASSERT_FATAL(nvme_bdev_ctrlr != NULL);

	ch = spdk_get_io_channel(nvme_bdev_ctrlr);
	SPDK_CU_ASSERT_FATAL(ch != NULL);

	nvme_ch = spdk_io_channel_get_ctx(ch);
	CU_ASSERT(nvme_ch->qpair != NULL);
	CU_ASSERT(nvme_ch->group != NULL);
	CU_ASSERT(nvme_ch->group->group != NULL);
	CU_ASSERT(nvme_ch->group->poller != NULL);

	/* Test if the disconnected qpair is reconnected. */
	nvme_ch->qpair->is_connected = false;

	poll_threads();

	CU_ASSERT(nvme_ch->qpair->is_connected == true);

	/* If the ctrlr is failed, reconnecting qpair should fail too. */
	nvme_ch->qpair->is_connected = false;
	ctrlr.is_failed = true;

	poll_threads();

	CU_ASSERT(nvme_ch->qpair->is_connected == false);

	spdk_put_io_channel(ch);

	poll_threads();

	rc = bdev_nvme_delete("nvme0", NULL);
	CU_ASSERT(rc == 0);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == NULL);
}

static void
test_aer_cb(void)
{
	struct spdk_nvme_transport_id trid = {};
	struct spdk_nvme_host_id hostid = {};
	struct spdk_nvme_ctrlr *ctrlr;
	struct nvme_bdev_ctrlr *nvme_bdev_ctrlr;
	struct nvme_bdev *bdev;
	const int STRING_SIZE = 32;
	const char *attached_names[STRING_SIZE];
	union spdk_nvme_async_event_completion event = {};
	struct spdk_nvme_cpl cpl = {};
	int rc;

	set_thread(0);

	memset(attached_names, 0, sizeof(char *) * STRING_SIZE);
	ut_init_trid(&trid);

	/* Attach a ctrlr, whose max number of namespaces is 4, and 2nd, 3rd, and 4th
	 * namespaces are populated.
	 */
	ctrlr = ut_attach_ctrlr(&trid, 4);
	SPDK_CU_ASSERT_FATAL(ctrlr != NULL);

	ctrlr->ns[0].is_active = false;

	g_ut_attach_ctrlr_status = 0;
	g_ut_attach_bdev_count = 3;

	rc = bdev_nvme_create(&trid, &hostid, "nvme0", attached_names, STRING_SIZE, NULL, 0,
			      attach_ctrlr_done, NULL, NULL);
	CU_ASSERT(rc == 0);

	spdk_delay_us(1000);
	poll_threads();

	nvme_bdev_ctrlr = nvme_bdev_ctrlr_get_by_name("nvme0");
	SPDK_CU_ASSERT_FATAL(nvme_bdev_ctrlr != NULL);

	CU_ASSERT(nvme_bdev_ctrlr->num_ns == 4);
	CU_ASSERT(nvme_bdev_ctrlr->namespaces[0]->populated == false);
	CU_ASSERT(nvme_bdev_ctrlr->namespaces[1]->populated == true);
	CU_ASSERT(nvme_bdev_ctrlr->namespaces[2]->populated == true);
	CU_ASSERT(nvme_bdev_ctrlr->namespaces[3]->populated == true);

	bdev = nvme_bdev_ctrlr->namespaces[3]->bdev;
	SPDK_CU_ASSERT_FATAL(bdev != NULL);
	CU_ASSERT(bdev->disk.blockcnt == 1024);

	/* Dynamically populate 1st namespace and depopulate 3rd namespace, and
	 * change the size of the 4th namespace.
	 */
	ctrlr->ns[0].is_active = true;
	ctrlr->ns[2].is_active = false;
	ctrlr->nsdata[3].nsze = 2048;

	event.bits.async_event_type = SPDK_NVME_ASYNC_EVENT_TYPE_NOTICE;
	event.bits.async_event_info = SPDK_NVME_ASYNC_EVENT_NS_ATTR_CHANGED;
	cpl.cdw0 = event.raw;

	aer_cb(nvme_bdev_ctrlr, &cpl);

	CU_ASSERT(nvme_bdev_ctrlr->namespaces[0]->populated == true);
	CU_ASSERT(nvme_bdev_ctrlr->namespaces[1]->populated == true);
	CU_ASSERT(nvme_bdev_ctrlr->namespaces[2]->populated == false);
	CU_ASSERT(nvme_bdev_ctrlr->namespaces[3]->populated == true);
	CU_ASSERT(bdev->disk.blockcnt == 2048);

	rc = bdev_nvme_delete("nvme0", NULL);
	CU_ASSERT(rc == 0);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == NULL);
}

static void
ut_test_submit_nvme_cmd(struct spdk_io_channel *ch, struct spdk_bdev_io *bdev_io,
			enum spdk_bdev_io_type io_type)
{
	struct nvme_io_channel *nvme_ch = spdk_io_channel_get_ctx(ch);
	struct nvme_bdev *nbdev = (struct nvme_bdev *)bdev_io->bdev->ctxt;
	struct nvme_bdev_ns *nvme_ns = NULL;
	struct spdk_nvme_qpair *qpair = NULL;

	CU_ASSERT(bdev_nvme_find_io_path(nbdev, nvme_ch, &nvme_ns, &qpair));

	bdev_io->type = io_type;
	bdev_io->internal.in_submit_request = true;

	bdev_nvme_submit_request(ch, bdev_io);

	CU_ASSERT(bdev_io->internal.in_submit_request == true);
	CU_ASSERT(qpair->num_outstanding_reqs == 1);

	poll_threads();

	CU_ASSERT(bdev_io->internal.in_submit_request == false);
	CU_ASSERT(bdev_io->internal.status == SPDK_BDEV_IO_STATUS_SUCCESS);
	CU_ASSERT(qpair->num_outstanding_reqs == 0);
}

static void
ut_test_submit_nop(struct spdk_io_channel *ch, struct spdk_bdev_io *bdev_io,
		   enum spdk_bdev_io_type io_type)
{
	struct nvme_io_channel *nvme_ch = spdk_io_channel_get_ctx(ch);
	struct nvme_bdev *nbdev = (struct nvme_bdev *)bdev_io->bdev->ctxt;
	struct nvme_bdev_ns *nvme_ns = NULL;
	struct spdk_nvme_qpair *qpair = NULL;

	CU_ASSERT(bdev_nvme_find_io_path(nbdev, nvme_ch, &nvme_ns, &qpair));

	bdev_io->type = io_type;
	bdev_io->internal.in_submit_request = true;

	bdev_nvme_submit_request(ch, bdev_io);

	CU_ASSERT(bdev_io->internal.in_submit_request == false);
	CU_ASSERT(bdev_io->internal.status == SPDK_BDEV_IO_STATUS_SUCCESS);
	CU_ASSERT(qpair->num_outstanding_reqs == 0);
}

static void
ut_test_submit_fused_nvme_cmd(struct spdk_io_channel *ch, struct spdk_bdev_io *bdev_io)
{
	struct nvme_io_channel *nvme_ch = spdk_io_channel_get_ctx(ch);
	struct nvme_bdev_io *bio = (struct nvme_bdev_io *)bdev_io->driver_ctx;
	struct ut_nvme_req *req;
	struct nvme_bdev *nbdev = (struct nvme_bdev *)bdev_io->bdev->ctxt;
	struct nvme_bdev_ns *nvme_ns = NULL;
	struct spdk_nvme_qpair *qpair = NULL;

	CU_ASSERT(bdev_nvme_find_io_path(nbdev, nvme_ch, &nvme_ns, &qpair));

	/* Only compare and write now. */
	bdev_io->type = SPDK_BDEV_IO_TYPE_COMPARE_AND_WRITE;
	bdev_io->internal.in_submit_request = true;

	bdev_nvme_submit_request(ch, bdev_io);

	CU_ASSERT(bdev_io->internal.in_submit_request == true);
	CU_ASSERT(qpair->num_outstanding_reqs == 2);
	CU_ASSERT(bio->first_fused_submitted == true);

	/* First outstanding request is compare operation. */
	req = TAILQ_FIRST(&nvme_ch->qpair->outstanding_reqs);
	SPDK_CU_ASSERT_FATAL(req != NULL);
	CU_ASSERT(req->opc == SPDK_NVME_OPC_COMPARE);
	req->cpl.cdw0 = SPDK_NVME_OPC_COMPARE;

	poll_threads();

	CU_ASSERT(bdev_io->internal.in_submit_request == false);
	CU_ASSERT(bdev_io->internal.status == SPDK_BDEV_IO_STATUS_SUCCESS);
	CU_ASSERT(qpair->num_outstanding_reqs == 0);
}

static void
ut_test_submit_admin_cmd(struct spdk_io_channel *ch, struct spdk_bdev_io *bdev_io,
			 struct spdk_nvme_ctrlr *ctrlr)
{
	bdev_io->type = SPDK_BDEV_IO_TYPE_NVME_ADMIN;
	bdev_io->internal.in_submit_request = true;
	bdev_io->u.nvme_passthru.cmd.opc = SPDK_NVME_OPC_GET_FEATURES;

	bdev_nvme_submit_request(ch, bdev_io);

	CU_ASSERT(bdev_io->internal.in_submit_request == true);
	CU_ASSERT(ctrlr->adminq.num_outstanding_reqs == 1);

	spdk_delay_us(10000);
	poll_thread_times(1, 1);

	CU_ASSERT(bdev_io->internal.in_submit_request == true);
	CU_ASSERT(ctrlr->adminq.num_outstanding_reqs == 0);

	poll_thread_times(0, 1);

	CU_ASSERT(bdev_io->internal.in_submit_request == false);
}

static void
test_submit_nvme_cmd(void)
{
	struct spdk_nvme_transport_id trid = {};
	struct spdk_nvme_host_id hostid = {};
	struct spdk_nvme_ctrlr *ctrlr;
	struct nvme_bdev_ctrlr *nvme_bdev_ctrlr;
	const int STRING_SIZE = 32;
	const char *attached_names[STRING_SIZE];
	struct nvme_bdev *bdev;
	struct spdk_bdev_io *bdev_io;
	struct spdk_io_channel *ch;
	int rc;

	memset(attached_names, 0, sizeof(char *) * STRING_SIZE);
	ut_init_trid(&trid);

	set_thread(1);

	ctrlr = ut_attach_ctrlr(&trid, 1);
	SPDK_CU_ASSERT_FATAL(ctrlr != NULL);

	g_ut_attach_ctrlr_status = 0;
	g_ut_attach_bdev_count = 1;

	rc = bdev_nvme_create(&trid, &hostid, "nvme0", attached_names, STRING_SIZE, NULL, 0,
			      attach_ctrlr_done, NULL, NULL);
	CU_ASSERT(rc == 0);

	spdk_delay_us(1000);
	poll_threads();

	nvme_bdev_ctrlr = nvme_bdev_ctrlr_get_by_name("nvme0");
	SPDK_CU_ASSERT_FATAL(nvme_bdev_ctrlr != NULL);

	bdev = nvme_bdev_ctrlr->namespaces[0]->bdev;
	SPDK_CU_ASSERT_FATAL(bdev != NULL);

	set_thread(0);

	ch = spdk_get_io_channel(nvme_bdev_ctrlr);
	SPDK_CU_ASSERT_FATAL(ch != NULL);

	bdev_io = calloc(1, sizeof(struct spdk_bdev_io) + sizeof(struct nvme_bdev_io));
	SPDK_CU_ASSERT_FATAL(bdev_io != NULL);
	bdev_io->bdev = &bdev->disk;
	bdev_io->internal.ch = (struct spdk_bdev_channel *)ch;

	bdev_io->u.bdev.iovs = NULL;

	ut_test_submit_nvme_cmd(ch, bdev_io, SPDK_BDEV_IO_TYPE_READ);

	ut_bdev_io_set_buf(bdev_io);

	ut_test_submit_nvme_cmd(ch, bdev_io, SPDK_BDEV_IO_TYPE_READ);
	ut_test_submit_nvme_cmd(ch, bdev_io, SPDK_BDEV_IO_TYPE_WRITE);
	ut_test_submit_nvme_cmd(ch, bdev_io, SPDK_BDEV_IO_TYPE_COMPARE);
	ut_test_submit_nvme_cmd(ch, bdev_io, SPDK_BDEV_IO_TYPE_UNMAP);

	ut_test_submit_nop(ch, bdev_io, SPDK_BDEV_IO_TYPE_FLUSH);

	ut_test_submit_fused_nvme_cmd(ch, bdev_io);

	ut_test_submit_admin_cmd(ch, bdev_io, ctrlr);

	free(bdev_io);

	spdk_put_io_channel(ch);

	poll_threads();

	set_thread(1);

	rc = bdev_nvme_delete("nvme0", NULL);
	CU_ASSERT(rc == 0);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == NULL);
}

static void
test_remove_trid(void)
{
	struct spdk_nvme_transport_id trid1 = {}, trid2 = {}, trid3 = {};
	struct spdk_nvme_ctrlr ctrlr = {};
	struct nvme_bdev_ctrlr *nvme_bdev_ctrlr = NULL;
	struct nvme_bdev_ctrlr_trid *ctrid;
	int rc;

	ut_init_trid(&trid1);
	ut_init_trid2(&trid2);
	ut_init_trid3(&trid3);

	set_thread(0);

	nvme_bdev_ctrlr_create(&ctrlr, "nvme0", &trid1, 0, NULL);

	nvme_bdev_ctrlr = nvme_bdev_ctrlr_get_by_name("nvme0");
	SPDK_CU_ASSERT_FATAL(nvme_bdev_ctrlr != NULL);

	bdev_nvme_add_secondary_trid(nvme_bdev_ctrlr, &ctrlr, &trid2, NULL);

	/* trid3 is not in the registered list. */
	rc = bdev_nvme_delete("nvme0", &trid3);
	CU_ASSERT(rc == -ENXIO);

	/* trid2 is not used, and simply removed. */
	rc = bdev_nvme_delete("nvme0", &trid2);
	CU_ASSERT(rc == 0);
	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == nvme_bdev_ctrlr);
	TAILQ_FOREACH(ctrid, &nvme_bdev_ctrlr->trids, link) {
		CU_ASSERT(spdk_nvme_transport_id_compare(&ctrid->trid, &trid2) != 0);
	}

	bdev_nvme_add_secondary_trid(nvme_bdev_ctrlr, &ctrlr, &trid3, NULL);

	/* trid1 is currently used and trid3 is an alternative path.
	 * If we remove trid1, path is changed to trid3.
	 */
	rc = bdev_nvme_delete("nvme0", &trid1);
	CU_ASSERT(rc == 0);
	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == nvme_bdev_ctrlr);
	CU_ASSERT(nvme_bdev_ctrlr->resetting == true);
	TAILQ_FOREACH(ctrid, &nvme_bdev_ctrlr->trids, link) {
		CU_ASSERT(spdk_nvme_transport_id_compare(&ctrid->trid, &trid1) != 0);
	}
	CU_ASSERT(spdk_nvme_transport_id_compare(nvme_bdev_ctrlr->connected_trid, &trid3) == 0);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr->resetting == false);

	/* trid3 is the current and only path. If we remove trid3, the corresponding
	 * nvme_bdev_ctrlr is removed.
	 */
	rc = bdev_nvme_delete("nvme0", &trid3);
	CU_ASSERT(rc == 0);
	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == nvme_bdev_ctrlr);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == NULL);

	nvme_bdev_ctrlr_create(&ctrlr, "nvme0", &trid1, 0, NULL);

	nvme_bdev_ctrlr = nvme_bdev_ctrlr_get_by_name("nvme0");
	SPDK_CU_ASSERT_FATAL(nvme_bdev_ctrlr != NULL);

	bdev_nvme_add_secondary_trid(nvme_bdev_ctrlr, &ctrlr, &trid2, NULL);

	/* If trid is not specified, nvme_bdev_ctrlr itself is removed. */
	rc = bdev_nvme_delete("nvme0", NULL);
	CU_ASSERT(rc == 0);
	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == nvme_bdev_ctrlr);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == NULL);
}

static void
test_abort(void)
{
	struct spdk_nvme_transport_id trid = {};
	struct spdk_nvme_host_id hostid = {};
	struct spdk_nvme_ctrlr *ctrlr;
	struct nvme_bdev_ctrlr *nvme_bdev_ctrlr;
	const int STRING_SIZE = 32;
	const char *attached_names[STRING_SIZE];
	struct nvme_bdev *bdev;
	struct spdk_bdev_io *write_io, *admin_io, *abort_io;
	struct spdk_io_channel *ch;
	struct nvme_io_channel *nvme_ch;
	int rc;

	/* Create ctrlr on thread 1 and submit I/O and admin requests to be aborted on
	 * thread 0. Abort requests are submitted on thread 0. Aborting I/O requests are
	 * done on thread 0 but aborting admin requests are done on thread 1.
	 */

	ut_init_trid(&trid);

	ctrlr = ut_attach_ctrlr(&trid, 1);
	SPDK_CU_ASSERT_FATAL(ctrlr != NULL);

	g_ut_attach_ctrlr_status = 0;
	g_ut_attach_bdev_count = 1;

	set_thread(1);

	rc = bdev_nvme_create(&trid, &hostid, "nvme0", attached_names, STRING_SIZE, NULL, 0,
			      attach_ctrlr_done, NULL, NULL);
	CU_ASSERT(rc == 0);

	spdk_delay_us(1000);
	poll_threads();

	nvme_bdev_ctrlr = nvme_bdev_ctrlr_get_by_name("nvme0");
	SPDK_CU_ASSERT_FATAL(nvme_bdev_ctrlr != NULL);

	bdev = nvme_bdev_ctrlr->namespaces[0]->bdev;
	SPDK_CU_ASSERT_FATAL(bdev != NULL);

	set_thread(0);

	write_io = calloc(1, sizeof(struct spdk_bdev_io) + sizeof(struct nvme_bdev_io));
	SPDK_CU_ASSERT_FATAL(write_io != NULL);
	write_io->bdev = &bdev->disk;
	write_io->type = SPDK_BDEV_IO_TYPE_WRITE;
	ut_bdev_io_set_buf(write_io);

	admin_io = calloc(1, sizeof(struct spdk_bdev_io) + sizeof(struct nvme_bdev_io));
	SPDK_CU_ASSERT_FATAL(admin_io != NULL);
	admin_io->bdev = &bdev->disk;
	admin_io->type = SPDK_BDEV_IO_TYPE_NVME_ADMIN;
	admin_io->u.nvme_passthru.cmd.opc = SPDK_NVME_OPC_GET_FEATURES;

	abort_io = calloc(1, sizeof(struct spdk_bdev_io) + sizeof(struct nvme_bdev_io));
	SPDK_CU_ASSERT_FATAL(abort_io != NULL);
	abort_io->bdev = &bdev->disk;
	abort_io->type = SPDK_BDEV_IO_TYPE_ABORT;

	ch = spdk_get_io_channel(nvme_bdev_ctrlr);
	SPDK_CU_ASSERT_FATAL(ch != NULL);
	nvme_ch = spdk_io_channel_get_ctx(ch);

	write_io->internal.ch = (struct spdk_bdev_channel *)ch;
	admin_io->internal.ch = (struct spdk_bdev_channel *)ch;
	abort_io->internal.ch = (struct spdk_bdev_channel *)ch;

	/* Aborting the already completed request should fail. */
	write_io->internal.in_submit_request = true;
	bdev_nvme_submit_request(ch, write_io);
	poll_threads();

	CU_ASSERT(write_io->internal.in_submit_request == false);

	abort_io->u.abort.bio_to_abort = write_io;
	abort_io->internal.in_submit_request = true;

	bdev_nvme_submit_request(ch, abort_io);

	poll_threads();

	CU_ASSERT(abort_io->internal.in_submit_request == false);
	CU_ASSERT(abort_io->internal.status == SPDK_BDEV_IO_STATUS_FAILED);
	CU_ASSERT(ctrlr->adminq.num_outstanding_reqs == 0);

	admin_io->internal.in_submit_request = true;
	bdev_nvme_submit_request(ch, admin_io);
	spdk_delay_us(10000);
	poll_threads();

	CU_ASSERT(admin_io->internal.in_submit_request == false);

	abort_io->u.abort.bio_to_abort = admin_io;
	abort_io->internal.in_submit_request = true;

	bdev_nvme_submit_request(ch, abort_io);

	poll_threads();

	CU_ASSERT(abort_io->internal.in_submit_request == false);
	CU_ASSERT(abort_io->internal.status == SPDK_BDEV_IO_STATUS_FAILED);
	CU_ASSERT(ctrlr->adminq.num_outstanding_reqs == 0);

	/* Aborting the write request should succeed. */
	write_io->internal.in_submit_request = true;
	bdev_nvme_submit_request(ch, write_io);

	CU_ASSERT(write_io->internal.in_submit_request == true);
	CU_ASSERT(nvme_ch->qpair->num_outstanding_reqs == 1);

	abort_io->u.abort.bio_to_abort = write_io;
	abort_io->internal.in_submit_request = true;

	bdev_nvme_submit_request(ch, abort_io);

	spdk_delay_us(10000);
	poll_threads();

	CU_ASSERT(abort_io->internal.in_submit_request == false);
	CU_ASSERT(abort_io->internal.status == SPDK_BDEV_IO_STATUS_SUCCESS);
	CU_ASSERT(ctrlr->adminq.num_outstanding_reqs == 0);
	CU_ASSERT(write_io->internal.in_submit_request == false);
	CU_ASSERT(write_io->internal.status == SPDK_BDEV_IO_STATUS_ABORTED);
	CU_ASSERT(nvme_ch->qpair->num_outstanding_reqs == 0);

	/* Aborting the admin request should succeed. */
	admin_io->internal.in_submit_request = true;
	bdev_nvme_submit_request(ch, admin_io);

	CU_ASSERT(admin_io->internal.in_submit_request == true);
	CU_ASSERT(ctrlr->adminq.num_outstanding_reqs == 1);

	abort_io->u.abort.bio_to_abort = admin_io;
	abort_io->internal.in_submit_request = true;

	bdev_nvme_submit_request(ch, abort_io);

	spdk_delay_us(10000);
	poll_threads();

	CU_ASSERT(abort_io->internal.in_submit_request == false);
	CU_ASSERT(abort_io->internal.status == SPDK_BDEV_IO_STATUS_SUCCESS);
	CU_ASSERT(ctrlr->adminq.num_outstanding_reqs == 0);
	CU_ASSERT(admin_io->internal.in_submit_request == false);
	CU_ASSERT(admin_io->internal.status == SPDK_BDEV_IO_STATUS_ABORTED);
	CU_ASSERT(ctrlr->adminq.num_outstanding_reqs == 0);

	spdk_put_io_channel(ch);

	poll_threads();

	free(write_io);
	free(admin_io);
	free(abort_io);

	set_thread(1);

	rc = bdev_nvme_delete("nvme0", NULL);
	CU_ASSERT(rc == 0);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == NULL);
}

static void
test_get_io_qpair(void)
{
	struct spdk_nvme_transport_id trid = {};
	struct spdk_nvme_ctrlr ctrlr = {};
	struct nvme_bdev_ctrlr *nvme_bdev_ctrlr = NULL;
	struct spdk_io_channel *ch;
	struct nvme_io_channel *nvme_ch;
	struct spdk_nvme_qpair *qpair;
	int rc;

	ut_init_trid(&trid);
	TAILQ_INIT(&ctrlr.active_io_qpairs);

	set_thread(0);

	nvme_bdev_ctrlr_create(&ctrlr, "nvme0", &trid, 0, NULL);

	nvme_bdev_ctrlr = nvme_bdev_ctrlr_get_by_name("nvme0");
	SPDK_CU_ASSERT_FATAL(nvme_bdev_ctrlr != NULL);

	ch = spdk_get_io_channel(nvme_bdev_ctrlr);
	SPDK_CU_ASSERT_FATAL(ch != NULL);
	nvme_ch = spdk_io_channel_get_ctx(ch);
	CU_ASSERT(nvme_ch->qpair != NULL);

	qpair = bdev_nvme_get_io_qpair(ch);
	CU_ASSERT(qpair == nvme_ch->qpair);

	spdk_put_io_channel(ch);

	rc = bdev_nvme_delete("nvme0", NULL);
	CU_ASSERT(rc == 0);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == NULL);
}

/* Test a scenario that the bdev subsystem starts shutdown when there still exists
 * any NVMe bdev. In this scenario, spdk_bdev_unregister() is called first. Add a
 * test case to avoid regression for this scenario. spdk_bdev_unregister() calls
 * bdev_nvme_destruct() in the end, and so call bdev_nvme_destruct() directly.
 */
static void
test_bdev_unregister(void)
{
	struct spdk_nvme_transport_id trid = {};
	struct spdk_nvme_host_id hostid = {};
	struct spdk_nvme_ctrlr *ctrlr;
	struct nvme_bdev_ctrlr *nvme_bdev_ctrlr;
	struct nvme_bdev_ns *nvme_ns1, *nvme_ns2;
	const int STRING_SIZE = 32;
	const char *attached_names[STRING_SIZE];
	struct nvme_bdev *bdev1, *bdev2;
	int rc;

	memset(attached_names, 0, sizeof(char *) * STRING_SIZE);
	ut_init_trid(&trid);

	ctrlr = ut_attach_ctrlr(&trid, 2);
	SPDK_CU_ASSERT_FATAL(ctrlr != NULL);

	g_ut_attach_ctrlr_status = 0;
	g_ut_attach_bdev_count = 2;

	rc = bdev_nvme_create(&trid, &hostid, "nvme0", attached_names, STRING_SIZE, NULL, 0,
			      attach_ctrlr_done, NULL, NULL);
	CU_ASSERT(rc == 0);

	spdk_delay_us(1000);
	poll_threads();

	nvme_bdev_ctrlr = nvme_bdev_ctrlr_get_by_name("nvme0");
	SPDK_CU_ASSERT_FATAL(nvme_bdev_ctrlr != NULL);

	nvme_ns1 = nvme_bdev_ctrlr->namespaces[0];
	SPDK_CU_ASSERT_FATAL(nvme_ns1 != NULL);

	bdev1 = nvme_ns1->bdev;
	SPDK_CU_ASSERT_FATAL(bdev1 != NULL);

	nvme_ns2 = nvme_bdev_ctrlr->namespaces[1];
	SPDK_CU_ASSERT_FATAL(nvme_ns2 != NULL);

	bdev2 = nvme_ns2->bdev;
	SPDK_CU_ASSERT_FATAL(bdev2 != NULL);

	bdev_nvme_destruct(&bdev1->disk);
	bdev_nvme_destruct(&bdev2->disk);

	poll_threads();

	CU_ASSERT(nvme_ns1->bdev == NULL);
	CU_ASSERT(nvme_ns2->bdev == NULL);

	nvme_bdev_ctrlr->destruct = true;
	_nvme_bdev_ctrlr_destruct(nvme_bdev_ctrlr);

	poll_threads();

	CU_ASSERT(nvme_bdev_ctrlr_get_by_name("nvme0") == NULL);
}

static void
init_accel(void)
{
	spdk_io_device_register(g_accel_p, accel_engine_create_cb, accel_engine_destroy_cb,
				sizeof(int), "accel_p");
}

static void
fini_accel(void)
{
	spdk_io_device_unregister(g_accel_p, NULL);
}

int
main(int argc, const char **argv)
{
	CU_pSuite	suite = NULL;
	unsigned int	num_failures;

	CU_set_error_action(CUEA_ABORT);
	CU_initialize_registry();

	suite = CU_add_suite("nvme", NULL, NULL);

	CU_ADD_TEST(suite, test_create_ctrlr);
	CU_ADD_TEST(suite, test_reset_ctrlr);
	CU_ADD_TEST(suite, test_race_between_reset_and_destruct_ctrlr);
	CU_ADD_TEST(suite, test_failover_ctrlr);
	CU_ADD_TEST(suite, test_pending_reset);
	CU_ADD_TEST(suite, test_attach_ctrlr);
	CU_ADD_TEST(suite, test_reconnect_qpair);
	CU_ADD_TEST(suite, test_aer_cb);
	CU_ADD_TEST(suite, test_submit_nvme_cmd);
	CU_ADD_TEST(suite, test_remove_trid);
	CU_ADD_TEST(suite, test_abort);
	CU_ADD_TEST(suite, test_get_io_qpair);
	CU_ADD_TEST(suite, test_bdev_unregister);

	CU_basic_set_mode(CU_BRM_VERBOSE);

	allocate_threads(3);
	set_thread(0);
	bdev_nvme_library_init();
	init_accel();

	CU_basic_run_tests();

	set_thread(0);
	bdev_nvme_library_fini();
	fini_accel();
	free_threads();

	num_failures = CU_get_number_of_failures();
	CU_cleanup_registry();

	return num_failures;
}
