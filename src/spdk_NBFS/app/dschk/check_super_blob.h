static void check_super_blob(struct spdk_blob_store *bs, int *flag);

static void _chsb_super_blobid_got(void *cb_arg, spdk_blob_id blobid, int bserrno)
{
    //this function runs after spdk_bs_get_super
    SPDK_NOTICELOG("entry\n");
    int *flag = cb_arg;
    //check the bserrno
    if (bserrno == -ENOENT)
        *flag = 0;
    else if (bserrno)
        *flag = -1;
    else
        *flag = 1;
}
/*check if superblob is set*/
static void check_super_blob(struct spdk_blob_store *bs, int *flag)
{
    //if superblob is set, set flag with 1, else 0;
    //if error occurs set flag = -1
    SPDK_NOTICELOG("entry\n");
    //try get super
    spdk_bs_get_super(bs, _chsb_super_blobid_got, flag);
}