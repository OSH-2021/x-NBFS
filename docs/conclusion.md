# 结题报告

- [结题报告](#结题报告)
  - [项目简介](#项目简介)
  - [项目背景](#项目背景)
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
      - [BlobFS](#blobfs)
    - [NBFS详细设计](#nbfs详细设计)
      - [通用接口设计](#通用接口设计)
      - [专用接口设计](#专用接口设计)
        - [Superblob](#superblob)
        - [文件查找](#文件查找)
        - [读写数据](#读写数据)
  - [挂载与测试](#挂载与测试)
      - [通用接口挂载FUSE测试](#通用接口挂载fuse测试)
      - [专用接口性能测试](#专用接口性能测试)
  - [项目总结与前景展望](#项目总结与前景展望)
  - [参考文献](#参考文献)


## 项目简介

本项目旨在设计和实现一个针对改进`NVMe SSD`读写性能的文件系统，并基于`SPDK`工具实现了该文件系统，同时提供了两套用于访问底层设备的接口，一套是用于`FUSE`挂载的通用接口，另一套是用于专用应用的专用接口。

## 项目背景

近几年存储行业发生了翻天覆地的变化，半导体存储登上了历史的舞台。和传统磁盘存储介质相比，半导体存储介质具有天然的优势。无论在可靠性、性能、功耗等方面都远远超越传统磁盘。目前常用的半导体存储介质是`NVMe SSD`，采用`PCIe`接口方式与主机进行交互，大大提升了性能，释放了存储介质本身的性能。

存储介质的革命一方面给存储系统性能提升带来了福音；另一方面对文件系统的设计带来了诸多挑战。原有面向磁盘设计的文件系统不能充分利用新型存储介质，传统的内核态文件系统过于厚重, 内核存储栈存在大量的拷贝、上下文切换以及中断等软件开销, 导致读写延迟较大, 无法充分发挥`NVMe SSD`的硬件性能。

### FUSE文件系统

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

#### BlobFS

TODO

### NBFS详细设计

自古鱼和熊掌不可兼的，既保证文件系统的通用性又要追求极致的性能，这在短时间内是无法完成的，所以我们选择实现两套接口：

- 通用接口：以通用性为第一目标，可对接`SPDK`提供的`FUSE`插件，进而可被正常软件调用，在此基础上，利用`SPDK`的特性提高其性能。
- 专用接口：以性能提升为第一目标，直接利用`SPDK`平台搭建一套异步、无锁、并发的文件系统，实现我们所设想的性能提升。

#### 通用接口设计

TODO

#### 专用接口设计

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

TODO

#### 专用接口性能测试

由于NBFS的设计特性是异步、无锁、并发以及对`NVMe`硬盘队列的支持，且专用接口不需要进行上下文切换，在频繁的请求时减少了大量的切换开销，因此对于高`IO`并发的文件应用速度提升会更明显。于是我们测试了NBFS和普通文件系统在处理高并发`IO`程序时所花的时间，即采用写入指数级增长的`IO`量进行读写，进行时间的对比，如下：

<img src="docs/files/multi_io.png" style="zoom:67%;" />

可以看出，当`IO`量很大时，`NBFS`由于使用并行的`io_channel`处理文件存储，很明显提高了读写性能；而对于小`IO`的读写，两者并没有什么差异，这是因为硬盘响应时间限制了`IO`的速度。

## 项目总结与前景展望

TODO

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