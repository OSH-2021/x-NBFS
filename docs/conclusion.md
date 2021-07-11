# 结题报告

- [结题报告](#结题报告)
  - [项目简介](#项目简介)
  - [项目背景](#项目背景)
    - [NVMe](#nvme)
    - [FUSE文件系统](#fuse文件系统)
    - [相关工作](#相关工作)
      - [SPDK及相应的文件系统](#spdk及相应的文件系统)
      - [NVMeDirect及相应的文件系统](#nvmedirect及相应的文件系统)
      - [UNVMe及相应的文件系统](#unvme及相应的文件系统)
      - [其它面向NVMe SSD的用户态文件系统](#其它面向nvme-ssd的用户态文件系统)
  - [项目设计](#项目设计)
    - [工具选择](#工具选择)
      - [FUSE文件系统](#fuse文件系统-1)
      - [SPDK](#spdk)
      - [BlobStore](#blobstore)
        - [Blob划分](#blob划分)
        - [特殊块](#特殊块)
    - [NBFS详细设计](#nbfs详细设计)
      - [通用接口设计](#通用接口设计)
        - [整体架构](#整体架构)
        - [B+树](#b树)
        - [创建/打开/重命名文件](#创建打开重命名文件)
        - [读/写文件](#读写文件)
      - [专用接口设计](#专用接口设计)
        - [Blob有关数据](#blob有关数据)
        - [Superblob](#superblob)
        - [文件查找](#文件查找)
        - [读写数据](#读写数据)
  - [挂载与测试](#挂载与测试)
      - [通用接口挂载FUSE测试](#通用接口挂载fuse测试)
      - [专用接口性能测试](#专用接口性能测试)
  - [项目总结与前景展望](#项目总结与前景展望)
    - [NBFS的特点](#nbfs的特点)
      - [异步无锁并发](#异步无锁并发)
      - [采用轮询方式而不是中断](#采用轮询方式而不是中断)
      - [直接与NVMe设备通信，规避多次内核态与用户态切换造成的性能损失](#直接与nvme设备通信规避多次内核态与用户态切换造成的性能损失)
    - [项目经过](#项目经过)
    - [存在的不足](#存在的不足)
    - [前景展望](#前景展望)
  - [参考文献](#参考文献)


## 项目简介

本项目旨在设计和实现一个针对改进`NVMe SSD`读写性能的文件系统，并基于`SPDK`工具实现了该文件系统，同时提供了两套用于访问底层设备的接口，一套是用于`FUSE`挂载的通用接口，另一套是用于专用应用的专用接口。

## 项目背景

近几年存储行业发生了翻天覆地的变化，半导体存储登上了历史的舞台。和传统磁盘存储介质相比，半导体存储介质具有天然的优势。无论在可靠性、性能、功耗等方面都远远超越传统磁盘。目前常用的半导体存储介质是`NVMe SSD`，采用`PCIe`接口方式与主机进行交互，大大提升了性能，释放了存储介质本身的性能。

存储介质的革命一方面给存储系统性能提升带来了福音；另一方面对文件系统的设计带来了诸多挑战。原有面向磁盘设计的文件系统不能充分利用新型存储介质，传统的内核态文件系统过于厚重, 内核存储栈存在大量的拷贝、上下文切换以及中断等软件开销, 导致读写延迟较大, 无法充分发挥`NVMe SSD`的硬件性能。

### NVMe

`NVM Express(NVMe)`，或称非易失性内存主机控制器接口规范`(Non-Volatile Memory Host Controller Interface Specification)`，是一个逻辑设备接口规范。它是基于设备逻辑接口的总线传输协议规范（相当于通讯协议中的应用层），用于访问通过`PCI Express(PCIe)`总线附加的非易失性存储器介质（例如采用闪存的固态硬盘驱动器），虽然理论上不一定要求`PCIe`总线协议。`NVMe`是一种协议，是一组允许`SSD`使用`PCIe`总线的软硬件标准；而`PCIe`是实际的物理连接通道。

此规范主要是为基于闪存的存储设备提供一个低延时、内部并发化的原生界面规范，也为现代CPU、电脑平台及相关应用提供原生存储并发化的支持，令主机硬件和软件可以充分利用固态存储设备的并行化存储能力。相比此前机械硬盘驱动器`(HDD)`时代的`AHCI`（`SATA`下的协议），`NVMe`降低了I/O操作等待时间、提升同一时间内的操作数、更大容量的操作队列等。

**NVMe工作原理**

`NVMe`使用的增强型`NVMHCI`基于成对的提交和完成队列机制：指令由主机软件放置在提交队列中。完成内容放入关联的控制器完成队列。多个提交队列可以利用相同的完成队列。提交和完成队列分配在主机内存中。存在用于设备管理目的的`Admin`提交和关联的完成队列和控制-例如创建和删除I/O提交和完成队列，中止命令，等等。只有属于管理命令集的命令才可以发布到管理提交中队列。

`NVMe`要求I/O命令集与I/O队列一起使用。该规范定义了一个I/O命令集，被命名为`NVM Command Set`。

![NVMe_controllor](/docs/files/1_NVMe_controllor.png)

主机软件会创建队列，最多不超过控制器支持的队列。通常数量创建的命令队列基于系统配置和预期的工作量。例如，在基于四核处理器的系统上，每个核可能有一个队列对`（queue pair）`，以避免锁定并确保数据结构是在适当的处理器内核的缓存中创建的。下图提供了图形队列机制的表示形式，显示了“提交队列”与“完成队列”之间的1：1映射完成队列。

### FUSE文件系统

传统文件系统结构如下：

![file_systerm](/docs/files/1_filesystem.jpg)

传统上操作系统在内核层面上对文件系统提供支持，但通常内核态的代码难以调试，内核下定制开发文件系统难度较高，生产率较低，在用户空间可定制实现文件系统是有需求的。并且用户空间下调试工具丰富，出问题不会导致系统崩溃，开发难度相对较低，开发周期较短。于是，用户态文件系统FUSE成为当下的热门研究方向。

用户空间文件系统（`Filesystem in Userspace`，简称`FUSE`）是一个面向类`Unix`计算机操作系统的软件接口，它使无特权的用户能够无需编辑内核代码而创建自己的文件系统。

`FUSE`包含两个组件：内核模块（在常规系统内核存储库中维护）和用户空间库（`libfuse`）。`FUSE`文件系统通常实现为与`libfuse`链接的独立应用程序。`libfuse`提供了以下功能：挂载文件系统，卸载文件系统，从内核读取请求以及将响应发送回。

在`Linux`中，对文件的访问都是统一通过VFS层提供的内核接口进行的（比如`open/read`），因此当一个进程（称为`"user"`）访问用户态文件系统时，依然需要途径`VFS`。当`VFS`接到`user`进程对文件的访问请求，并且判断出该文件是属于某个用户态文件系统，就会将这个请求转交给一个名为`"fuse"`的内核模块；而后，`"fuse"`将该请求传给用户态进程（如下图）。

![fuse](/docs/files/2_fuse.png)

`FUSE`作为用户态文件系统使用、调试方便，但由于其额外的内核态/用户态切换会影响性能。

### 相关工作

传统的内核态文件系统存在拷贝、上下文切换以及中断等方面的开销，因此, 轻薄高效的用户态文件系统逐渐成为研究热点。近年来, 一些面向`NVMe SSD`的用户态框架、驱动及文件系统被相继提出。

#### SPDK及相应的文件系统

`Intel`公司提出针对高性能`NVMe SSD`设备的存储性能开发工具包`SPDK`, 将所有必需的驱动程序移动到用户空间, 避免系统调用, 并允许从应用程序进行零拷贝访问, 通过轮询方式检查操作完成情况, 以降低延迟, 通过消息传递机制来避免I/O路径中的锁冲突。以下是多种基于`SPDK`的文件系统：

- `BLOBFS`是基于`SPDK`实现的用户态文件系统, 功能较为简单, 没有提供齐全的`POSIX`接口, 不提供对目录的支持, 不支持原地址更新, 只能被一个进程独享使用。
- `BlueStore`是一个针对`SSD`特性优化的存储引擎, 其中包含一个轻量级的文件系统`BLUEFS`, 并含有用户态块设备抽象层`NVMEDevice`, 调用`SPDK`的用户态块设备驱动。
与`BLOBFS`相似,`BLUEFS`只提供少量文件接口, 只支持顺序写和两层目录, 只能被一个进程独享使用。
- `kongseok`提出基于`SPDK`的`NVMe Driver`的用户态文件系统`NVFUSE`, 使用类似 `Ext3`的文件系统布局, 并计划通过实现轻量级日志, 来进行文件系统的一致性管理, 目前开发不活跃。
- `DashFS`是一种利用进程间通信机制的用户态文件系统, 同样是基于`SPDK`开发, 仅提供简单的文件操作, 不支持目录, 不考虑崩溃一致性, 研究重点在于通过微内核参与的方式实现进程的信任和安全认证, 但缺乏页缓存机制, 访问性能存在一定的提升空间, 并且缺乏对进程的死锁检测机制。

#### NVMeDirect及相应的文件系统

`Kim`等提出用户级I/O框架`NVMeDirect`,通过允许用户应用程序直接访问存储设备来提高性能。与`SPDK`不同,`NVMeDirect`可以与内核的传统I/O堆栈共存,以便现有基于内核的应用程序可以在不同的分区上与`NVMeDirect`应用程序同时使用同一个`NVMe SSD`。

`NVMeDirect`提供队列管理, 每个用户应用程序可根据其I/O特性和需要来选择自己的I/O策略。`NVMeDirect`框架比`SPDK`框架简单, 框架线程直接操作设备的I/O队列。然而,`NVMeDirect`是一个用户态调度的框架, 使用起来不方便。

`ForestFS`是`NVMeDirect`支持的用户态文件系统, 使用`ForestDB`进行文件系统元数据管理。与`BLOBFS`相比,`ForestFS`支持多级目录和原地更新, 但代码量小, 功能较为简单。

#### UNVMe及相应的文件系统
`Micron`公司提出面向`NVMe SSD`的用户态驱动`UNVMe`, 驱动和客户端应用合并在一个进程内, 性能较好, 但限制一个`NVMe`设备在同一时刻只能被一个应用程序访问。基于`UNVMe`, `Micron`公司提出`UNFS`, 但只支持同步接口, 不支持异步接口, 不提供崩溃一致性保证。

#### 其它面向NVMe SSD的用户态文件系统
除此之外, 还有很多面向`NVMe SSD`的用户态文件系统, 包括`Rustfs`, `Moneta-D`, `CRUISE`, `BurstFS`, `SCFS`和`BlueSky`等, 但它们尚未开源，因此在本项目中不做讨论。

## 项目设计

我们使用`SPDK`作为文件系统的底层框架，为了照顾通用性与性能，对上层，我们实现了两套接口：通用接口、专用接口。具体工具的选择以及文件系统设计细节如下：

### 工具选择

#### FUSE文件系统

对于针对`NVMe`优化的文件系统，我们选择采用**用户态文件系统**来实现，原因如下：

- 相比于内核态文件系统，基于用户态的文件系统便于**开发与调试**；
- 传统的内核态文件系统过于厚重，频繁的内核态与用户态文件的拷贝将严重影响效率；
- 利用SPDK的特性，我们可以在用户态**直接**向NVMe SSD提交I/O请求，从而规避FUSE额外的内核态、用户态切换，真正提高效率。

我们将利用`SPDK`提供的`FUSE`插件来对我们设计的文件系统的通用接口进行测试.

#### SPDK

`The Storage Performance Development Kit`（`SPDK`）提供了一套工具和库，用于编写高性能、可扩展的用户模式存储应用程序。它通过使用一些关键技术来实现高性能：

- 将所有必要的驱动程序移入用户空间。这样可以避免系统调用，并实现应用程序的零拷贝访问。
- 对硬件进行轮询，而不是使用中断，这降低了总延迟和延迟波动。
- 避免I/O路径中的所有锁，而是依靠消息传递。

`SPDK`的底层是一个用户空间、`polled`模式、异步、无锁的`NVMe`驱动。它提供了从用户空间应用的零拷贝、高度并行的直接访问。该驱动以C语言库的形式编写，且只有一个公共头。

`SPDK`还提供了一个完整的区块堆栈，作为用户空间库，执行大量与操作系统中区块堆栈相同的操作。这包括统一不同存储设备之间的接口，利用队列处理内存不足或I/O挂起等情况，以及逻辑分区的管理。

最后，`SPDK`提供了建立在这些组件之上的`NVMe-oF`、`iSCSI`和`vhost`服务器，它们能够通过网络或向其他进程提供磁盘服务。`NVMe-oF`和`iSCSI`的标准`Linux`内核启动器可以与这些目标互操作，`QEMU`也可以与`vhost`互操作。这些服务器的CPU的执行效率可以比其他实现高出一个数量级。这些目标可以作为如何实现高性能存储目标的例子，或者作为生产部署的基础。

选择SPDK的原因如下：

- **`SPDK`通过针对性的优化，例如驱动程序移入用户空间、采用轮询等，能充分利用`NVMe`的性能；**
- **由于`SPDK`直接与`NVMe`设备通信，能规避用户态文件系统`FUSE`多次内核态与用户态切换造成的性能损失。**

#### BlobStore

`Blobstore`是`SPDK`中的一个模块，它实现对`Blob`的管理，包括`Blob`的分配、删除、读取、写入、元数据的管理等。在`Blobstore`下层，与`SPDK bdev`层对接。`SPDK bdev`层类似于内核中的通用块设备层，是对底层不同类型设备的统一抽象管理。

##### Blob划分

在`blobstore`中，将`SSD`中的块划分为多个抽象层，主要由`Logical Block`、`Page`、`Cluster`、`Blob`组成，它们之间的关系如下所示：

![blob](/docs/files/blob.jpg)

- **Logical Block**：与块设备中所提供的逻辑块相对应，通常为`512B`或`4KiB`。
- **Page**：由多个连续的`Logical Block`构成，通常一个`page`的大小为`4KiB`，因此一个`Page`由八个或一个`Logical Block`构成，取决于`Logical Block`的大小。在`Blobstore`中，`Page`是连续的，即从`SSD`的`LBA 0`开始，多个或一个块构成`Page 0`,接下来是`Page 1`，依次类推。
- **Cluster**：由多个连续的`Page`构成，通常一个`Cluster`的大小默认为`1MiB`，因此一个`Cluster`由`256个Page`构成。`Cluster`与`Page`一样，是连续的，即从`SSD`的`LBA 0`开始的位置依次为`Cluster 0`到`Cluster N`。
- **Blob**：`Blobstore`中主要的操作对象为`Blob`，与`BlobFS`中的文件相对应，提供`read`、`write`、`create`、`delete`等操作。一个`Blob`由多个`Cluster`构成，但构成`Blob`中的`Cluster`并不一定是连续的。

##### 特殊块

在`Blobstore`中，会将`cluster 0`作为一个特殊的`cluster`。该`cluster`用于存放`Blobtore`的所有信息以及元数据，对每个`blob`数据块的查找、分配都是依赖`cluster 0`中所记录的元数据所进行的。`Cluster 0`的结构如下：

![cluster0](/docs/files/cluster0.jpg)

`Cluster 0`中的第一个`page`作为`super block`，`Blobstore`初始化后的一些基本信息都存放在`super block`中，例如`cluster`的大小、已使用`page`的起始位置、已使用`page`的个数、已使用`cluster`的起始位置、已使用`cluster`的个数、`Blobstore`的大小等信息。

`Cluster 0`中的其它`page`将组成元数据域`（metadata region）`。元数据域主要由以下几部分组成：

![metadata](/docs/files/metadata.jpg)

- **Metadata Page Allocation**：用于记录所有元数据页的分配情况。在分配或释放元数据页后，将会对`metadata page allocation`中的数据做相应的修改。
- **Cluster Allocation**：用于记录所有`cluster`的分配情况。在分配新的`cluster`或释放`cluster`后会对`cluster allocation`中的数据做相应的修改。
- **Blob Id Allocation**：用于记录`blob id`的分配情况。对于`blobstore`中的所有`blob`，都是通过唯一的标识符`blob id`将其对应起来。在元数据域中，将会在`blob allocation`中记录所有的`blob id`分配情况。
- **Metadata Pages Region**：元数据页区域中存放着每个`blob`的元数据页。每个`blob`中所分配的`cluster`都会记录在该`blob`的元数据页中，在读写`blob`时，首先会通过`blob id`定位到该`blob`的元数据页，其次根据元数据页中所记录的信息，检索到对应的`cluster`。对于每个`blob`的元数据页，并不是连续的。

对于一个`blob`来说，`metadata page`记录了该`blob`的所有信息，数据存放于分配给该`blob`的`cluster`中。在创建`blob`时，首先会为其分配`blob id`以及`metadata page`，其次更新`metadata region`。当对`blob`进行写入时，首先会为其分配`cluster`，其次更新该`blob`的`metadata page`，最后将数据写入，并持久化到磁盘中。

为了实现对磁盘空间的动态分配管理，`Blobstore`中为每个`blob`分配的`cluster`并不是连续的。对于每个`blob`，通过相应的结构维护当前使用的`cluster`以及`metadata page`的信息：`clusters`与`pages`。`Cluster`中记录了当前该`blob`所有`cluster`的`LBA`起始地址，`pages`中记录了当前该`blob`所有`metadata page`的`LBA`起始地址。

`Blobstore`实现了对磁盘空间分配的动态管理，并保证断电不丢失数据，因此`Blob`具有`persistent`特性。`Blobstore`中的配置信息与数据信息均在`super block`与`metadata region`中管理，在重启后，若要保持`persistent`，可以通过`Blobstore`中所提供的`load`操作。

### NBFS详细设计

自古鱼和熊掌不可兼的，既保证文件系统的通用性又要追求极致的性能，这在短时间内是难以完成的，所以我们选择实现两套接口：

- 通用接口：以通用性为第一目标，可对接`SPDK`提供的`FUSE`插件，进而可被正常软件调用，在此基础上，利用`SPDK`的特性提高其性能。
- 专用接口：以性能提升为第一目标，直接利用`SPDK`平台搭建一套异步、无锁、并发的文件系统，实现我们所设想的性能提升。

#### 通用接口设计

##### 整体架构

利用`SPDK`搭建`NBFS`整体架构如下：

![fs_design](/docs/files/fs_design.png)

我们利用`SPDK`提供的用户态框架，使`NBFS`直接与`NVMe SSD`通信，减少了传统文件系统多层切换，与此同时，由于整套系统处于用户态，减少了用户态与内核态的切换开销。

#####  B+树

`NBFS`采用`B+`树作为文件的组织形式，`Key`值为文件名，叶子节点指向文件结构体`spdk_file`:

```c
struct spdk_file
{
    struct spdk_filesystem *fs;     //指向文件系统
    struct spdk_blob *blob;         //文件存储物理块
    char *name;                     //文件名
    uint64_t length;                //文件长度
    bool is_deleted;
    bool open_for_writing;
    uint64_t length_flushed;
    uint64_t length_xattr;
    uint64_t append_pos;
    uint64_t seq_byte_count;
    uint64_t next_seq_offset;
    uint32_t priority;

    spdk_blob_id blobid;
    uint32_t ref_count;
    pthread_spinlock_t lock;
    struct cache_buffer *last;
    struct cache_tree *tree;

    /* 消息队列 */
    TAILQ_HEAD(open_requests_head, spdk_fs_request)
    open_requests;
    TAILQ_HEAD(sync_requests_head, spdk_fs_request)
    sync_requests;
    
    ......
};
```

C语言不提供B+树操作，所以我们专门实现了一个B+树的库文件，里面包含B+树的主要操作：

```c
/* 初始化一颗B+树 */
struct node *B_plus_tree_initial(void);

/* 查找文件 */
struct record *B_plus_tree_find(node *root, const char *key);

/* 插入文件 */
struct node *B_plus_tree_insert(node *root, const char *key, struct spdk_file *file);

/* 删除文件 */
struct node *B_plus_tree_remove(node *root, const char *key);

/* 销毁文件系统 */
void delete_B_plus_tree(node *root);

/* 获取文件系统中第一个文件 */
struct record *B_plus_tree_get_first(node *root);

/* 获取文件系统中current文件的下一个文件 */
struct record *B_plus_tree_get_next(node *root, struct spdk_file *current);

/* 遍历叶子节点 */
void traverse_leaves(node *root, void (*fn)(struct spdk_file *file));

/* B+树是否为空 */
bool B_plus_tree_empty(node *root);
```

##### 创建/打开/重命名文件

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

##### 读/写文件

文件读取操作的流程图如下所示：

![read](/docs/files/read.jpg)

为了提高文件的读取效率，`NBFS`在内存中提供了`cache buffer`。在文件读写时，首先会进行`read ahead`操作，将一部分数据从磁盘预先读取到内存的`buffer`中。其后，根据`cache buffer`的大小，对文件的`I/O`进行切分，使每个`I/O`的最大长度不超过一个`cache buffer`的大小。对于拆分后的文件`I/O`，会根据其`offset`在`cache buffer tree`中查找相应的`buffer`。

若存在，则直接从`cache buffer`中读取数据，进行`memcpy`。而对于没有缓存到`cache buffer`中的数据，将会对该文件的读取，转换到该文件对应的`blob`进行读取。对`blob`读取时候，根据已打开的`blob`结构中记录的信息，可以获取该 `blob`所有`cluster`的`LBA`起始位置，并根据读取位置的`offset`信息，计算相应的`LBA`地址。最后向`SPDK bdev`层发送异步的读请求，并等待`I/O`完成。`NBFS`所提供的读操作为同步读，`I/O`完成后会在`callback`函数中，通过信号量通知`NBFS`完成信号，至此文件读取结束。

文件写入操作的流程图如下所示：

![write](/docs/files/write.jpg)

在进行文件写入时，首先会根据文件当前的写入位置检查是否符合`cache buffer`写入需求，若满足，则直接将数据写入到`cache buffer`中，同时触发异步的`flush`操作。在`flush`的过程中，`NBFS`触发`blob`的写操作，将`cache buffer`中的数据，写入到文件对应`blob`的相应位置。

若不满足`cache buffer`的写入需求，`NBFS`则直接触发文件对应的`blob`的写操作。`Blobstore`首先为该`blob`分配`cluster`，根据计算得到的写入`LBA`信息，向`SPDK bdev`层发送异步的写请求，将数据写入，并更新相应的元数据。

对于元数据的更新，出于性能考虑，当前对元数据的更新都在内存中操作，当用户使用强制同步或卸载`Blobstore`时，更新后的元数据信息才会同步到磁盘中。此外，`blob`结构中维护了两份可变信息（指`cluster`与`metadata page`）的元数据，分别为`clean`与`active`。`clean`中记录的是当前磁盘的元数据信息，而`active`中记录的是当前在内存中更新后的元数据信息。同步操作会将`clean`中记录的信息与`active`记录的信息相匹配。

具体的函数调用过程如下：

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

#### 专用接口设计

##### Blob有关数据

这里简要列出Blobstore定义的有关Blob结构体，后面的接口设计将直接基于blob操作.

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

##### Superblob

`blobstore`提供了一个`superblob`的概念，对它的读写和获取相较于其他的普通`blob`更加方便，因此我们使用`superblob`来存储文件系统的文件信息数据，例如为了保存文件名和文件对应`blobid`的数据，可以将文件名和`blobid`按序存储在`superblob`的数据区中，然后使用`superblob`的`x_attr`存储一个键值对，`"SUPERBLOB_DATA_SIZE": (size_t) data_size`，用于保存文件数据的长度，从而可以在读取数据时确定数据长度，如下：

<img src="/docs/files/superblob.svg" style="zoom:150%;" />

##### 文件查找

使用哈希表的方式查找，当文件系统新建或删除一个文件时，将文件名作为键，文件位置作为值插入到文件系统内部的哈希表中：

![hash_table](/docs/files/hash_table.svg)

当文件系统卸载时，会遍历哈希表的表项，将表中数据按序存储到`superblob`中：

![unload](/docs/files/unload.svg)

当文件系统装载时，会将`superblob`的数据依次读取并插入哈希表中，从而重建哈希表：

![load](/docs/files/load.svg)

##### 读写数据

NBFS的第二套接口设计了封装好的异步读写函数，且对于每一个读写单元，可以单独分配`io_channel`用于并行处理，从而实现并发性。

异步函数指的是：例如调用`nbfs2_write_file`时，需要传入一个回调函数`cb_fn`以及对应的回调参数`cb_arg`，应用程序执行`nbfs2_write_file`函数时，会立即返回，应用程序代码中该函数之后的部分会无等待的进行，而`cb_fn`则会在文件写入磁盘之后再被调用，并且传入`cb_arg`以及`errno`参数。

## 挂载与测试

#### 通用接口挂载FUSE测试

由于SPDK的挂载将会抹除整块盘的数据，所以我们准备一块空闲盘插入电脑的第二条M.2插槽中，具体流程如下:

- **Step1 编译**

```
./configure --with-fuse
make
```

- **Step2 获取空闲NVMe设备，并初始化文件系统**

在`SPDK`目录下输入命令：

```
HUGEMEM=5120 scripts/setup.sh
scripts/gen_nvme.sh --json-with-subsystems > rocksdb.json
test/blobfs/mkfs/mkfs rocksdb.json Nvme0n1
```
![load_filesysterm](/docs/files/load_filesysterm.png)

- **Step3 使用FUSE**

安装`fuse3`组件:

```
sudo apt-get install libfuse3-dev
```

创建`fuse`挂载点：

```
midir /mnt/fuse
```

在`SPDK`目录下，输入以下命令将文件系统挂载至`FUSE`：

```
test/blobfs/fuse/fuse rocksdb.json Nvme0n1 /mnt/fuse
```

![fuse](/docs/files/fuse.png)

- **Step4 运行**

在`Step3`之后，终端会被阻塞，新打开一个终端，使用cd命令进入挂载点`/mnt/fuse`,接着可以使用常见的`shell`指令对文件操作。

![test](/docs/files/test.png)

结果表明，文件系统正常工作。

#### 专用接口性能测试

测试过程在此省略，过程截图如下：

![test](/docs/files/multi_io_test.png)

由于NBFS的设计特性是异步、无锁、并发以及对`NVMe`硬盘队列的支持，且专用接口不需要进行上下文切换，在频繁的请求时减少了大量的切换开销，因此对于高`IO`并发的文件应用速度提升会更明显。于是我们测试了NBFS和普通文件系统在处理高并发`IO`程序时所花的时间，即采用写入指数级增长的`IO`量进行读写，进行时间的对比，如下：

<img src="/docs/files/multi_io.png" style="zoom:67%;" />

可以看出，当`IO`量很大时，`NBFS`由于使用并行的`io_channel`处理文件存储，很明显提高了读写性能；而对于小`IO`的读写，两者并没有什么差异，这是因为硬盘响应时间限制了`IO`的速度。

## 项目总结与前景展望

### NBFS的特点

我们实现的基于`SPDK`的文件系统`NBFS`具有以下特点：

#### 异步无锁并发

异步，即指对文件的操作是异步的，由于采用了回调函数的机制，发送读写文件命令后不必等待，而是可以做其他事情，当完成`I/O`后会通过回调函数告知；

无锁，即指对文件的操作不必加锁，这是因为`NVMe`具有多`I/O`的特点，当有对同一个文件的多个访问时，可以给每个访问分配一个`I/O`通道，而不必用软件加锁的办法；

并发，同样利用`NVMe`多`I/O`特性实现大量`I/O`的并发执行。

#### 采用轮询方式而不是中断

这是基于`SPDK`的特性，由于`NVMe`读写延迟相较于传统机械硬盘显著降低，所以根据我们所查询的期刊文献，轮询花费时间比中断开销更低，当然这种方式会让`CPU`资源占用率提高，在小`I/O`量的时候得不偿失，但是在高`I/O`请求时，这种方式将带来更大提升，这点在上面的测试结果中已经体现。

#### 直接与NVMe设备通信，规避多次内核态与用户态切换造成的性能损失

这也是利用`SPDK`在用户态的特性，避免了内核态、用户态的切换。

### 项目经过

为了实现文件系统，我们翻阅了`SPDK`各种示例程序，`Blobstore`库函数等将近**4000**行代码，在学习过程中，我们仿照实例程序以及简陋的官方文档，一步步搭建文件系统，完成了**2000**多行代码的编写与调试，实现了两套接口并分别做了针对性的测试。

是小组成员间的密切配合，才让这个项目按照预定计划按时完成。

### 存在的不足

- 还未支持文件夹结构，无法使用`mkdir`指令，后续可以在`B+`树的基础上再作修改完善；
- 我们原本打算实现绕过`FUSE`直接将`Blobstore`封装成类似于系统调用的函数，这样可以完全`bypass`内核，让通用应用程序在用户态访问硬件，但是由于涉及到十多个文件，上万行代码的阅读学习，再加上之前没有类似的项目可供参考，花了大量时间也没能实现，遂放弃，如果时间足够充裕，相信能实现兼具通用性与效率的`NBFS`终极进化版。

### 前景展望

`NBFS`的通用接口在保留通用性的基础上利用`SPDK`实现了性能的提升，用户程序可不做修改即可获得比传统文件系统更高速的`I/O`性能；

`NBFS`的专用接口则充分利用`NVMe`及`SPDK`的特性，以性能提升为第一目标，同时它又不像直接访问`Blob`一样需要对底层有充分了解才能实现，我们的接口在最大化性能的同时对`Blob`做了一层封装，对于小团队要实现高性能`I/O`是一个很好的选择。

## 参考文献

* File system. (2021, March 29). Retrieved April 14, 2021, from https://en.wikipedia.org/wiki/File_system/.
* NVM express. (2021, March 26). Retrieved April 14, 2021, from https://en.wikipedia.org/wiki/NVM_Express.
* White papers. (2016, August 05). Retrieved April 14, 2021, from https://nvmexpress.org/white-papers/.
* “Filesystem in Userspace.” *Wikipedia*, Wikimedia Foundation, 23 Mar. 2021, from https://en.wikipedia.org/wiki/Filesystem_in_Userspace. 
* Libfuse. “Libfuse/Libfuse.” *GitHub*, https://github.com/libfuse/libfuse. 
* Storage performance development kit. Retrieved April 14, 2021, from https://spdk.io/.
* Nvfuse. “Nvfuse/Nvfuse.” *GitHub*, https://github.com/nvfuse/nvfuse. 
* Y. Tu, Y. Han, Z. Chen, Z. Chen and B. Chen, "URFS: A User-space Raw File System based on NVMe SSD," 2020 IEEE 26th International Conference on Parallel and Distributed Systems (ICPADS), Hong Kong, pp. 494-501, 2020.
* Q. Zhang, D. Feng, F. Wang and Y. Xie, "An Efficient, QoS-Aware I/O Scheduler for Solid State Drive," 2013 IEEE 10th International Conference on High Performance Computing and Communications & 2013 IEEE International Conference on Embedded and Ubiquitous Computing, Zhangjiajie, China, pp. 1408-1415, 2013.
* A. Demers, S. Keshav, and S. Shenker. "Analysis and simulation of a fair queuing algorithm", Journal of Internetworking Research and Experience, vol. 1, no. 1, pp. 3-26, 1990.
* P. Goyal, H. M. Vin, and H. Cheng. "Start-time fair queuing: A scheduling algorithm for integrated services packet switching networks", Technical Report CS-TR-96-02, UT Austin, January, 1996.
* A. L. N. Reddy and J. Wyllie, "Disk scheduling in a multimedia I/O system", Proc. of the 1st ACM international conference on Multimedia, pp. 225-233, 1993.
* A. Povzner, D. Sawyer and S. A. Brandt, "Horizon: efficient deadlinedriven disk I/O management for distributed storage systems", In Proc. of the 19th International Symposium on High Performance Distributed Computing, pp. 1-12, 2010.
* Yang Ziye, Harris J R, Walker B, et al. SPDK: a development kit to build high performance storage applications // Proceedings of the IEEE International Conference on Cloud Computing Technology and Sci-ence. Hong Kong, 2017: 154–161
* Weil S A. Goodbye XFS: building a new faster storage backend for ceph [EB/OL]. (2017–09–12) [2020–02–01]. https://www.snia.org/sites/default/files/DC/2017/presentations/General_Session/Weil_Sage%20_Red_Hat_Goodbye_XFS_Building_a_new_faster_storage_backend_for_Ceph.pdf
* Lee D Y, Jeong K, Han S H, et al. Understanding write behaviors of storage backends in ceph object store. Proceedings of the IEEE International Con-ference on Massive Storage Systems and Technology. Santa Clara, CA, 2017: 1–10
* Yongseok O. NVMe based file system in user-space [EB/OL]. (2018–12–09) [2020–02–01]. https://github.com/NVFUSE/NVFUSE
* Liu J, Andrea C A, Remzi H, et al. File systems as processes. Proceedings of the 11th USENIX Work-shop on Hot Topics in Storage and File Systems. Ber-keley, 2019: No. 14
* Kim H J, Lee Y S, Kim J S. NVMeDirect: a user-space I/O framework for application-specific optimi-zation on NVMe SSDs. Proceedings of the 8th USENIX Workshop on Hot Topics in Storage and File Systems. Berkeley, 2016: 41–45
* Kim H J. NVMeDirect _v2 forestfs [EB/OL]. (2018–05–03) [2020–02–01].http://github.com/nvmedirect/nvmedirect_v2/tree/master/forestfs
* Ahn J S, Seo C, Mayuram R, et al. ForestDB: a fast key-value storage system for variable-length string keys. IEEE Transactions on Computers, 2015, 65(3): 902–915
* Mircon Technology Inc. UNMe [EB/OL]. (2019–05–02) [2020–02–01]. https://github.com/MicronSSD/unvme
* Mircon Technology Inc. User space nameless file-system [EB/OL]. (2017–04–07) [2020–02–01]. https://github.com/MicronSSD/UNFS
* Hu Z, Chidambaram V. A rust user-space file system [EB/OL]. (2019–05–08) [2020–02–01]. https://github.com/utsaslab/rustfs
* Caulfield A M, Mollov T I, Eisner L A, et al. Provi-ding safe, user space access to fast, solid state disks. ACM SIGARCH Computer Architecture News, 2012, 40(1): 387–400
* Rajachandrasekar R, Moody A, Mohror K,et al. A 1 PB/s file system to checkpoint three million MPI tasks. Proceedings of the 22nd ACM International Symposium on High-Performance Parallel and Distri-buted Computing. New York, 2013: 143–154
* Wang T, Mohror K, Moody A, et al. An ephemeral burst-buffer file system for scientific applications. Proceedings of the International Conference for High Performance Computing, Networking, Storage and Analysis. Salt Lake City, 2016: 807–818
* Bessani A, Mendes R, Oliveira T, et al. SCFS: a shared cloud-backed file system. Proceedings of the USENIX Annual Technical Conference. Berkeley, 2014: 169–180
* Vrable M, Savage S, Voelker G M. BlueSky: a cloud-backed file system for the enterprise. Proceedings of the 10th USENIX Conference on File and Storage Technologies. Berkeley, 2012: 1–14
* Y. T. Jin, S. Ahn and S. Lee, "Performance Analysis of NVMe SSD-Based All-flash Array Systems," 2018 IEEE International Symposium on Performance Analysis of Systems and Software (ISPASS), Belfast, UK, pp. 12-21, 2018.
* A. Tavakkol et al., "FLIN: Enabling Fairness and Enhancing Performance in Modern NVMe Solid State Drives," 2018 ACM/IEEE 45th Annual International Symposium on Computer Architecture (ISCA), Los Angeles, CA, USA, pp. 397-410, 2018.
* 看文件系统结构如何降低nvme性能. (2019, January 02). Retrieved April 14, 2021, from https://searchstorage.techtarget.com.cn/6-27904/
* Written by Michael Larabel in Storage on 7 January 2019. Page 1 of 4. 61 Comments. (2019, January 7). Linux 5.0 File-System Benchmarks: Btrfs vs. ext4 VS. F2FS Vs. xfs. Retrieved April 14, 2021, from https://www.phoronix.com/scan.php?page=article&item=linux-50-filesystems&num=1