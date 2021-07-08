# BlobFS 主要数据结构及函数

## 主要数据结构

- `spdk_blob_store`作为整个`bdev`设备的管理数据结构，可看作`spdk_blob`以及`spdk_blob_list`的队列头

```c
struct spdk_blob_store
{
    uint64_t md_start; /* Offset from beginning of disk, in pages */
    uint32_t md_len;   /* Count, in pages */

    struct spdk_io_channel *md_channel;

    pthread_mutex_t used_clusters_mutex;

    spdk_blob_id super_blob;//store the messages of the bdev
    struct spdk_bs_type bstype;

    //list of blobs
    TAILQ_HEAD(, spdk_blob)
    blobs;
    TAILQ_HEAD(, spdk_blob_list)
    snapshots;

    ......
};
```
- `spdk_blob`是每一个`blob`块的数据结构

```c
struct spdk_blob
{
    /* point to blobstore */
    struct spdk_blob_store *bs;

    uint32_t open_ref;

    spdk_blob_id id;
    spdk_blob_id parent_id;

    TAILQ_ENTRY(spdk_blob)
    link;

    /* A list of pending metadata pending_persists */
    TAILQ_HEAD(, spdk_blob_persist_ctx)
    pending_persists;

    /* Number of data clusters retrived from extent table,
	 * that many have to be read from extent pages. */
    uint64_t remaining_clusters_in_et;

    ......
};
```
- `spdk_filesystem`是`BlobFS`的数据结构，可理解为文件系统的目录

```c 
struct spdk_filesystem
{
    struct spdk_blob_store *bs;
    TAILQ_HEAD(, spdk_file)
    files;
    struct spdk_bs_opts bs_opts;
    struct spdk_bs_dev *bdev;
    fs_send_request_fn send_request; //请求发送函数指针

    struct
    {
        uint32_t max_ops;
        struct spdk_io_channel *sync_io_channel;
        struct spdk_fs_channel *sync_fs_channel;
    } sync_target;

    struct
    {
        uint32_t max_ops;
        struct spdk_io_channel *md_io_channel;
        struct spdk_fs_channel *md_fs_channel;
    } md_target;

    struct
    {
        uint32_t max_ops;
    } io_target;
};
```

- `spdk_file`是每一个文件的表项，包含该文件的各种信息以及请求队列

```c
struct spdk_file
{
    struct spdk_filesystem *fs;
    struct spdk_blob *blob;
    char *name;
    uint64_t trace_arg_name;
    uint64_t length;
    bool is_deleted;
    bool open_for_writing;
    uint64_t length_flushed;
    uint64_t length_xattr;
    uint64_t append_pos;
    uint64_t seq_byte_count;
    uint64_t next_seq_offset;
    uint32_t priority;

    TAILQ_ENTRY(spdk_file)
    tailq;

    spdk_blob_id blobid;
    uint32_t ref_count;
    pthread_spinlock_t lock;
    struct cache_buffer *last;
    struct cache_tree *tree;

    TAILQ_HEAD(open_requests_head, spdk_fs_request)
    open_requests;
    TAILQ_HEAD(sync_requests_head, spdk_fs_request)
    sync_requests;
    TAILQ_ENTRY(spdk_file)
    cache_tailq;
};
```

- `spdk_fs_cb_arg`是文件系统发送请求的数据结构，包含请求完成后的回调函数指针、各种请求的参数等

```c
struct spdk_fs_cb_args
{
    union
    {
        spdk_fs_op_with_handle_complete fs_op_with_handle;
        spdk_fs_op_complete fs_op;
        spdk_file_op_with_handle_complete file_op_with_handle;
        spdk_file_op_complete file_op;
        spdk_file_stat_op_complete stat_op;
    } fn;
    void *arg;
    sem_t *sem; //同步信号量
    struct spdk_filesystem *fs;
    struct spdk_file *file;
    
    union
    {
        struct
        {
            TAILQ_HEAD(, spdk_deleted_file)
            deleted_files;
        } fs_load;
        struct
        {
            uint64_t length;
        } truncate;
        struct
        {
            struct spdk_io_channel *channel;
            void *pin_buf;
            int is_read;
            off_t offset;
            size_t length;
            uint64_t start_lba;
            uint64_t num_lba;
            uint32_t blocklen;
        } rw;
        struct
        {
            const char *old_name;
            const char *new_name;
        } rename;
        struct
        {
            struct cache_buffer *cache_buffer;
            uint64_t length;
        } flush;
        struct
        {
            struct cache_buffer *cache_buffer;
            uint64_t length;
            uint64_t offset;
        } readahead;
        struct
        {
            /* offset of the file when the sync request was made */
            uint64_t offset;
            TAILQ_ENTRY(spdk_fs_request)
            tailq;
            bool xattr_in_progress;
            /* length written to the xattr for this file - this should
			 * always be the same as the offset if only one thread is
			 * writing to the file, but could differ if multiple threads
			 * are appending
			 */
            uint64_t length;
        } sync;
        struct
        {
            uint32_t num_clusters;
        } resize;
        struct
        {
            const char *name;
            uint32_t flags;
            TAILQ_ENTRY(spdk_fs_request)
            tailq;
        } open;
        struct
        {
            const char *name;
            struct spdk_blob *blob;
        } create;
        struct
        {
            const char *name;
        } delete;
        struct
        {
            const char *name;
        } stat;
    } op;
};
```

- `spdk_fs_request`文件请求的队列

```c
struct spdk_fs_request
{
    struct spdk_fs_cb_args args;
    TAILQ_ENTRY(spdk_fs_request)
    link;
    struct spdk_fs_channel *channel;
};
```

- `spdk_fs_channel`文件请求发送的通道
  
```c
struct spdk_fs_channel
{
    struct spdk_fs_request *req_mem;
    TAILQ_HEAD(, spdk_fs_request)
    reqs;
    sem_t sem; //同步信号量
    struct spdk_filesystem *fs;
    struct spdk_io_channel *bs_channel;
    fs_send_request_fn send_request;
    bool sync;
    uint32_t outstanding_reqs;
    pthread_spinlock_t lock;
};

typedef spdk_fs_channel spdk_fs_thread_ctx
```

## 主要函数调用过程

### 创建/打开/重命名文件

以创建文件为例：

- 第一步，调用函数

```c
int spdk_fs_create_file(struct spdk_filesystem *fs, struct spdk_fs_thread_ctx *ctx, const char *name)
```
通过通道`ctx`在文件系统`fs`中创建一个名为`name`的文件，在该函数中调用`fs`中的`send_request()`函数，发送`__fs_create_file`命令，然后阻塞。

- 第二步，`__fs_create_file()`函数调用

```c
void spdk_fs_create_file_async(struct spdk_filesystem *fs, const char *name, spdk_file_op_complete cb_fn, void *cb_arg)
```
该函数首先通过`file_alloc()`在文件系统`fs`中创建一个文件，然后再利用`spdk_bs_create_blob()`在物理空间中创造一个`blob`.

- 最后通过回调函数

```c
__fs_create_file_done(void *arg, int fserrno)
```
返回，完成创建文件。

打开、关闭、重命名文件的操作流程类似。

### 读/写文件

- 第一步，调用函数

```c
int64_t
spdk_file_read(struct spdk_file *file, struct spdk_fs_thread_ctx *ctx,
void *payload, uint64_t offset, uint64_t length)

int spdk_file_write(struct spdk_file *file, struct spdk_fs_thread_ctx *ctx,
void *payload, uint64_t offset, uint64_t length)
```
通过通道`ctx`从文件`file`，偏移量为`offset`，读取长度为`length`的信息存入`payload`中,或从`payload`中写入长度为`length`的信息。在该函数执行过程中，先通过`readahead()`预读取到`cache_buffer_tree`中，然后对`I/O`分成不大于`cache_buffer`的块，在`cache_buffer_tree`中查找每一块，若存在则`memcpy()`,否则，调用`__send_rw_from_file()`.为保证原子性，读操作使用lock锁定。

- `__send_rw_from_file`函数

```c
static int
__send_rw_from_file(struct spdk_file *file, void *payload, uint64_t offset, uint64_t length, bool is_read, struct rw_from_file_arg *arg)
```
利用`arg`中的参数，往消息队列中添加读写请求，调用`__rw_from_file`函数

- `__rw_from_file`函数根据`is_read`这一参数判断读/写，转至异步读写函数`spdk_file_read_async`或`spdk_file_write_async`
  
```c
void spdk_file_read_async(struct spdk_file *file, struct spdk_io_channel *channel, void *payload, uint64_t offset, uint64_t length, spdk_file_op_complete cb_fn, void *cb_arg)

void spdk_file_write_async(struct spdk_file *file, struct spdk_io_channel *channel, void *payload, uint64_t offset, uint64_t length, spdk_file_op_complete cb_fn, void *cb_arg)
```

- 上述两个函数再调用`__readwrite`,进而调用`__readvwritev`

```c
static void
__readvwritev(struct spdk_file *file, struct spdk_io_channel *_channel, struct iovec *iovs, uint32_t iovcnt, uint64_t offset, uint64_t length, spdk_file_op_complete cb_fn, void *cb_arg, int is_read)
```
该函数根据读写请求，分配`channel`,`request`,调用`blobstore`的函数进行读写操作。