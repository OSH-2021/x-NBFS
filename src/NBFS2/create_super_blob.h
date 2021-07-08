struct _create_super_blob_args
{
    struct spdk_blob_store *bs;
    spdk_blob_id blobid;
    spdk_blob_op_with_id_complete cb_fn;
    void *cb_arg;
};
static void create_super_blob(struct spdk_blob_store *bs,
                              spdk_blob_op_with_id_complete cb_fn, void *cb_arg);

static void _csb_super_blob_set(void *cb_arg, int bserrno)
{
    //this function runs after super blob set
    SPDK_NOTICELOG("entry\n");
    struct _create_super_blob_args *args = cb_arg;
    //return
    args->cb_fn(args->cb_arg, args->blobid, bserrno);
    free(args);
}
static void _csb_super_blob_created(void *cb_arg, spdk_blob_id blobid, int bserrno)
{
    //this function runs after super blob created
    SPDK_NOTICELOG("entry\n");
    struct _create_super_blob_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d spdk_bs_create_blob\n", bserrno);
        args->cb_fn(args->cb_arg, args->blobid, bserrno);
        free(args);
        return;
    }
    //set blobid
    args->blobid = blobid;
    SPDK_NOTICELOG("super blob id %" PRIu64 "\n", blobid);
    //set blob as superblob
    spdk_bs_set_super(args->bs, blobid, _csb_super_blob_set, args);
}
/*create a super blob*/
static void create_super_blob(struct spdk_blob_store *bs,
                              spdk_blob_op_with_id_complete cb_fn, void *cb_arg)
{
    //the callback function is defined as spdk_blob_op_with_id_complete
    //if bserrno is not 0, then failed
    //current problem: I'm not sure if a restart of blobstore is needed to write the superblob info to the SSD
    SPDK_NOTICELOG("entry\n");
    struct _create_super_blob_args *args = NULL;
    if ((args = malloc(sizeof(struct _create_super_blob_args))) == NULL)
    {
        cb_fn(cb_arg, 0, -ENOMEM);
        SPDK_ERRLOG("Could not malloc create_super_blob_args struct!!\n");
        return;
    }
    args->bs = bs;
    args->cb_fn = cb_fn;
    args->cb_arg = cb_arg;
    spdk_bs_create_blob(bs, _csb_super_blob_created, args);
}