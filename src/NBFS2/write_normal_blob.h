struct _write_normal_blob_args
{
    struct spdk_blob_store *bs;
    struct spdk_blob *blob;
    spdk_blob_id blobid;
    struct spdk_io_channel *channel;
    void *payload;
    uint64_t offset, length;
    spdk_blob_op_complete cb_fn;
    void *cb_arg;
    uint64_t cluster_size, io_unit_size;
};
static void write_normal_blob(struct spdk_blob_store *bs, spdk_blob_id blobid,
                              void *payload, uint64_t offset, uint64_t length,
                              spdk_blob_op_complete cb_fn, void *cb_arg);
static void _wnb_write_complete(void *cb_arg, int bserrno)
{
    //this function runs after spdk_blob_io_write
    SPDK_NOTICELOG("entry\n");
    struct _write_normal_blob_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d spdk_blob_io_write\n", bserrno);
        args->cb_fn(args->cb_arg, bserrno);
        free(args);
        return;
    }
    //write complete, free io_channel and close blob
    if (args->channel)
        spdk_bs_free_io_channel(args->channel);
    spdk_blob_close(args->blob, args->cb_fn, args->cb_arg);
    //free args
    free(args);
}
static void _wnb_sync_complete(void *cb_arg, int bserrno)
{
    //this function runs after spdk_blob_sync_md
    SPDK_NOTICELOG("entry\n");
    struct _write_normal_blob_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d spdk_blob_sync_md\n", bserrno);
        args->cb_fn(args->cb_arg, bserrno);
        free(args);
        return;
    }
    //write data
    spdk_blob_io_write(args->blob, args->channel, args->payload, args->offset,
                       args->length, _wnb_write_complete, args);
}
static void _wnb_resize_complete(void *cb_arg, int bserrno)
{
    //this function runs after spdk_blob_resize
    SPDK_NOTICELOG("entry\n");
    struct _write_normal_blob_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d spdk_blob_resize\n", bserrno);
        args->cb_fn(args->cb_arg, bserrno);
        free(args);
        return;
    }
    //check cluster num
    uint64_t c_num = 0;
    c_num = spdk_blob_get_num_clusters(args->blob);
    SPDK_NOTICELOG("blob now has USED clusters of %" PRIu64 "\n",
                   c_num);
    //synchronize
    spdk_blob_sync_md(args->blob, _wnb_sync_complete, args);
}
static void _wnb_open_complete(void *cb_arg, struct spdk_blob *blb,
                               int bserrno)
{
    //this function runs after spdk_bs_open_blob
    SPDK_NOTICELOG("entry\n");
    struct _write_normal_blob_args *args = cb_arg;
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
    //check cluster num
    uint64_t c_num, io_unit_num, free_c_num, add_io_num, add_c_num;
    c_num = spdk_blob_get_num_clusters(args->blob);
    SPDK_NOTICELOG("the blob now has USED clusters of %" PRIu64 "\n",
                   c_num);
    io_unit_num = spdk_blob_get_num_io_units(args->blob);
    SPDK_NOTICELOG("the blob now has USED io_units of %" PRIu64 "\n",
                   io_unit_num);
    //the offset and length are calculated by io_units
    //check available size
    if (args->offset + args->length > io_unit_num)
    {
        //calculate clusters needed
        add_io_num = args->offset + args->length - io_unit_num;
        add_c_num = add_io_num * args->io_unit_size / args->cluster_size + (add_io_num * args->io_unit_size % args->cluster_size != 0);
        //get free cluster num
        free_c_num = spdk_bs_free_cluster_count(args->bs);
        if (free_c_num < add_c_num)
        {
            SPDK_ERRLOG("Error %d spdk_bs_free_cluster_count\n", -ENOSPC);
            //return no space on device error
            args->cb_fn(args->cb_arg, -ENOSPC);
            free(args);
            return;
        }
        //resize the blob
        spdk_blob_resize(args->blob, c_num + add_c_num,
                         _wnb_resize_complete, args);
    }
    else
    {
        //write data
        spdk_blob_io_write(args->blob, args->channel, args->payload, args->offset,
                           args->length, _wnb_write_complete, args);
    }
}
/*write data in payload(buffer) to a specific blob described by id*/
static void write_normal_blob(struct spdk_blob_store *bs, spdk_blob_id blobid,
                              void *payload, uint64_t offset, uint64_t length,
                              spdk_blob_op_complete cb_fn, void *cb_arg)
{
    SPDK_NOTICELOG("entry\n");
    struct _write_normal_blob_args *args = NULL;
    if ((args = malloc(sizeof(struct _write_normal_blob_args))) == NULL)
    {
        cb_fn(cb_arg, -ENOMEM);
        SPDK_ERRLOG("Could not malloc _write_normal_blob_args struct!!\n");
        return;
    }
    //set args
    args->bs = bs;
    args->blobid = blobid;
    args->offset = offset;
    args->length = length;
    args->cb_fn = cb_fn;
    args->cb_arg = cb_arg;
    //get cluster size and io_unit size (in bytes), which will be used later
    args->cluster_size = spdk_bs_get_cluster_size(args->bs);
    args->io_unit_size = spdk_bs_get_io_unit_size(args->bs);
    //open blob by id
    spdk_bs_open_blob(args->bs, args->blobid,
                      _wnb_open_complete, args);
}