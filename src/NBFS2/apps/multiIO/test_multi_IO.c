#include "spdk/stdinc.h"

#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/blob_bdev.h"
#include "spdk/blob.h"
#include "spdk/log.h"
#include "spdk/string.h"
#include <time.h>

#define N_CLUSTER 2
#define IO_NUM 4

struct timespec ts1, ts2;

struct test_multi_io_t
{
    struct spdk_blob_store *bs;
    char * bdev_name;
    struct spdk_blob *blob;
    spdk_blob_id blobid;
    struct spdk_io_channel *channel;
    uint8_t *read_buff;
    uint8_t *write_buff;
    uint64_t io_unit_size, cluster_size;
    int rc;
};

int fini_count =0;

static void
unload_complete(void *cb_arg, int bserrno)
{
    struct test_multi_io_t *test_multi_io = cb_arg;

    SPDK_NOTICELOG("entry\n");
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d unloading the bobstore\n", bserrno);
        test_multi_io->rc = bserrno;
    }

    spdk_app_stop(test_multi_io->rc);
    //free buffer
    spdk_free(test_multi_io->read_buff);
    spdk_free(test_multi_io->write_buff);
    free(test_multi_io);
}

static void
unload_bs(struct test_multi_io_t *test_multi_io, char *msg, int bserrno)
{
    if (bserrno)
    {
        SPDK_ERRLOG("%s (err %d)\n", msg, bserrno);
        test_multi_io->rc = bserrno;
    }
    if (test_multi_io->bs)
    {
        if (test_multi_io->channel)
        {
            spdk_bs_free_io_channel(test_multi_io->channel);
        }
        fini_count ++;
        if(fini_count == IO_NUM)
            spdk_bs_unload(test_multi_io->bs, unload_complete, test_multi_io);
    }
    else
    {
        spdk_app_stop(bserrno);
    }
}

static void
delete_complete(void *arg1, int bserrno)
{
    struct test_multi_io_t *test_multi_io = arg1;

    SPDK_NOTICELOG("entry\n");
    if (bserrno)
    {
        unload_bs(test_multi_io, "Error in delete completion",
                  bserrno);
        return;
    }

    unload_bs(test_multi_io, "", 0);
}

static void
delete_blob(void *arg1, int bserrno)
{
    struct test_multi_io_t *test_multi_io = arg1;

    SPDK_NOTICELOG("entry\n");
    if (bserrno)
    {
        unload_bs(test_multi_io, "Error in close completion",
                  bserrno);
        return;
    }

    spdk_bs_delete_blob(test_multi_io->bs, test_multi_io->blobid,
                        delete_complete, test_multi_io);
}

static void
read_complete(void *arg1, int bserrno)
{
    struct test_multi_io_t *test_multi_io = arg1;
    int match_res = -1;

    SPDK_NOTICELOG("entry\n");
    if (bserrno)
    {
        unload_bs(test_multi_io, "Error in read completion",
                  bserrno);
        return;
    }
    match_res = memcmp(test_multi_io->write_buff, test_multi_io->read_buff,
                       test_multi_io->cluster_size);
    if (match_res)
    {
        unload_bs(test_multi_io, "Error in data compare", -1);
        return;
    }
    else
    {
        SPDK_NOTICELOG("read SUCCESS and data matches!\n");
    }

    spdk_blob_close(test_multi_io->blob, delete_blob, test_multi_io);
}

static void
read_blob(struct test_multi_io_t *test_multi_io)
{
    SPDK_NOTICELOG("entry\n");

    test_multi_io->read_buff = spdk_malloc(test_multi_io->cluster_size * N_CLUSTER,
                                           test_multi_io->io_unit_size, NULL, SPDK_ENV_LCORE_ID_ANY,
                                           SPDK_MALLOC_DMA);
    if (test_multi_io->read_buff == NULL)
    {
        unload_bs(test_multi_io, "Error in memory allocation",
                  -ENOMEM);
        return;
    }

    spdk_blob_io_read(test_multi_io->blob, test_multi_io->channel,
                      test_multi_io->read_buff, 0,
                      test_multi_io->cluster_size * N_CLUSTER / test_multi_io->io_unit_size,
                      read_complete, test_multi_io);
}
static void cal_time(struct test_multi_io_t *test_multi_io){
    fini_count++;
    if(fini_count == IO_NUM){
        fini_count = 0;
        clock_gettime(CLOCK_REALTIME, &ts2);
        if (ts2.tv_nsec < ts1.tv_nsec)
        {
            ts2.tv_nsec += 1000000000;
            ts2.tv_sec--;
        }
        SPDK_NOTICELOG("multi io write using:%ld.%09ld\n", (long)(ts2.tv_sec - ts1.tv_sec),
                       ts2.tv_nsec - ts1.tv_nsec);
    }
    read_blob(test_multi_io);
}
static void
write_complete(void *arg1, int bserrno)
{
    struct test_multi_io_t *test_multi_io = arg1;

    SPDK_NOTICELOG("entry\n");
    if (bserrno)
    {
        unload_bs(test_multi_io, "Error in write completion",
                  bserrno);
        return;
    }

    cal_time(test_multi_io);
}

static void
blob_write(struct test_multi_io_t *test_multi_io)
{
    SPDK_NOTICELOG("entry\n");

    test_multi_io->write_buff = spdk_malloc(test_multi_io->cluster_size * N_CLUSTER,
                                            test_multi_io->io_unit_size, NULL, SPDK_ENV_LCORE_ID_ANY,
                                            SPDK_MALLOC_DMA);
    if (test_multi_io->write_buff == NULL)
    {
        unload_bs(test_multi_io, "Error in allocating memory",
                  -ENOMEM);
        return;
    }
    memset(test_multi_io->write_buff, 0x5a, test_multi_io->cluster_size);

    test_multi_io->channel = spdk_bs_alloc_io_channel(test_multi_io->bs);
    if (test_multi_io->channel == NULL)
    {
        unload_bs(test_multi_io, "Error in allocating channel",
                  -ENOMEM);
        return;
    }

    spdk_blob_io_write(test_multi_io->blob, test_multi_io->channel,
                       test_multi_io->write_buff, 0, 
                       test_multi_io->cluster_size * N_CLUSTER/ test_multi_io->io_unit_size,
                       write_complete, test_multi_io);
}

static void
sync_complete(void *arg1, int bserrno)
{
    struct test_multi_io_t *test_multi_io = arg1;

    SPDK_NOTICELOG("entry\n");
    if (bserrno)
    {
        unload_bs(test_multi_io, "Error in sync callback",
                  bserrno);
        return;
    }

    blob_write(test_multi_io);
}

static void
resize_complete(void *cb_arg, int bserrno)
{
    struct test_multi_io_t *test_multi_io = cb_arg;
    uint64_t total = 0;

    if (bserrno)
    {
        unload_bs(test_multi_io, "Error in blob resize", bserrno);
        return;
    }

    total = spdk_blob_get_num_clusters(test_multi_io->blob);
    SPDK_NOTICELOG("resized blob now has USED clusters of %" PRIu64 "\n",
                   total);

    spdk_blob_sync_md(test_multi_io->blob, sync_complete, test_multi_io);
}

static void
open_complete(void *cb_arg, struct spdk_blob *blob, int bserrno)
{
    struct test_multi_io_t *test_multi_io = cb_arg;

    SPDK_NOTICELOG("entry\n");
    if (bserrno)
    {
        unload_bs(test_multi_io, "Error in open completion",
                  bserrno);
        return;
    }

    test_multi_io->blob = blob;
    //resize one cluster
    spdk_blob_resize(test_multi_io->blob, N_CLUSTER, resize_complete, test_multi_io);
}

static void
blob_create_complete(void *arg1, spdk_blob_id blobid, int bserrno)
{
    struct test_multi_io_t *test_multi_io = arg1;

    SPDK_NOTICELOG("entry\n");
    if (bserrno)
    {
        unload_bs(test_multi_io, "Error in blob create callback",
                  bserrno);
        return;
    }

    test_multi_io->blobid = blobid;
    SPDK_NOTICELOG("new blob id %" PRIu64 "\n", test_multi_io->blobid);

    spdk_bs_open_blob(test_multi_io->bs, test_multi_io->blobid,
                      open_complete, test_multi_io);
}

static void
create_blob(struct test_multi_io_t *test_multi_io)
{
    SPDK_NOTICELOG("entry\n");
    spdk_bs_create_blob(test_multi_io->bs, blob_create_complete, test_multi_io);
}

static void
bs_init_complete(void *cb_arg, struct spdk_blob_store *bs,
                 int bserrno)
{
    struct test_multi_io_t *test_multi_io = cb_arg;

    SPDK_NOTICELOG("entry\n");
    if (bserrno)
    {
        unload_bs(test_multi_io, "Error init'ing the blobstore",
                  bserrno);
        return;
    }

    test_multi_io->bs = bs;
    SPDK_NOTICELOG("blobstore: %p\n", test_multi_io->bs);
    //get io_unit_size and cluster_size
    test_multi_io->io_unit_size = spdk_bs_get_io_unit_size(test_multi_io->bs);
    test_multi_io->cluster_size = spdk_bs_get_cluster_size(test_multi_io->bs);
    //create a list of args
    struct test_multi_io_t *meta_args[IO_NUM];
    for (int i = 0; i < IO_NUM; i++)
    {
        if ((meta_args[i] = malloc(sizeof(struct test_multi_io_t))) == NULL)
        {
            SPDK_ERRLOG("Error %d allocating the meta buffer\n", -ENOMEM);
            spdk_app_stop(-1);
            return;
        }
    }
    //init args
    for (int i = 0; i < IO_NUM; i++)
    {
        meta_args[i]->bs = test_multi_io->bs;
        meta_args[i]->io_unit_size = test_multi_io->io_unit_size;
        meta_args[i]->cluster_size = test_multi_io->io_unit_size;
    }
    //get time
    clock_gettime(CLOCK_REALTIME, &ts1);
    //run multi io test
    for (int i = 0; i < IO_NUM; i++)
    {
        create_blob(meta_args[i]);
    }
}

static void
base_bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev *bdev,
                   void *event_ctx)
{
    SPDK_WARNLOG("Unsupported bdev event: type %d\n", type);
}

static void
test_start(void *arg1)
{
    struct test_multi_io_t *test_multi_io = arg1;
    struct spdk_bs_dev *bs_dev = NULL;
    int rc;

    SPDK_NOTICELOG("entry\n");
    rc = spdk_bdev_create_bs_dev_ext(test_multi_io->bdev_name, base_bdev_event_cb, NULL, &bs_dev);
    if (rc != 0)
    {
        SPDK_ERRLOG("Could not create blob bdev, %s!!\n",
                    spdk_strerror(-rc));
        spdk_app_stop(-1);
        return;
    }

    spdk_bs_init(bs_dev, NULL, bs_init_complete, test_multi_io);
}

int main(int argc, char **argv)
{
    if(argc !=3 ){
        SPDK_NOTICELOG("invalid arguments!\n");
        return 0;
    }
    struct spdk_app_opts opts = {};
    int rc = 0;
    struct test_multi_io_t *test_multi_io = NULL;

    SPDK_NOTICELOG("entry\n");

    spdk_app_opts_init(&opts, sizeof(opts));

    opts.name = "test_multi_io";
    opts.json_config_file = argv[1];

    test_multi_io = calloc(1, sizeof(struct test_multi_io_t));
    if (test_multi_io != NULL)
    {
        test_multi_io->bdev_name = argv[2];
        rc = spdk_app_start(&opts, test_start, test_multi_io);
        if (rc)
        {
            SPDK_NOTICELOG("ERROR!\n");
        }
        else
        {
            SPDK_NOTICELOG("SUCCESS!\n");
        }
        //free
        free(test_multi_io);
    }
    else
    {
        SPDK_ERRLOG("Could not alloc test_multi_io struct!!\n");
        rc = -ENOMEM;
    }
    spdk_app_fini();
    return rc;
}
