#include "spdk/stdinc.h"
#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/blob_bdev.h"
#include "spdk/blob.h"
#include "spdk/log.h"
#include "spdk/string.h"

#include "check_super_blob.h"
#include "create_super_blob.h"
#include "read_super_blob.h"
#include "write_super_blob.h"
#include "read_normal_blob.h"
#include "write_normal_blob.h"

#include <time.h>

#define _TEST_BUFFER_SIZE 0x1000
#define _TEST_OFFSET 0
#define _TEST_BLOB_NUM 1

struct _test_write_args
{
    struct spdk_blob_store *bs;
    spdk_blob_id blobid;
    uint8_t *write_buff;
    uint64_t io_unit_size, alloc_pages;
};
struct _test_multi_io_args
{
    struct spdk_blob_store *bs;
    struct _test_write_args *t_args[_TEST_BLOB_NUM];
    char *bdev_name;
    uint64_t io_unit_size;
    int rc;
};

int finish_count = 0;
struct timespec ts1, ts2;

static void _write_complete(void *cb_arg, int bserrno)
{
    //this function runs after write_normal_blob
    finish_count++;
    if (finish_count == _TEST_BLOB_NUM)
    {
        clock_gettime(CLOCK_REALTIME, &ts2);
        if (ts2.tv_nsec < ts1.tv_nsec)
        {
            ts2.tv_nsec += 1000000000;
            ts2.tv_sec--;
        }
        SPDK_NOTICELOG("multi io using:%ld.%09ld\n", (long)(ts2.tv_sec - ts1.tv_sec),
                       ts2.tv_nsec - ts1.tv_nsec);
        spdk_app_stop(0);
    }
}
static void _blob_created(void *arg1, spdk_blob_id blobid, int bserrno)
{
    //this function runs after spdk_bs_create_blob
    SPDK_NOTICELOG("entry\n");
    struct _test_write_args *args = arg1;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d create the blob\n", bserrno);
        spdk_app_stop(-1);
        return;
    }
    //set blobid
    args->blobid = blobid;
    //malloc a buffer, alignment: io_unit_size
    args->write_buff = spdk_malloc(_TEST_BUFFER_SIZE,
                                   args->io_unit_size, NULL, SPDK_ENV_LCORE_ID_ANY,
                                   SPDK_MALLOC_DMA);
    //allocate pages
    args->alloc_pages = _TEST_BUFFER_SIZE / args->io_unit_size + (_TEST_BUFFER_SIZE % args->io_unit_size != 0);
    SPDK_NOTICELOG("buffer size:%x, io_unit_size:%x, alloc pages:%d\n",_TEST_BUFFER_SIZE,args->io_unit_size,args->alloc_pages);
    if (args->write_buff == NULL)
    {
        SPDK_ERRLOG("Error %d allocating the write buffer\n", -ENOMEM);
        spdk_app_stop(-1);
        return;
    }
    //set buffer data
    memset(args->write_buff,0x65,_TEST_BUFFER_SIZE);
    //write data to the buffer
    write_normal_blob(args->bs, args->blobid, args->write_buff, _TEST_OFFSET, args->alloc_pages, _write_complete, NULL);
}
static void _test_write(struct _test_multi_io_args *args)
{
    //run this to test multi io
    SPDK_NOTICELOG("entry\n");
    //set write args
    for (int i = 0; i < _TEST_BLOB_NUM; i++)
    {
        args->t_args[i]->bs = args->bs;
        args->t_args[i]->io_unit_size = args->io_unit_size;
    }
    //get time
    clock_gettime(CLOCK_REALTIME, &ts1);
    //multi io
    for (int i = 0; i < _TEST_BLOB_NUM; i++)
        spdk_bs_create_blob(args->bs, _blob_created, args->t_args[i]);
}
static void _bs_init_complete(void *cb_arg, struct spdk_blob_store *bs,
                              int bserrno)
{
    //this function runs after spdk_bs_load
    SPDK_NOTICELOG("entry\n");
    struct _test_multi_io_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d init the blobstore\n", bserrno);
        spdk_app_stop(-1);
        return;
    }
    //set bs
    args->bs = bs;
    SPDK_NOTICELOG("blobstore: %p\n", args->bs);
    //set io_unit_size
    args->io_unit_size = spdk_bs_get_io_unit_size(args->bs);
    //test
    _test_write(args);
}
static void _test_bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev *bdev,
                                void *event_ctx)
{
    //this function is a bdev event callback
    SPDK_WARNLOG("Unsupported bdev event: type %d\n", type);
}
static void _test_start(void *arg1)
{
    //this function runs after spdk_app_start
    SPDK_NOTICELOG("entry\n");
    struct _test_multi_io_args *args = arg1;
    //create blobstore by bdev
    struct spdk_bs_dev *bs_dev = NULL;
    int rc;
    rc = spdk_bdev_create_bs_dev_ext(args->bdev_name, _test_bdev_event_cb,
                                     NULL, &bs_dev);
    if (rc != 0)
    {
        SPDK_ERRLOG("Could not create blob bdev, %s!!\n",
                    spdk_strerror(-rc));
        spdk_app_stop(-1);
        return;
    }
    //init blobstore by bs_bdev
    spdk_bs_init(bs_dev, NULL, _bs_init_complete, args);
}
int main(int argc, char **argv)
{
    if (argc != 3)
    {
        SPDK_ERRLOG("Error %d invalid argument\n", -EINVAL);
        return 0;
    }
    SPDK_NOTICELOG("entry\n");
    struct spdk_app_opts opts = {};
    int rc = 0;
    struct _test_multi_io_args args;
    struct _test_write_args t_args[_TEST_BLOB_NUM];
    //set opts
    spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "test_multi_io";
    opts.json_config_file = argv[1];
    //set args
    args.bdev_name = argv[2];
    for(int i=0;i<_TEST_BLOB_NUM;i++)
        args.t_args[i] =&(t_args[i]);
    //run app
    rc = spdk_app_start(&opts, _test_start, &args);
    if (rc)
        SPDK_NOTICELOG("ERROR!\n");
    else
        SPDK_NOTICELOG("SUCCESS!\n");
    //return
    spdk_app_fini();
    return rc;
}