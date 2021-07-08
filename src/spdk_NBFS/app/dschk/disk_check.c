#include "spdk/stdinc.h"
#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/blob_bdev.h"
#include "spdk/blob.h"
#include "spdk/log.h"
#include "spdk/string.h"

#include "check_super_blob.h"

enum BUILD_METHOD{mth_init,mth_load};
struct _test_pack_args
{
    struct spdk_blob_store *bs;
    char *bdev_name;
    enum BUILD_METHOD mth;
    int rc;
};

static void _unload_complete(void *cb_arg, int bserrno)
{
    //this runs after spdk_bs_unload
    SPDK_NOTICELOG("entry\n");
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d creating the superblob\n", bserrno);
        spdk_app_stop(-1);
        return;
    }
    spdk_app_stop(0);
}
static void _bs_build_complete(void *cb_arg, struct spdk_blob_store *bs,
                              int bserrno)
{
    //this function runs after spdk_bs_load or init
    SPDK_NOTICELOG("entry\n");
    struct _test_pack_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d loading/init'ing the blobstore\n", bserrno);
        spdk_app_stop(-1);
        return;
    }
    //set bs
    args->bs = bs;
    SPDK_NOTICELOG("blobstore: %p\n", args->bs);
    //check free clusters
    uint64_t free = spdk_bs_free_cluster_count(args->bs);
    SPDK_NOTICELOG("blobstore has FREE clusters of %" PRIu64 "\n",
		       free);
    //check superblob
    int flag = 0;
    check_super_blob(args->bs, &flag);
    if (flag == 0)
        SPDK_NOTICELOG("no superblob!!\n");
    else if (flag == 1)
        SPDK_NOTICELOG("superblob found!!\n");
    else
        SPDK_NOTICELOG("superblob check error!!\n");
    //unload bs
    spdk_bs_unload(args->bs, _unload_complete, NULL);
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
    struct _test_pack_args *args = arg1;
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
    //build blobstore by bs_bdev
    if(args->mth == mth_init)
        spdk_bs_init(bs_dev, NULL, _bs_build_complete, args);
    else
         spdk_bs_load(bs_dev, NULL, _bs_build_complete, args);
}
int main(int argc, char **argv)
{
    if(argc != 4){
        SPDK_WARNLOG("Invalid arguments.\n");
        return 0;
    }
    struct spdk_app_opts opts = {};
    int rc = 0;
    struct _test_pack_args args;

    SPDK_NOTICELOG("entry\n");
    //set opts
    spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "disk_check";
    opts.json_config_file = argv[1];
    //set args
    args.bdev_name = argv[2];
    args.mth = strcmp(argv[3],"init");
    //run spdk app
    rc = spdk_app_start(&opts, _test_start, &args);
    if (rc)
    {
        SPDK_NOTICELOG("ERROR!\n");
    }
    else
    {
        SPDK_NOTICELOG("SUCCESS!\n");
    }
    //return
    spdk_app_fini();
    return rc;
}