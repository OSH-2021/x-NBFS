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
#include "nvme/nvme_rdma.c"
#include "common/lib/nvme/common_stubs.h"
#include "common/lib/test_rdma.c"

SPDK_LOG_REGISTER_COMPONENT(nvme)

DEFINE_STUB(spdk_mem_map_set_translation, int, (struct spdk_mem_map *map, uint64_t vaddr,
		uint64_t size, uint64_t translation), 0);
DEFINE_STUB(spdk_mem_map_clear_translation, int, (struct spdk_mem_map *map, uint64_t vaddr,
		uint64_t size), 0);

DEFINE_STUB(spdk_mem_map_alloc, struct spdk_mem_map *, (uint64_t default_translation,
		const struct spdk_mem_map_ops *ops, void *cb_ctx), NULL);
DEFINE_STUB_V(spdk_mem_map_free, (struct spdk_mem_map **pmap));

DEFINE_STUB(nvme_poll_group_connect_qpair, int, (struct spdk_nvme_qpair *qpair), 0);

DEFINE_STUB_V(nvme_qpair_resubmit_requests, (struct spdk_nvme_qpair *qpair, uint32_t num_requests));
DEFINE_STUB(spdk_nvme_poll_group_process_completions, int64_t, (struct spdk_nvme_poll_group *group,
		uint32_t completions_per_qpair, spdk_nvme_disconnected_qpair_cb disconnected_qpair_cb), 0);

DEFINE_STUB(rdma_ack_cm_event, int, (struct rdma_cm_event *event), 0);
DEFINE_STUB_V(rdma_free_devices, (struct ibv_context **list));
DEFINE_STUB(fcntl, int, (int fd, int cmd, ...), 0);
DEFINE_STUB_V(rdma_destroy_event_channel, (struct rdma_event_channel *channel));

DEFINE_STUB(ibv_dereg_mr, int, (struct ibv_mr *mr), 0);

/* ibv_reg_mr can be a macro, need to undefine it */
#ifdef ibv_reg_mr
#undef ibv_reg_mr
#endif

DEFINE_RETURN_MOCK(ibv_reg_mr, struct ibv_mr *);
struct ibv_mr *
ibv_reg_mr(struct ibv_pd *pd, void *addr, size_t length, int access)
{
	HANDLE_RETURN_MOCK(ibv_reg_mr);
	if (length > 0) {
		return &g_rdma_mr;
	} else {
		return NULL;
	}
}

struct nvme_rdma_ut_bdev_io {
	struct iovec iovs[NVME_RDMA_MAX_SGL_DESCRIPTORS];
	int iovpos;
};

DEFINE_RETURN_MOCK(rdma_get_devices, struct ibv_context **);
struct ibv_context **
rdma_get_devices(int *num_devices)
{
	static struct ibv_context *_contexts[] = {
		(struct ibv_context *)0xDEADBEEF,
		(struct ibv_context *)0xFEEDBEEF,
		NULL
	};

	HANDLE_RETURN_MOCK(rdma_get_devices);
	return _contexts;
}

DEFINE_RETURN_MOCK(rdma_create_event_channel, struct rdma_event_channel *);
struct rdma_event_channel *
rdma_create_event_channel(void)
{
	HANDLE_RETURN_MOCK(rdma_create_event_channel);
	return NULL;
}

DEFINE_RETURN_MOCK(ibv_query_device, int);
int
ibv_query_device(struct ibv_context *context,
		 struct ibv_device_attr *device_attr)
{
	if (device_attr) {
		device_attr->max_sge = NVME_RDMA_MAX_SGL_DESCRIPTORS;
	}
	HANDLE_RETURN_MOCK(ibv_query_device);

	return 0;
}

/* essentially a simplification of bdev_nvme_next_sge and bdev_nvme_reset_sgl */
static void nvme_rdma_ut_reset_sgl(void *cb_arg, uint32_t offset)
{
	struct nvme_rdma_ut_bdev_io *bio = cb_arg;
	struct iovec *iov;

	for (bio->iovpos = 0; bio->iovpos < NVME_RDMA_MAX_SGL_DESCRIPTORS; bio->iovpos++) {
		iov = &bio->iovs[bio->iovpos];
		/* Only provide offsets at the beginning of an iov */
		if (offset == 0) {
			break;
		}

		offset -= iov->iov_len;
	}

	SPDK_CU_ASSERT_FATAL(bio->iovpos < NVME_RDMA_MAX_SGL_DESCRIPTORS);
}

static int nvme_rdma_ut_next_sge(void *cb_arg, void **address, uint32_t *length)
{
	struct nvme_rdma_ut_bdev_io *bio = cb_arg;
	struct iovec *iov;

	SPDK_CU_ASSERT_FATAL(bio->iovpos < NVME_RDMA_MAX_SGL_DESCRIPTORS);

	iov = &bio->iovs[bio->iovpos];

	*address = iov->iov_base;
	*length = iov->iov_len;
	bio->iovpos++;

	return 0;
}

static void
test_nvme_rdma_build_sgl_request(void)
{
	struct nvme_rdma_qpair rqpair;
	struct spdk_nvme_ctrlr ctrlr = {0};
	struct spdk_nvmf_cmd cmd = {{0}};
	struct spdk_nvme_rdma_req rdma_req = {0};
	struct nvme_request req = {{0}};
	struct nvme_rdma_ut_bdev_io bio;
	uint64_t i;
	int rc;

	ctrlr.max_sges = NVME_RDMA_MAX_SGL_DESCRIPTORS;
	ctrlr.cdata.nvmf_specific.msdbd = 16;
	ctrlr.ioccsz_bytes = 4096;

	rqpair.mr_map = (struct spdk_rdma_mem_map *)0xdeadbeef;
	rqpair.rdma_qp = (struct spdk_rdma_qp *)0xdeadbeef;
	rqpair.qpair.ctrlr = &ctrlr;
	rqpair.cmds = &cmd;
	cmd.sgl[0].address = 0x1111;
	rdma_req.id = 0;
	rdma_req.req = &req;

	req.payload.reset_sgl_fn = nvme_rdma_ut_reset_sgl;
	req.payload.next_sge_fn = nvme_rdma_ut_next_sge;
	req.payload.contig_or_cb_arg = &bio;
	req.qpair = &rqpair.qpair;

	for (i = 0; i < NVME_RDMA_MAX_SGL_DESCRIPTORS; i++) {
		bio.iovs[i].iov_base = (void *)i;
		bio.iovs[i].iov_len = 0;
	}

	/* Test case 1: single SGL. Expected: PASS */
	bio.iovpos = 0;
	req.payload_offset = 0;
	req.payload_size = 0x1000;
	bio.iovs[0].iov_len = 0x1000;
	rc = nvme_rdma_build_sgl_request(&rqpair, &rdma_req);
	SPDK_CU_ASSERT_FATAL(rc == 0);
	CU_ASSERT(bio.iovpos == 1);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.type == SPDK_NVME_SGL_TYPE_KEYED_DATA_BLOCK);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.subtype == SPDK_NVME_SGL_SUBTYPE_ADDRESS);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.length == req.payload_size);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.key == RDMA_UT_RKEY);
	CU_ASSERT(req.cmd.dptr.sgl1.address == (uint64_t)bio.iovs[0].iov_base);
	CU_ASSERT(rdma_req.send_sgl[0].length == sizeof(struct spdk_nvme_cmd));

	/* Test case 2: multiple SGL. Expected: PASS */
	bio.iovpos = 0;
	req.payload_offset = 0;
	req.payload_size = 0x4000;
	for (i = 0; i < 4; i++) {
		bio.iovs[i].iov_len = 0x1000;
	}
	rc = nvme_rdma_build_sgl_request(&rqpair, &rdma_req);
	SPDK_CU_ASSERT_FATAL(rc == 0);
	CU_ASSERT(bio.iovpos == 4);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.type == SPDK_NVME_SGL_TYPE_LAST_SEGMENT);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.subtype == SPDK_NVME_SGL_SUBTYPE_OFFSET);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.length == 4 * sizeof(struct spdk_nvme_sgl_descriptor));
	CU_ASSERT(req.cmd.dptr.sgl1.address == (uint64_t)0);
	CU_ASSERT(rdma_req.send_sgl[0].length == 4 * sizeof(struct spdk_nvme_sgl_descriptor) + sizeof(
			  struct spdk_nvme_cmd))
	for (i = 0; i < 4; i++) {
		CU_ASSERT(cmd.sgl[i].keyed.type == SPDK_NVME_SGL_TYPE_KEYED_DATA_BLOCK);
		CU_ASSERT(cmd.sgl[i].keyed.subtype == SPDK_NVME_SGL_SUBTYPE_ADDRESS);
		CU_ASSERT(cmd.sgl[i].keyed.length == bio.iovs[i].iov_len);
		CU_ASSERT(cmd.sgl[i].keyed.key == RDMA_UT_RKEY);
		CU_ASSERT(cmd.sgl[i].address == (uint64_t)bio.iovs[i].iov_base);
	}

	/* Test case 3: Multiple SGL, SGL 2X mr size. Expected: FAIL */
	bio.iovpos = 0;
	req.payload_offset = 0;
	g_mr_size = 0x800;
	rc = nvme_rdma_build_sgl_request(&rqpair, &rdma_req);
	SPDK_CU_ASSERT_FATAL(rc != 0);
	CU_ASSERT(bio.iovpos == 1);

	/* Test case 4: Multiple SGL, SGL size smaller than I/O size. Expected: FAIL */
	bio.iovpos = 0;
	req.payload_offset = 0;
	req.payload_size = 0x6000;
	g_mr_size = 0x0;
	rc = nvme_rdma_build_sgl_request(&rqpair, &rdma_req);
	SPDK_CU_ASSERT_FATAL(rc != 0);
	CU_ASSERT(bio.iovpos == NVME_RDMA_MAX_SGL_DESCRIPTORS);

	/* Test case 5: SGL length exceeds 3 bytes. Expected: FAIL */
	req.payload_size = 0x1000 + (1 << 24);
	bio.iovs[0].iov_len = 0x1000;
	bio.iovs[1].iov_len = 1 << 24;
	rc = nvme_rdma_build_sgl_request(&rqpair, &rdma_req);
	SPDK_CU_ASSERT_FATAL(rc != 0);

	/* Test case 6: 4 SGL descriptors, size of SGL descriptors exceeds ICD. Expected: FAIL */
	ctrlr.ioccsz_bytes = 60;
	bio.iovpos = 0;
	req.payload_offset = 0;
	req.payload_size = 0x4000;
	for (i = 0; i < 4; i++) {
		bio.iovs[i].iov_len = 0x1000;
	}
	rc = nvme_rdma_build_sgl_request(&rqpair, &rdma_req);
	SPDK_CU_ASSERT_FATAL(rc == -1);
}

static void
test_nvme_rdma_build_sgl_inline_request(void)
{
	struct nvme_rdma_qpair rqpair;
	struct spdk_nvme_ctrlr ctrlr = {0};
	struct spdk_nvmf_cmd cmd = {{0}};
	struct spdk_nvme_rdma_req rdma_req = {0};
	struct nvme_request req = {{0}};
	struct nvme_rdma_ut_bdev_io bio;
	int rc;

	ctrlr.max_sges = NVME_RDMA_MAX_SGL_DESCRIPTORS;
	ctrlr.cdata.nvmf_specific.msdbd = 16;

	rqpair.mr_map = (struct spdk_rdma_mem_map *)0xdeadbeef;
	rqpair.rdma_qp = (struct spdk_rdma_qp *)0xdeadbeef;
	rqpair.qpair.ctrlr = &ctrlr;
	rqpair.cmds = &cmd;
	cmd.sgl[0].address = 0x1111;
	rdma_req.id = 0;
	rdma_req.req = &req;

	req.payload.reset_sgl_fn = nvme_rdma_ut_reset_sgl;
	req.payload.next_sge_fn = nvme_rdma_ut_next_sge;
	req.payload.contig_or_cb_arg = &bio;
	req.qpair = &rqpair.qpair;

	/* Test case 1: single inline SGL. Expected: PASS */
	bio.iovpos = 0;
	req.payload_offset = 0;
	req.payload_size = 0x1000;
	bio.iovs[0].iov_base = (void *)0xdeadbeef;
	bio.iovs[0].iov_len = 0x1000;
	rc = nvme_rdma_build_sgl_inline_request(&rqpair, &rdma_req);
	SPDK_CU_ASSERT_FATAL(rc == 0);
	CU_ASSERT(bio.iovpos == 1);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.type == SPDK_NVME_SGL_TYPE_DATA_BLOCK);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.subtype == SPDK_NVME_SGL_SUBTYPE_OFFSET);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.length == req.payload_size);
	CU_ASSERT(req.cmd.dptr.sgl1.address == 0);
	CU_ASSERT(rdma_req.send_sgl[0].length == sizeof(struct spdk_nvme_cmd));
	CU_ASSERT(rdma_req.send_sgl[1].length == req.payload_size);
	CU_ASSERT(rdma_req.send_sgl[1].addr == (uint64_t)bio.iovs[0].iov_base);
	CU_ASSERT(rdma_req.send_sgl[1].lkey == RDMA_UT_LKEY);

	/* Test case 2: SGL length exceeds 3 bytes. Expected: PASS */
	bio.iovpos = 0;
	req.payload_offset = 0;
	req.payload_size = 1 << 24;
	bio.iovs[0].iov_len = 1 << 24;
	rc = nvme_rdma_build_sgl_inline_request(&rqpair, &rdma_req);
	SPDK_CU_ASSERT_FATAL(rc == 0);
	CU_ASSERT(bio.iovpos == 1);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.type == SPDK_NVME_SGL_TYPE_DATA_BLOCK);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.subtype == SPDK_NVME_SGL_SUBTYPE_OFFSET);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.length == req.payload_size);
	CU_ASSERT(req.cmd.dptr.sgl1.address == 0);
	CU_ASSERT(rdma_req.send_sgl[0].length == sizeof(struct spdk_nvme_cmd));
	CU_ASSERT(rdma_req.send_sgl[1].length == req.payload_size);
	CU_ASSERT(rdma_req.send_sgl[1].addr == (uint64_t)bio.iovs[0].iov_base);
	CU_ASSERT(rdma_req.send_sgl[1].lkey == RDMA_UT_LKEY);
}

static void
test_nvme_rdma_build_contig_request(void)
{
	struct nvme_rdma_qpair rqpair;
	struct spdk_nvme_ctrlr ctrlr = {0};
	struct spdk_nvmf_cmd cmd = {{0}};
	struct spdk_nvme_rdma_req rdma_req = {0};
	struct nvme_request req = {{0}};
	int rc;

	ctrlr.max_sges = NVME_RDMA_MAX_SGL_DESCRIPTORS;
	ctrlr.cdata.nvmf_specific.msdbd = 16;

	rqpair.mr_map = (struct spdk_rdma_mem_map *)0xdeadbeef;
	rqpair.rdma_qp = (struct spdk_rdma_qp *)0xdeadbeef;
	rqpair.qpair.ctrlr = &ctrlr;
	rqpair.cmds = &cmd;
	cmd.sgl[0].address = 0x1111;
	rdma_req.id = 0;
	rdma_req.req = &req;

	req.payload.contig_or_cb_arg = (void *)0xdeadbeef;
	req.qpair = &rqpair.qpair;

	/* Test case 1: contig request. Expected: PASS */
	req.payload_offset = 0;
	req.payload_size = 0x1000;
	rc = nvme_rdma_build_contig_request(&rqpair, &rdma_req);
	SPDK_CU_ASSERT_FATAL(rc == 0);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.type == SPDK_NVME_SGL_TYPE_KEYED_DATA_BLOCK);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.subtype == SPDK_NVME_SGL_SUBTYPE_ADDRESS);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.length == req.payload_size);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.key == RDMA_UT_RKEY);
	CU_ASSERT(req.cmd.dptr.sgl1.address == (uint64_t)req.payload.contig_or_cb_arg);
	CU_ASSERT(rdma_req.send_sgl[0].length == sizeof(struct spdk_nvme_cmd));

	/* Test case 2: SGL length exceeds 3 bytes. Expected: FAIL */
	req.payload_offset = 0;
	req.payload_size = 1 << 24;
	rc = nvme_rdma_build_contig_request(&rqpair, &rdma_req);
	SPDK_CU_ASSERT_FATAL(rc != 0);
}

static void
test_nvme_rdma_build_contig_inline_request(void)
{
	struct nvme_rdma_qpair rqpair;
	struct spdk_nvme_ctrlr ctrlr = {0};
	struct spdk_nvmf_cmd cmd = {{0}};
	struct spdk_nvme_rdma_req rdma_req = {0};
	struct nvme_request req = {{0}};
	int rc;

	ctrlr.max_sges = NVME_RDMA_MAX_SGL_DESCRIPTORS;
	ctrlr.cdata.nvmf_specific.msdbd = 16;

	rqpair.mr_map = (struct spdk_rdma_mem_map *)0xdeadbeef;
	rqpair.rdma_qp = (struct spdk_rdma_qp *)0xdeadbeef;
	rqpair.qpair.ctrlr = &ctrlr;
	rqpair.cmds = &cmd;
	cmd.sgl[0].address = 0x1111;
	rdma_req.id = 0;
	rdma_req.req = &req;

	req.payload.contig_or_cb_arg = (void *)0xdeadbeef;
	req.qpair = &rqpair.qpair;

	/* Test case 1: single inline SGL. Expected: PASS */
	req.payload_offset = 0;
	req.payload_size = 0x1000;
	rc = nvme_rdma_build_contig_inline_request(&rqpair, &rdma_req);
	SPDK_CU_ASSERT_FATAL(rc == 0);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.type == SPDK_NVME_SGL_TYPE_DATA_BLOCK);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.subtype == SPDK_NVME_SGL_SUBTYPE_OFFSET);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.length == req.payload_size);
	CU_ASSERT(req.cmd.dptr.sgl1.address == 0);
	CU_ASSERT(rdma_req.send_sgl[0].length == sizeof(struct spdk_nvme_cmd));
	CU_ASSERT(rdma_req.send_sgl[1].length == req.payload_size);
	CU_ASSERT(rdma_req.send_sgl[1].addr == (uint64_t)req.payload.contig_or_cb_arg);
	CU_ASSERT(rdma_req.send_sgl[1].lkey == RDMA_UT_LKEY);

	/* Test case 2: SGL length exceeds 3 bytes. Expected: PASS */
	req.payload_offset = 0;
	req.payload_size = 1 << 24;
	rc = nvme_rdma_build_contig_inline_request(&rqpair, &rdma_req);
	SPDK_CU_ASSERT_FATAL(rc == 0);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.type == SPDK_NVME_SGL_TYPE_DATA_BLOCK);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.subtype == SPDK_NVME_SGL_SUBTYPE_OFFSET);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.length == req.payload_size);
	CU_ASSERT(req.cmd.dptr.sgl1.address == 0);
	CU_ASSERT(rdma_req.send_sgl[0].length == sizeof(struct spdk_nvme_cmd));
	CU_ASSERT(rdma_req.send_sgl[1].length == req.payload_size);
	CU_ASSERT(rdma_req.send_sgl[1].addr == (uint64_t)req.payload.contig_or_cb_arg);
	CU_ASSERT(rdma_req.send_sgl[1].lkey == RDMA_UT_LKEY);
}

static void
test_nvme_rdma_alloc_reqs(void)
{
	struct nvme_rdma_qpair rqpair = {};
	int rc;

	memset(&g_nvme_hooks, 0, sizeof(g_nvme_hooks));

	/* Test case 1: zero entry. Expect: FAIL */
	rqpair.num_entries = 0;

	rc = nvme_rdma_alloc_reqs(&rqpair);
	CU_ASSERT(rqpair.rdma_reqs == NULL);
	SPDK_CU_ASSERT_FATAL(rc == -ENOMEM);

	/* Test case 2: single entry. Expect: PASS */
	memset(&rqpair, 0, sizeof(rqpair));
	rqpair.num_entries = 1;

	rc = nvme_rdma_alloc_reqs(&rqpair);
	CU_ASSERT(rc == 0);
	CU_ASSERT(rqpair.rdma_reqs[0].send_sgl[0].addr
		  == (uint64_t)&rqpair.cmds[0]);
	CU_ASSERT(rqpair.rdma_reqs[0].send_wr.wr_id
		  == (uint64_t)&rqpair.rdma_reqs[0].rdma_wr);
	CU_ASSERT(rqpair.rdma_reqs[0].send_wr.next == NULL);
	CU_ASSERT(rqpair.rdma_reqs[0].send_wr.opcode == IBV_WR_SEND);
	CU_ASSERT(rqpair.rdma_reqs[0].send_wr.send_flags == IBV_SEND_SIGNALED);
	CU_ASSERT(rqpair.rdma_reqs[0].send_wr.sg_list
		  == rqpair.rdma_reqs[0].send_sgl);
	CU_ASSERT(rqpair.rdma_reqs[0].send_wr.imm_data == 0);
	spdk_free(rqpair.rdma_reqs);
	spdk_free(rqpair.cmds);

	/* Test case 3: multiple entries. Expect: PASS */
	memset(&rqpair, 0, sizeof(rqpair));
	rqpair.num_entries = 5;

	rc = nvme_rdma_alloc_reqs(&rqpair);
	CU_ASSERT(rc == 0);
	for (int i = 0; i < 5; i++) {
		CU_ASSERT(rqpair.rdma_reqs[i].send_sgl[0].addr
			  == (uint64_t)&rqpair.cmds[i]);
		CU_ASSERT(rqpair.rdma_reqs[i].send_wr.wr_id
			  == (uint64_t)&rqpair.rdma_reqs[i].rdma_wr);
		CU_ASSERT(rqpair.rdma_reqs[i].send_wr.next == NULL);
		CU_ASSERT(rqpair.rdma_reqs[i].send_wr.opcode == IBV_WR_SEND);
		CU_ASSERT(rqpair.rdma_reqs[i].send_wr.send_flags
			  == IBV_SEND_SIGNALED);
		CU_ASSERT(rqpair.rdma_reqs[i].send_wr.sg_list
			  == rqpair.rdma_reqs[i].send_sgl);
		CU_ASSERT(rqpair.rdma_reqs[i].send_wr.imm_data == 0);
	}
	spdk_free(rqpair.rdma_reqs);
	spdk_free(rqpair.cmds);
}

static void
test_nvme_rdma_alloc_rsps(void)
{
	struct nvme_rdma_qpair rqpair = {};
	int rc;

	memset(&g_nvme_hooks, 0, sizeof(g_nvme_hooks));

	/* Test case 1 calloc false */
	rqpair.num_entries = 0;
	rc = nvme_rdma_alloc_rsps(&rqpair);
	CU_ASSERT(rqpair.rsp_sgls == NULL);
	SPDK_CU_ASSERT_FATAL(rc == -ENOMEM);

	/* Test case 2 calloc success */
	memset(&rqpair, 0, sizeof(rqpair));
	rqpair.num_entries = 1;

	rc = nvme_rdma_alloc_rsps(&rqpair);
	CU_ASSERT(rc == 0);
	CU_ASSERT(rqpair.rsp_sgls != NULL);
	CU_ASSERT(rqpair.rsp_recv_wrs != NULL);
	CU_ASSERT(rqpair.rsps != NULL);
	nvme_rdma_free_rsps(&rqpair);
}

static void
test_nvme_rdma_ctrlr_create_qpair(void)
{
	struct spdk_nvme_ctrlr ctrlr = {};
	uint16_t qid, qsize;
	struct spdk_nvme_qpair *qpair;
	struct nvme_rdma_qpair *rqpair;

	/* Test case 1: max qsize. Expect: PASS */
	qsize = 0xffff;
	qid = 1;

	qpair = nvme_rdma_ctrlr_create_qpair(&ctrlr, qid, qsize,
					     SPDK_NVME_QPRIO_URGENT, 1,
					     false);
	CU_ASSERT(qpair != NULL);
	rqpair = SPDK_CONTAINEROF(qpair, struct nvme_rdma_qpair, qpair);
	CU_ASSERT(qpair == &rqpair->qpair);
	CU_ASSERT(rqpair->num_entries == qsize);
	CU_ASSERT(rqpair->delay_cmd_submit == false);
	CU_ASSERT(rqpair->rsp_sgls != NULL);
	CU_ASSERT(rqpair->rsp_recv_wrs != NULL);
	CU_ASSERT(rqpair->rsps != NULL);

	nvme_rdma_free_reqs(rqpair);
	nvme_rdma_free_rsps(rqpair);
	nvme_rdma_free(rqpair);
	rqpair = NULL;

	/* Test case 2: queue qsize zero. ExpectL FAIL */
	qsize = 0;

	qpair = nvme_rdma_ctrlr_create_qpair(&ctrlr, qid, qsize,
					     SPDK_NVME_QPRIO_URGENT, 1,
					     false);
	SPDK_CU_ASSERT_FATAL(qpair == NULL);
}

DEFINE_STUB(ibv_create_cq, struct ibv_cq *, (struct ibv_context *context, int cqe, void *cq_context,
		struct ibv_comp_channel *channel, int comp_vector), (struct ibv_cq *)0xFEEDBEEF);
DEFINE_STUB(ibv_destroy_cq, int, (struct ibv_cq *cq), 0);

static void
test_nvme_rdma_poller_create(void)
{
	struct nvme_rdma_poll_group	group = {};
	struct ibv_context *contexts = (struct ibv_context *)0xDEADBEEF;

	/* Case: calloc and ibv not need to fail test */
	STAILQ_INIT(&group.pollers);
	group.num_pollers = 1;
	int rc = nvme_rdma_poller_create(&group, contexts);

	CU_ASSERT(rc == 0);
	CU_ASSERT(group.num_pollers = 2);
	CU_ASSERT(&group.pollers != NULL);
	CU_ASSERT(group.pollers.stqh_first->device == contexts);
	CU_ASSERT(group.pollers.stqh_first->cq == (struct ibv_cq *)0xFEEDBEEF);
	CU_ASSERT(group.pollers.stqh_first->current_num_wc == DEFAULT_NVME_RDMA_CQ_SIZE);
	CU_ASSERT(group.pollers.stqh_first->required_num_wc == 0);

	nvme_rdma_poll_group_free_pollers(&group);
}

static void
test_nvme_rdma_qpair_process_cm_event(void)
{
	struct nvme_rdma_qpair rqpair = {};
	struct rdma_cm_event	 event = {};
	struct spdk_nvmf_rdma_accept_private_data	accept_data = {};
	int rc = 0;

	/* case1: event == RDMA_CM_EVENT_ADDR_RESOLVED */
	rqpair.evt = &event;
	event.event = RDMA_CM_EVENT_ADDR_RESOLVED;
	rc = nvme_rdma_qpair_process_cm_event(&rqpair);
	CU_ASSERT(rc == 0);

	/* case2: event == RDMA_CM_EVENT_CONNECT_REQUEST */
	rqpair.evt = &event;
	event.event = RDMA_CM_EVENT_CONNECT_REQUEST;
	rc = nvme_rdma_qpair_process_cm_event(&rqpair);
	CU_ASSERT(rc == 0);

	/* case3: event == RDMA_CM_EVENT_CONNECT_ERROR */
	rqpair.evt = &event;
	event.event = RDMA_CM_EVENT_CONNECT_ERROR;
	rc = nvme_rdma_qpair_process_cm_event(&rqpair);
	CU_ASSERT(rc == 0);

	/* case4: event == RDMA_CM_EVENT_UNREACHABLE */
	rqpair.evt = &event;
	event.event = RDMA_CM_EVENT_UNREACHABLE;
	rc = nvme_rdma_qpair_process_cm_event(&rqpair);
	CU_ASSERT(rc == 0);

	/* case5: event == RDMA_CM_EVENT_CONNECT_RESPONSE */
	rqpair.evt = &event;
	event.event = RDMA_CM_EVENT_CONNECT_RESPONSE;
	event.param.conn.private_data = NULL;
	rc = nvme_rdma_qpair_process_cm_event(&rqpair);
	CU_ASSERT(rc == -1);

	rqpair.evt = &event;
	event.event = RDMA_CM_EVENT_CONNECT_RESPONSE;
	event.param.conn.private_data = &accept_data;
	accept_data.crqsize = 512;
	rqpair.num_entries = 1024;
	rc = nvme_rdma_qpair_process_cm_event(&rqpair);
	CU_ASSERT(rc == 0);
	CU_ASSERT(rqpair.num_entries == 512);

	/* case6: event == RDMA_CM_EVENT_DISCONNECTED */
	rqpair.evt = &event;
	event.event = RDMA_CM_EVENT_DISCONNECTED;
	rc = nvme_rdma_qpair_process_cm_event(&rqpair);
	CU_ASSERT(rc == 0);
	CU_ASSERT(rqpair.qpair.transport_failure_reason == SPDK_NVME_QPAIR_FAILURE_REMOTE);

	/* case7: event == RDMA_CM_EVENT_DEVICE_REMOVAL */
	rqpair.evt = &event;
	event.event = RDMA_CM_EVENT_DEVICE_REMOVAL;
	rc = nvme_rdma_qpair_process_cm_event(&rqpair);
	CU_ASSERT(rc == 0);
	CU_ASSERT(rqpair.qpair.transport_failure_reason == SPDK_NVME_QPAIR_FAILURE_LOCAL);

	/* case8: event == RDMA_CM_EVENT_MULTICAST_JOIN */
	rqpair.evt = &event;
	event.event = RDMA_CM_EVENT_MULTICAST_JOIN;
	rc = nvme_rdma_qpair_process_cm_event(&rqpair);
	CU_ASSERT(rc == 0);

	/* case9: event == RDMA_CM_EVENT_ADDR_CHANGE */
	rqpair.evt = &event;
	event.event = RDMA_CM_EVENT_ADDR_CHANGE;
	rc = nvme_rdma_qpair_process_cm_event(&rqpair);
	CU_ASSERT(rc == 0);
	CU_ASSERT(rqpair.qpair.transport_failure_reason == SPDK_NVME_QPAIR_FAILURE_LOCAL);

	/* case10: event == RDMA_CM_EVENT_TIMEWAIT_EXIT */
	rqpair.evt = &event;
	event.event = RDMA_CM_EVENT_TIMEWAIT_EXIT;
	rc = nvme_rdma_qpair_process_cm_event(&rqpair);
	CU_ASSERT(rc == 0);

	/* case11: default event == 0xFF */
	rqpair.evt = &event;
	event.event = 0xFF;
	rc = nvme_rdma_qpair_process_cm_event(&rqpair);
	CU_ASSERT(rc == 0);
}

static void
test_nvme_rdma_mr_get_lkey(void)
{
	union nvme_rdma_mr mr = {};
	struct ibv_mr	ibv_mr = {};
	uint64_t mr_key;
	uint32_t lkey;

	memset(&g_nvme_hooks, 0, sizeof(g_nvme_hooks));
	ibv_mr.lkey = 1;
	mr_key = 2;

	/* Case 1:  get key form key address */
	mr.key = (uint64_t)&mr_key;
	g_nvme_hooks.get_rkey = (void *)0xAEADBEEF;

	lkey = nvme_rdma_mr_get_lkey(&mr);
	CU_ASSERT(lkey == mr_key);

	/* Case 2: Get key from ibv_mr  */
	g_nvme_hooks.get_rkey = NULL;
	mr.mr = &ibv_mr;

	lkey = nvme_rdma_mr_get_lkey(&mr);
	CU_ASSERT(lkey == ibv_mr.lkey);
}

static void
test_nvme_rdma_ctrlr_construct(void)
{
	struct spdk_nvme_ctrlr *ctrlr;
	struct spdk_nvme_transport_id trid = {};
	struct spdk_nvme_ctrlr_opts opts = {};
	struct nvme_rdma_qpair *rqpair = NULL;
	struct nvme_rdma_ctrlr *rctrlr = NULL;
	struct rdma_event_channel cm_channel = {};
	void *devhandle = NULL;
	int rc;

	opts.transport_retry_count = NVME_RDMA_CTRLR_MAX_TRANSPORT_RETRY_COUNT + 1;
	opts.transport_ack_timeout = NVME_RDMA_CTRLR_MAX_TRANSPORT_ACK_TIMEOUT + 1;
	opts.admin_queue_size = 0xFFFF;
	trid.trtype = SPDK_NVME_TRANSPORT_RDMA;
	trid.adrfam = SPDK_NVMF_ADRFAM_IPV4;
	MOCK_SET(rdma_create_event_channel, &cm_channel);

	ctrlr = nvme_rdma_ctrlr_construct(&trid, &opts, devhandle);
	SPDK_CU_ASSERT_FATAL(ctrlr != NULL);
	CU_ASSERT(ctrlr->opts.transport_retry_count ==
		  NVME_RDMA_CTRLR_MAX_TRANSPORT_RETRY_COUNT);
	CU_ASSERT(ctrlr->opts.transport_ack_timeout ==
		  NVME_RDMA_CTRLR_MAX_TRANSPORT_ACK_TIMEOUT);
	CU_ASSERT(ctrlr->opts.admin_queue_size == opts.admin_queue_size);
	rctrlr = SPDK_CONTAINEROF(ctrlr, struct nvme_rdma_ctrlr, ctrlr);
	CU_ASSERT(rctrlr->max_sge == NVME_RDMA_MAX_SGL_DESCRIPTORS);
	CU_ASSERT(rctrlr->cm_channel == &cm_channel);
	CU_ASSERT(!strncmp((char *)&rctrlr->ctrlr.trid,
			   (char *)&trid, sizeof(trid)));

	SPDK_CU_ASSERT_FATAL(ctrlr->adminq != NULL);
	rqpair = SPDK_CONTAINEROF(ctrlr->adminq, struct nvme_rdma_qpair, qpair);
	CU_ASSERT(rqpair->num_entries == opts.admin_queue_size);
	CU_ASSERT(rqpair->delay_cmd_submit == false);
	CU_ASSERT(rqpair->rsp_sgls != NULL);
	CU_ASSERT(rqpair->rsp_recv_wrs != NULL);
	CU_ASSERT(rqpair->rsps != NULL);
	MOCK_CLEAR(rdma_create_event_channel);

	/* Hardcode the trtype, because nvme_qpair_init() is stub function. */
	rqpair->qpair.trtype = SPDK_NVME_TRANSPORT_RDMA;
	rc = nvme_rdma_ctrlr_destruct(ctrlr);
	CU_ASSERT(rc == 0);
}

static void
test_nvme_rdma_req_put_and_get(void)
{
	struct nvme_rdma_qpair rqpair = {};
	struct spdk_nvme_rdma_req rdma_req = {};
	struct spdk_nvme_rdma_req *rdma_req_get;

	/* case 1: nvme_rdma_req_put */
	TAILQ_INIT(&rqpair.free_reqs);
	rdma_req.completion_flags = 1;
	rdma_req.req = (struct nvme_request *)0xDEADBEFF;
	rdma_req.id = 10086;
	nvme_rdma_req_put(&rqpair, &rdma_req);

	CU_ASSERT(rqpair.free_reqs.tqh_first == &rdma_req);
	CU_ASSERT(rqpair.free_reqs.tqh_first->completion_flags == 0);
	CU_ASSERT(rqpair.free_reqs.tqh_first->req == NULL);
	CU_ASSERT(rqpair.free_reqs.tqh_first->id == 10086);
	CU_ASSERT(rdma_req.completion_flags == 0);
	CU_ASSERT(rdma_req.req == NULL);

	/* case 2: nvme_rdma_req_get */
	TAILQ_INIT(&rqpair.outstanding_reqs);
	rdma_req_get = nvme_rdma_req_get(&rqpair);
	CU_ASSERT(rdma_req_get == &rdma_req);
	CU_ASSERT(rdma_req_get->id == 10086);
	CU_ASSERT(rqpair.free_reqs.tqh_first == NULL);
	CU_ASSERT(rqpair.outstanding_reqs.tqh_first == rdma_req_get);
}

static void
test_nvme_rdma_req_init(void)
{
	struct nvme_rdma_qpair rqpair = {};
	struct spdk_nvme_ctrlr ctrlr = {};
	struct spdk_nvmf_cmd cmd = {};
	struct spdk_nvme_rdma_req rdma_req = {};
	struct nvme_request req = {};
	struct nvme_rdma_ut_bdev_io bio = {};
	int rc = 1;

	ctrlr.max_sges = NVME_RDMA_MAX_SGL_DESCRIPTORS;
	ctrlr.cdata.nvmf_specific.msdbd = 16;

	rqpair.mr_map = (struct spdk_rdma_mem_map *)0xdeadbeef;
	rqpair.rdma_qp = (struct spdk_rdma_qp *)0xdeadbeef;
	rqpair.qpair.ctrlr = &ctrlr;
	rqpair.cmds = &cmd;
	cmd.sgl[0].address = 0x1111;
	rdma_req.id = 0;
	req.cmd.opc = SPDK_NVME_DATA_HOST_TO_CONTROLLER;

	req.payload.contig_or_cb_arg = (void *)0xdeadbeef;
	/* case 1: req->payload_size == 0, expect: pass. */
	req.payload_size = 0;
	rqpair.qpair.ctrlr->ioccsz_bytes = 1024;
	rqpair.qpair.ctrlr->icdoff = 0;
	rc = nvme_rdma_req_init(&rqpair, &req, &rdma_req);
	CU_ASSERT(rc == 0);
	CU_ASSERT(req.cmd.psdt == SPDK_NVME_PSDT_SGL_MPTR_CONTIG);
	CU_ASSERT(rdma_req.send_sgl[0].length == sizeof(struct spdk_nvme_cmd));
	CU_ASSERT(rdma_req.send_wr.num_sge == 1);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.type == SPDK_NVME_SGL_TYPE_KEYED_DATA_BLOCK);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.subtype == SPDK_NVME_SGL_SUBTYPE_ADDRESS);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.length == 0);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.key == 0);
	CU_ASSERT(req.cmd.dptr.sgl1.address == 0);

	/* case 2: payload_type == NVME_PAYLOAD_TYPE_CONTIG, expect: pass. */
	/* icd_supported is true */
	rdma_req.req = NULL;
	rqpair.qpair.ctrlr->icdoff = 0;
	req.payload_offset = 0;
	req.payload_size = 1024;
	req.payload.reset_sgl_fn = NULL;
	rc = nvme_rdma_req_init(&rqpair, &req, &rdma_req);
	CU_ASSERT(rc == 0);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.type == SPDK_NVME_SGL_TYPE_DATA_BLOCK);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.subtype == SPDK_NVME_SGL_SUBTYPE_OFFSET);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.length == req.payload_size);
	CU_ASSERT(req.cmd.dptr.sgl1.address == 0);
	CU_ASSERT(rdma_req.send_sgl[0].length == sizeof(struct spdk_nvme_cmd));
	CU_ASSERT(rdma_req.send_sgl[1].length == req.payload_size);
	CU_ASSERT(rdma_req.send_sgl[1].addr == (uint64_t)req.payload.contig_or_cb_arg);
	CU_ASSERT(rdma_req.send_sgl[1].lkey == RDMA_UT_LKEY);

	/* icd_supported is false */
	rdma_req.req = NULL;
	rqpair.qpair.ctrlr->icdoff = 1;
	req.payload_offset = 0;
	req.payload_size = 1024;
	req.payload.reset_sgl_fn = NULL;
	rc = nvme_rdma_req_init(&rqpair, &req, &rdma_req);
	CU_ASSERT(rc == 0);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.type == SPDK_NVME_SGL_TYPE_KEYED_DATA_BLOCK);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.subtype == SPDK_NVME_SGL_SUBTYPE_ADDRESS);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.length == req.payload_size);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.key == RDMA_UT_RKEY);
	CU_ASSERT(req.cmd.dptr.sgl1.address == (uint64_t)req.payload.contig_or_cb_arg);
	CU_ASSERT(rdma_req.send_sgl[0].length == sizeof(struct spdk_nvme_cmd));

	/* case 3: payload_type == NVME_PAYLOAD_TYPE_SGL, expect: pass. */
	/* icd_supported is true */
	rdma_req.req = NULL;
	rqpair.qpair.ctrlr->icdoff = 0;
	req.payload.reset_sgl_fn = nvme_rdma_ut_reset_sgl;
	req.payload.next_sge_fn = nvme_rdma_ut_next_sge;
	req.payload.contig_or_cb_arg = &bio;
	req.qpair = &rqpair.qpair;
	bio.iovpos = 0;
	req.payload_offset = 0;
	req.payload_size = 1024;
	bio.iovs[0].iov_base = (void *)0xdeadbeef;
	bio.iovs[0].iov_len = 1024;
	rc = nvme_rdma_req_init(&rqpair, &req, &rdma_req);
	CU_ASSERT(rc == 0);
	CU_ASSERT(bio.iovpos == 1);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.type == SPDK_NVME_SGL_TYPE_DATA_BLOCK);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.subtype == SPDK_NVME_SGL_SUBTYPE_OFFSET);
	CU_ASSERT(req.cmd.dptr.sgl1.unkeyed.length == req.payload_size);
	CU_ASSERT(req.cmd.dptr.sgl1.address == 0);
	CU_ASSERT(rdma_req.send_sgl[0].length == sizeof(struct spdk_nvme_cmd));
	CU_ASSERT(rdma_req.send_sgl[1].length == req.payload_size);
	CU_ASSERT(rdma_req.send_sgl[1].addr == (uint64_t)bio.iovs[0].iov_base);
	CU_ASSERT(rdma_req.send_sgl[1].lkey == RDMA_UT_LKEY);

	/* icd_supported is false */
	rdma_req.req = NULL;
	rqpair.qpair.ctrlr->icdoff = 1;
	req.payload.reset_sgl_fn = nvme_rdma_ut_reset_sgl;
	req.payload.next_sge_fn = nvme_rdma_ut_next_sge;
	req.payload.contig_or_cb_arg = &bio;
	req.qpair = &rqpair.qpair;
	bio.iovpos = 0;
	req.payload_offset = 0;
	req.payload_size = 1024;
	bio.iovs[0].iov_base = (void *)0xdeadbeef;
	bio.iovs[0].iov_len = 1024;
	rc = nvme_rdma_req_init(&rqpair, &req, &rdma_req);
	CU_ASSERT(rc == 0);
	CU_ASSERT(bio.iovpos == 1);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.type == SPDK_NVME_SGL_TYPE_KEYED_DATA_BLOCK);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.subtype == SPDK_NVME_SGL_SUBTYPE_ADDRESS);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.length == req.payload_size);
	CU_ASSERT(req.cmd.dptr.sgl1.keyed.key == RDMA_UT_RKEY);
	CU_ASSERT(req.cmd.dptr.sgl1.address == (uint64_t)bio.iovs[0].iov_base);
	CU_ASSERT(rdma_req.send_sgl[0].length == sizeof(struct spdk_nvme_cmd));
}

static void
test_nvme_rdma_validate_cm_event(void)
{
	enum rdma_cm_event_type expected_evt_type;
	struct rdma_cm_event reaped_evt = {};
	int rc;

	/* case 1: expected_evt_type == reaped_evt->event, expect: pass */
	expected_evt_type = RDMA_CM_EVENT_ADDR_RESOLVED;
	reaped_evt.event = RDMA_CM_EVENT_ADDR_RESOLVED;

	rc = nvme_rdma_validate_cm_event(expected_evt_type, &reaped_evt);
	CU_ASSERT(rc == 0);

	/* case 2: expected_evt_type != RDMA_CM_EVENT_ESTABLISHED and is not equal to reaped_evt->event, expect: fail */
	reaped_evt.event = RDMA_CM_EVENT_CONNECT_RESPONSE;

	rc = nvme_rdma_validate_cm_event(expected_evt_type, &reaped_evt);
	CU_ASSERT(rc == -EBADMSG);

	/* case 3: expected_evt_type == RDMA_CM_EVENT_ESTABLISHED */
	expected_evt_type = RDMA_CM_EVENT_ESTABLISHED;
	/* reaped_evt->event == RDMA_CM_EVENT_REJECTED and reaped_evt->status == 10, expect: fail */
	reaped_evt.event = RDMA_CM_EVENT_REJECTED;
	reaped_evt.status = 10;

	rc = nvme_rdma_validate_cm_event(expected_evt_type, &reaped_evt);
	CU_ASSERT(rc == -ESTALE);

	/* reaped_evt->event == RDMA_CM_EVENT_CONNECT_RESPONSE, expect: pass */
	reaped_evt.event = RDMA_CM_EVENT_CONNECT_RESPONSE;

	rc = nvme_rdma_validate_cm_event(expected_evt_type, &reaped_evt);
	CU_ASSERT(rc == 0);
}

static void
test_nvme_rdma_register_and_unregister_reqs(void)
{
	struct nvme_rdma_qpair rqpair = {};
	struct spdk_nvmf_cmd cmds = {};
	struct rdma_cm_id cm_id = {};
	struct spdk_nvme_rdma_req rdma_reqs[50] = {};
	int rc;

	rqpair.cm_id = &cm_id;
	rqpair.cmds = &cmds;
	g_nvme_hooks.get_rkey = NULL;
	rqpair.rdma_reqs = rdma_reqs;
	/* case 1: nvme_rdma_register_req: nvme_rdma_reg_mr fail, expect: fail */
	rqpair.num_entries = 0;

	rc = nvme_rdma_register_reqs(&rqpair);
	CU_ASSERT(rc == -ENOMEM);
	CU_ASSERT(rqpair.cmd_mr.mr == NULL);

	/* case 2: nvme_rdma_register_req: single entry, expect: PASS */
	rqpair.num_entries = 1;

	rc = nvme_rdma_register_reqs(&rqpair);
	CU_ASSERT(rc == 0);
	CU_ASSERT(rqpair.cmd_mr.mr == &g_rdma_mr);
	CU_ASSERT(rqpair.rdma_reqs[0].send_sgl[0].lkey == rqpair.cmd_mr.mr->lkey);

	/* case 3: nvme_rdma_register_req: multiple entry, expect: PASS */
	rqpair.num_entries = 50;

	rc = nvme_rdma_register_reqs(&rqpair);
	CU_ASSERT(rc == 0);
	CU_ASSERT(rqpair.cmd_mr.mr == &g_rdma_mr);
	for (int i = 0; i < rqpair.num_entries; i++) {
		CU_ASSERT(rqpair.rdma_reqs[i].send_sgl[0].lkey == rqpair.cmd_mr.mr->lkey);
	}

	/* case4: nvme_rdma_unregister_reqs, expect: PASS */
	nvme_rdma_unregister_reqs(&rqpair);
	CU_ASSERT(rqpair.cmd_mr.mr == NULL);
}

int main(int argc, char **argv)
{
	CU_pSuite	suite = NULL;
	unsigned int	num_failures;

	CU_set_error_action(CUEA_ABORT);
	CU_initialize_registry();

	suite = CU_add_suite("nvme_rdma", NULL, NULL);
	CU_ADD_TEST(suite, test_nvme_rdma_build_sgl_request);
	CU_ADD_TEST(suite, test_nvme_rdma_build_sgl_inline_request);
	CU_ADD_TEST(suite, test_nvme_rdma_build_contig_request);
	CU_ADD_TEST(suite, test_nvme_rdma_build_contig_inline_request);
	CU_ADD_TEST(suite, test_nvme_rdma_alloc_reqs);
	CU_ADD_TEST(suite, test_nvme_rdma_alloc_rsps);
	CU_ADD_TEST(suite, test_nvme_rdma_ctrlr_create_qpair);
	CU_ADD_TEST(suite, test_nvme_rdma_poller_create);
	CU_ADD_TEST(suite, test_nvme_rdma_qpair_process_cm_event);
	CU_ADD_TEST(suite, test_nvme_rdma_mr_get_lkey);
	CU_ADD_TEST(suite, test_nvme_rdma_ctrlr_construct);
	CU_ADD_TEST(suite, test_nvme_rdma_req_put_and_get);
	CU_ADD_TEST(suite, test_nvme_rdma_req_init);
	CU_ADD_TEST(suite, test_nvme_rdma_validate_cm_event);
	CU_ADD_TEST(suite, test_nvme_rdma_register_and_unregister_reqs);

	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	num_failures = CU_get_number_of_failures();
	CU_cleanup_registry();
	return num_failures;
}
