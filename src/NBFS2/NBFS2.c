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
#include "hash_table.h"
#include "id_and_char.h"

#define CMD_ARG_NUM 4
#define INIT_HASHTABLE_SIZE 65536
#define MAXFILENAME 128
#define BLOBID_STRING_LEN 20
#define ENABLE_RANDOM_RW 0
#define XATTR_HTS "HASHTABLE_SIZE"
#define XATTR_SPBDS "SUPERBLOB_DATA_SIZE"
enum FS_BUILD_METHOD
{
    MTHD_INIT,
    MTHD_LOAD
};

struct _nbfs2_super_block_meta_data
{
    spdk_blob_id blobid;
    char name[MAXFILENAME];
};
struct _nbfs2_filesystem
{
    struct spdk_blob_store *bs;
    struct spdk_blob *superblob;
    spdk_blob_id superblob_id;
    uint64_t spb_data_size;
    struct _nbfs2_super_block_meta_data *spb_mdata;
    uint64_t io_unit_size;
    HashTable *table;
    unsigned ht_size;
    uint8_t *read_buff, *write_buff;
};
struct _nbfs2_file
{
    struct _nbfs2_filesystem *fs;
    spdk_blob_id blobid;
    char *filename;
};
struct _nbfs2_args
{
    struct _nbfs2_filesystem *fs;
    struct _nbfs2_file *file;
    enum FS_BUILD_METHOD build_mthd;
    char *bdev_name;
    struct spdk_bs_dev *bs_dev;
    spdk_blob_op_complete cb_fn;
    void *cb_arg;
};
static void _nbfs2_blob_deleted(void *cb_arg, int bserrno)
{
    //this function runs after spdk_bs_delete_blob
    SPDK_NOTICELOG("entry\n");
    struct _nbfs2_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d creating a blob\n", bserrno);
        spdk_app_stop(-1);
        return;
    }
    //remove hash table item
    char blobid_string[BLOBID_STRING_LEN];
    blobid2char(args->file->blobid, blobid_string);
    ht_delete(args->fs->table, blobid_string);
    //update data size
    args->fs->spb_data_size -= sizeof(struct _nbfs2_super_block_meta_data);
    //file deleted
    SPDK_NOTICELOG("file deleted!\n");
}
static void nbfs2_delete_file(struct _nbfs2_args *args, char *filename,
                              spdk_blob_op_complete cb_fn, void *cb_arg)
{
    //delete file
    //set callback fn&arg
    args->cb_fn = cb_fn;
    args->cb_arg = cb_arg;
    //search table and get blobid
    char blobid_string[BLOBID_STRING_LEN];
    strcpy(blobid_string, ht_search(args->fs->table, filename));
    spdk_blob_id blobid;
    char2blobid(blobid_string, &blobid);
    //set file object
    args->file->filename = filename;
    args->file->blobid = blobid;
    args->file->fs = args->fs;
    //delete blob
    spdk_bs_delete_blob(args->fs->bs, blobid, _nbfs2_blob_deleted, args);
}
static void nbfs2_read_file(struct _nbfs2_args *args, void *buffer,uint64_t length,
                              spdk_blob_op_complete cb_fn, void *cb_arg)
{
    //read file to buffer(must be opened)
    read_normal_blob(args->fs->bs, args->file->blobid, buffer,
                     ENABLE_RANDOM_RW, length, cb_fn, cb_arg);
}
static void nbfs2_write_file(struct _nbfs2_args *args, void *buffer,uint64_t length,
                              spdk_blob_op_complete cb_fn, void *cb_arg)
{
    //write buffer to file(must be opened)
    write_normal_blob(args->fs->bs, args->file->blobid, buffer,
                      ENABLE_RANDOM_RW, length, cb_fn, cb_arg);
}
static void nbfs2_open_file_sync(struct _nbfs2_args *args, char *filename)
{
    //use filename open file
    //search table and get blobid
    char blobid_string[BLOBID_STRING_LEN];
    strcpy(blobid_string, ht_search(args->fs->table, filename));
    spdk_blob_id blobid;
    char2blobid(blobid_string, &blobid);
    //set file
    args->file->blobid = blobid;
    args->file->filename = filename;
    args->file->fs = args->fs;
    //file opened
    SPDK_NOTICELOG("file opened!\n");
}
static void _nbfs2_file_created(void *cb_arg, spdk_blob_id blobid, int bserrno)
{
    //this function runs after spdk_bs_create_blob
    SPDK_NOTICELOG("entry\n");
    struct _nbfs2_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d creating a blob\n", bserrno);
        args->cb_fn(args->cb_arg,bserrno);
        spdk_app_stop(-1);
        return;
    }
    //set blobid
    args->file->blobid = blobid;
    //convert id
    char blobid_string[BLOBID_STRING_LEN];
    blobid2char(args->file->blobid, blobid_string);
    //store id&name on table
    ht_insert(args->fs->table, args->file->filename, blobid_string);
    //update data_size
    args->fs->spb_data_size += sizeof(struct _nbfs2_super_block_meta_data);
    //file created
    SPDK_NOTICELOG("file created!\n");
    args->cb_fn(args->cb_arg,0);
}
static void nbfs2_create_file(struct _nbfs2_args *args, char *filename,
        spdk_blob_op_complete 	cb_fn,void * 	cb_arg )
{
    //create a file
    //set callback fn&arg
    args->cb_fn = cb_fn;
    args->cb_arg = cb_arg;
    struct _nbfs2_file *file;
    if ((file = malloc(sizeof(struct _nbfs2_file))) == NULL)
    {
        SPDK_ERRLOG("Error %d malloc a file\n", -ENOMEM);
        args->cb_fn(args->cb_arg,-ENOMEM);
        spdk_app_stop(-1);
        return;
    }
    args->file = file;
    //initialize file
    file->filename = filename;
    file->fs = args->fs;
    //just alloc a blob, and store id&name on table, update data_size
    spdk_bs_create_blob(args->fs->bs, _nbfs2_file_created, args);
}
static void _nbfs2_unload_complete(void *cb_arg, int bserrno)
{
    //this function runs after spdk_bs_unload
    SPDK_NOTICELOG("entry\n");
    struct _nbfs2_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d unload the blobstore\n", bserrno);
        args->cb_fn(args->cb_arg,bserrno);
        spdk_app_stop(-1);
        return;
    }
    //free buffers
    spdk_free(args->fs->write_buff);
    spdk_free(args->fs->read_buff);
    //return
        args->cb_fn(args->cb_arg,0);
    spdk_app_stop(0);
}
static void _nbfs2_superblob_written(void *cb_arg, int bserrno)
{
    //this function runs after write_super_blob
    SPDK_NOTICELOG("entry\n");
    struct _nbfs2_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d writing the superblob\n", bserrno);
        args->cb_fn(args->cb_arg,bserrno);
        spdk_app_stop(-1);
        return;
    }
    //unload bs
    spdk_bs_unload(args->fs->bs, _nbfs2_unload_complete, args);
}
static void _nbfs2_write_htable(struct _nbfs2_args *args)
{
    SPDK_NOTICELOG("entry\n");
    //malloc spb_mdata
    struct _nbfs2_super_block_meta_data *spb_mdata;
    if ((spb_mdata = malloc(sizeof(struct _nbfs2_super_block_meta_data))) == NULL)
    {
        SPDK_ERRLOG("Error %d malloc spb_mdata\n", -ENOMEM);
        args->cb_fn(args->cb_arg,-ENOMEM);
        spdk_app_stop(-1);
        return;
    }
    //write all meta data to buffer
    HashTable *table = args->fs->table;
    int mdata_num = 0, offset = 0;
    for (unsigned i = 0; i < args->fs->ht_size; i++)
    {
        if (table->items[i])
        {
            mdata_num++;
            //set spb_mdata
            strcpy(spb_mdata->name, table->items[i]->key);
            char2blobid(table->items[i]->value, &(spb_mdata->blobid));
            //write it to buffer
            memcpy(args->fs->write_buff + offset, spb_mdata,
                   sizeof(struct _nbfs2_super_block_meta_data));
            offset += sizeof(struct _nbfs2_super_block_meta_data);
            //check overflow bucket
            if (table->overflow_buckets[i])
            {
                LinkedList *head = table->overflow_buckets[i];
                while (head)
                {
                    mdata_num++;
                    //set spb_mdata
                    strcpy(spb_mdata->name, head->item->key);
                    char2blobid(head->item->value, &(spb_mdata->blobid));
                    //write it to buffer
                    memcpy(args->fs->write_buff + offset, spb_mdata,
                           sizeof(struct _nbfs2_super_block_meta_data));
                    offset += sizeof(struct _nbfs2_super_block_meta_data);
                    head = head->next;
                }
            }
        }
    }
    //set datasize
    args->fs->spb_data_size = mdata_num * sizeof(struct _nbfs2_super_block_meta_data);
    //set xattrs
    //set hash table size
    size_t xattr_size = sizeof(args->fs->ht_size);
    spdk_blob_set_xattr(args->fs->superblob, XATTR_HTS, (const void *)&(args->fs->ht_size), xattr_size);
    //set superblob datasize
    xattr_size = sizeof(args->fs->spb_data_size);
    spdk_blob_set_xattr(args->fs->superblob, XATTR_SPBDS, (const void *)&(args->fs->spb_data_size), xattr_size);
    //write buffer to superblob
    write_super_blob(args->fs->bs, args->fs->write_buff, 0,
                     args->fs->spb_data_size, _nbfs2_superblob_written, args);
}
static void nbfs2_unload_fs(struct _nbfs2_args *args,
                              spdk_blob_op_complete cb_fn, void *cb_arg)
{
    //unload filesystem
    //set callback fn&arg
    args->cb_fn = cb_fn;
    args->cb_arg = cb_arg;
    //store hash table
    //malloc write buffer
    args->fs->write_buff = spdk_malloc(args->fs->spb_data_size,
                                       args->fs->io_unit_size, NULL,
                                       SPDK_ENV_LCORE_ID_ANY, SPDK_MALLOC_DMA);
    //write htable to buff
    _nbfs2_write_htable(args);
}
static void _nbfs2_build_htable(struct _nbfs2_args *args)
{
    SPDK_NOTICELOG("entry\n");
    //malloc spb_mdata
    struct _nbfs2_super_block_meta_data *spb_mdata;
    char *blobid_string;
    if ((spb_mdata = malloc(sizeof(struct _nbfs2_super_block_meta_data))) == NULL)
    {
        SPDK_ERRLOG("Error %d malloc spb_mdata\n", -ENOMEM);
        args->cb_fn(args->cb_arg,-ENOMEM);
        spdk_app_stop(-1);
        return;
    }
    //read all meta data
    int mdata_num = args->fs->spb_data_size / sizeof(struct _nbfs2_super_block_meta_data);
    int offset = 0;
    for (int i = 0; i < mdata_num; i++)
    {
        memcpy(spb_mdata, args->fs->read_buff + offset, sizeof(struct _nbfs2_super_block_meta_data));
        offset += sizeof(struct _nbfs2_super_block_meta_data);
        //get blobid string
        if ((blobid_string = malloc(BLOBID_STRING_LEN * sizeof(char))) == NULL)
        {
            SPDK_ERRLOG("Error %d malloc blobid_string\n", -ENOMEM);
            spdk_app_stop(-1);
            return;
        }
        //convert and insert
        blobid2char(spb_mdata->blobid, blobid_string);
        ht_insert(args->fs->table, spb_mdata->name, blobid_string);
    }
    //hash table created
    SPDK_NOTICELOG("hash table loaded!\n");
    args->cb_fn(args->cb_arg,0);
}
static void _nbfs2_superblob_read(void *cb_arg, int bserrno)
{
    //this function runs after read_super_blob
    SPDK_NOTICELOG("entry\n");
    struct _nbfs2_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d reading the superblob\n", bserrno);
        args->cb_fn(args->cb_arg,bserrno);
        spdk_app_stop(-1);
        return;
    }
    //build htable by key-val
    _nbfs2_build_htable(args);
}
static void _nbfs2_superblob_opened_load(void *cb_arg, struct spdk_blob *blb, int bserrno)
{
    //this function runs after spdk_bs_open_blob
    SPDK_NOTICELOG("entry\n");
    struct _nbfs2_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d openning the superblob\n", bserrno);
        args->cb_fn(args->cb_arg,bserrno);
        spdk_app_stop(-1);
        return;
    }
    //set superblob
    args->fs->superblob = blb;
    //get hash table size
    size_t xattr_size = sizeof(args->fs->ht_size);
    spdk_blob_get_xattr_value(args->fs->superblob, XATTR_HTS,
                              (const void **)&(args->fs->ht_size), &xattr_size);
    //init hash table
    args->fs->table = create_table(args->fs->ht_size);
    //get superblob datasize
    xattr_size = sizeof(args->fs->spb_data_size);
    spdk_blob_get_xattr_value(args->fs->superblob, XATTR_SPBDS,
                              (const void **)&(args->fs->spb_data_size), &xattr_size);
    //read the superblob, which contains val-key of files
    //key:filename, value:blobid in char
    //malloc buffer to read
    args->fs->read_buff = spdk_malloc(args->fs->spb_data_size,
                                      args->fs->io_unit_size, NULL,
                                      SPDK_ENV_LCORE_ID_ANY, SPDK_MALLOC_DMA);
    //read
    read_super_blob(args->fs->bs, args->fs->read_buff, 0,
                    args->fs->spb_data_size, _nbfs2_superblob_read, args);
}
static void _nbfs2_superblob_id_got(void *cb_arg, spdk_blob_id blobid, int bserrno)
{
    //this function runs after spdk_bs_get_super
    SPDK_NOTICELOG("entry\n");
    struct _nbfs2_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d getting the superblob id\n", bserrno);
        args->cb_fn(args->cb_arg,bserrno);
        spdk_app_stop(-1);
        return;
    }
    //set id
    args->fs->superblob_id = blobid;
    //open superblob
    spdk_bs_open_blob(args->fs->bs, args->fs->superblob_id,
                      _nbfs2_superblob_opened_load, args);
}
static void _nbfs2_bs_load_complete(void *cb_arg, struct spdk_blob_store *bs,
                                    int bserrno)
{
    //this function runs after spdk_bs_load
    SPDK_NOTICELOG("entry\n");
    struct _nbfs2_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d loading the blobstore\n", bserrno);
        args->cb_fn(args->cb_arg,bserrno);
        spdk_app_stop(-1);
        return;
    }
    //set bs
    args->fs->bs = bs;
    SPDK_NOTICELOG("blobstore: %p\n", args->fs->bs);
    //get superblob id
    spdk_bs_get_super(args->fs->bs, _nbfs2_superblob_id_got, args);
}
static void _nbfs2_superblob_opened_init(void *cb_arg, struct spdk_blob *blb, int bserrno)
{
    //this function runs after spdk_bs_open_blob
    SPDK_NOTICELOG("entry\n");
    struct _nbfs2_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d open superblob\n", bserrno);
        args->cb_fn(args->cb_arg,bserrno);
        spdk_app_stop(-1);
        return;
    }
    //set blob
    args->fs->superblob = blb;
    //set hashtable size and xattr
    args->fs->ht_size = INIT_HASHTABLE_SIZE;
    size_t xattr_size = sizeof(args->fs->ht_size);
    spdk_blob_set_xattr(args->fs->superblob, "HASHTABLE_SIZE",
                        (const void *)&(args->fs->ht_size), xattr_size);
    //init hashtable
    args->fs->table = create_table(args->fs->ht_size);
    //show
    SPDK_NOTICELOG("nbfs2 initialized!\n");
    args->cb_fn(args->cb_arg,0);
}
static void _nbfs2_superblob_created(void *cb_arg, spdk_blob_id blobid, int bserrno)
{
    //this function runs after create_superblob
    SPDK_NOTICELOG("entry\n");
    struct _nbfs2_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d initalize the blobstore\n", bserrno);
        args->cb_fn(args->cb_arg,bserrno);
        spdk_app_stop(-1);
        return;
    }
    //set blobid
    args->fs->superblob_id = blobid;
    //open superblob
    spdk_bs_open_blob(args->fs->bs, args->fs->superblob_id,
                      _nbfs2_superblob_opened_init, args);
}
static void _nbfs2_bs_init_complete(void *cb_arg, struct spdk_blob_store *bs,
                                    int bserrno)
{
    //this function runs after spdk_bs_init
    SPDK_NOTICELOG("entry\n");
    struct _nbfs2_args *args = cb_arg;
    //errno handle
    if (bserrno)
    {
        SPDK_ERRLOG("Error %d initalize the blobstore\n", bserrno);
        args->cb_fn(args->cb_arg,bserrno);
        spdk_app_stop(-1);
        return;
    }
    //set bs
    args->fs->bs = bs;
    SPDK_NOTICELOG("blobstore: %p\n", args->fs->bs);
    //create superblob
    create_super_blob(args->fs->bs, _nbfs2_superblob_created, args);
}
static void nbfs2_build_fs(struct _nbfs2_args *args,
                              spdk_blob_op_complete cb_fn, void *cb_arg)
{
    //use this function to build fs
    //set callback fn&arg
    args->cb_fn = cb_fn;
    args->cb_arg = cb_arg;
    if (args->build_mthd == MTHD_INIT)
    {
        //initialize
        spdk_bs_init(args->bs_dev, NULL, _nbfs2_bs_init_complete, args);
    }
    else
    {
        //load
        spdk_bs_load(args->bs_dev, NULL, _nbfs2_bs_load_complete, args);
    }
}
static void _test_bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev *bdev,
                                void *event_ctx)
{
    //this function is a bdev event callback
    SPDK_WARNLOG("Unsupported bdev event: type %d\n", type);
}
static void _nbfs2_start(void *arg1)
{
    //this function runs after spdk_app_start
    SPDK_NOTICELOG("entry\n");
    struct _nbfs2_args *args = arg1;
    //create a blobstore by bdev
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
    //set bs_dev
    args->bs_dev = bs_dev;
    //build blobstore
    nbfs2_build_fs(args,NULL,NULL);
}
int main(int argc, char **argv)
{
    if (argc != CMD_ARG_NUM)
    {
        SPDK_ERRLOG("Error %d invalid argument\n", -EINVAL);
        return -1;
    }
    SPDK_NOTICELOG("entry\n");
    struct spdk_app_opts opts = {};
    int rc = 0;
    struct _nbfs2_filesystem fs;
    struct _nbfs2_args args;
    //set opts
    spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "NBFS2";
    opts.json_config_file = argv[1];
    //set args
    args.bdev_name = argv[2];
    args.fs = &fs;
    //set fs build method
    args.build_mthd = strcmp(argv[3], "init");
    //run spdk app
    rc = spdk_app_start(&opts, _nbfs2_start, &args);
    if (rc)
        SPDK_NOTICELOG("ERROR!\n");
    else
        SPDK_NOTICELOG("SUCCESS!\n");
    //return
    spdk_app_fini();
    return rc;
}