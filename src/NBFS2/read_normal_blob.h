struct _read_normal_blob_args
{
    struct spdk_blob_store *bs;
    struct spdk_blob *blob;
    struct spdk_io_channel *channel;
    spdk_blob_id blobid;
    void *payload;
    uint64_t offset, length;
    spdk_blob_op_complete cb_fn;
    void *cb_arg;
};
static void read_normal_blob(struct spdk_blob_store *bs, spdk_blob_id blobid,
                             void *payload, uint64_t offset, uint64_t length,
                             spdk_blob_op_complete cb_fn, void *cb_arg);
static void _rnb_read_complete(void *cb_arg, int bserrno)
{
    //this function runs after spdk_blob_io_read
    SPDK_NOTICELOG("entry\n");
    struct _read_normal_blob_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d spdk_blob_io_read\n", bserrno);
        args->cb_fn(args->cb_arg, bserrno);
        free(args);
        return;
    }
    //free io channel
    if (args->channel){
        spdk_bs_free_io_channel(args->channel);
    }
    //close blob
    spdk_blob_close(args->blob, args->cb_fn, args->cb_arg);
    //free args
    free(args);
}
static void _rnb_open_complete(void *cb_arg, struct spdk_blob *blb,
                               int bserrno)
{
    //this function runs after spdk_bs_open_blob
    SPDK_NOTICELOG("entry\n");
    struct _read_normal_blob_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d spdk_bs_open_blob\n", bserrno);
        args->cb_fn(args->cb_arg, bserrno);
        free(args);
        return;
    }
    //set blob
    args->blob = blb;
    //allocate an io channel
    args->channel = spdk_bs_alloc_io_channel(args->bs);
    if (args->channel == NULL)
    {
        SPDK_ERRLOG("Error %d spdk_bs_alloc_io_channel\n", -ENOMEM);
        args->cb_fn(args->cb_arg, -ENOMEM);
        free(args);
        return;
    }
    //read data
    spdk_blob_io_read(args->blob, args->channel, args->payload,
                      args->offset, args->length, _rnb_read_complete, args);
    
}
/*read data from a specific blob to payload(buffer)*/
static void read_normal_blob(struct spdk_blob_store *bs, spdk_blob_id blobid,
                             void *payload, uint64_t offset, uint64_t length,
                             spdk_blob_op_complete cb_fn, void *cb_arg)
{
    SPDK_NOTICELOG("entry\n");
    struct _read_normal_blob_args *args = NULL;
    if ((args = malloc(sizeof(struct _read_normal_blob_args))) == NULL)
    {
        cb_fn(cb_arg, -ENOMEM);
        SPDK_ERRLOG("Could not malloc _read_normal_blob_args struct!!\n");
        return;
    }
    //set args
    args->bs = bs;
    args->payload = payload;
    args->offset = offset;
    args->length = length;
    args->cb_fn = cb_fn;
    args->cb_arg = cb_arg;
    //open blob
    spdk_bs_open_blob(args->bs, args->blobid,
                      _rnb_open_complete, args);
}