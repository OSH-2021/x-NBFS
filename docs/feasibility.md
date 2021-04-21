# 可行性报告

- [可行性报告](#可行性报告)
  - [项目介绍](#项目介绍)
  - [理论依据](#理论依据)
    - [内核文件系统和用户态文件系统](#内核文件系统和用户态文件系统)
      - [FUSE简介](#fuse简介)
      - [选择FUSE的原因](#选择fuse的原因)
    - [各类面向NVMe SSD 的用户态框架、驱动及文件系统](#各类面向nvme-ssd-的用户态框架驱动及文件系统)
      - [SPDK及相应的文件系统](#spdk及相应的文件系统)
      - [NVMeDirect及相应的文件系统](#nvmedirect及相应的文件系统)
      - [UNVMe及相应的文件系统](#unvme及相应的文件系统)
      - [其它面向NVMe SSD的用户态文件系统](#其它面向nvme-ssd的用户态文件系统)
  - [技术依据](#技术依据)
    - [SPDK](#spdk)
      - [SPDK简介](#spdk简介)
      - [选择SPDK的原因](#选择spdk的原因)
    - [NVFUSE](#nvfuse)
      - [NVFUSE简介](#nvfuse简介)
      - [选择NVFUSE的原因](#选择nvfuse的原因)
    - [测试工具](#测试工具)
  - [技术路线](#技术路线)
    - [文件系统设计](#文件系统设计)
    - [FUSE使用](#fuse使用)
    - [测试文件系统性能](#测试文件系统性能)
  - [参考文献](#参考文献)

## 项目介绍

近几年存储行业发生了翻天覆地的变化，半导体存储登上了历史的舞台。和传统磁盘存储介质相比，半导体存储介质具有天然的优势。无论在可靠性、性能、功耗等方面都远远超越传统磁盘。目前常用的半导体存储介质是`NVMe SSD`，采用PCIe接口方式与主机进行交互，大大提升了性能，释放了存储介质本身的性能。

存储介质的革命一方面给存储系统性能提升带来了福音；另一方面对文件系统的设计带来了诸多挑战。原有面向磁盘设计的文件系统不能充分利用新型存储介质，传统的内核态文件系统过于厚重, 内核存储栈存在大量的拷贝、上下文切换以及中断等软件开销, 导致读写延迟较大, 无法充分发挥`NVMe SSD`的硬件性能。 

本次实验项目将利用`SPDK`以及用户态文件系统`FUSE`设计基于用户态的文件系统，以提升文件系统性能与效率。

## 理论依据

### 内核文件系统和用户态文件系统

传统上操作系统在内核层面上对文件系统提供支持，但通常内核态的代码难以调试，内核下定制开发文件系统难度较高，生产率较低，在用户空间可定制实现文件系统是有需求的。并且用户空间下调试工具丰富，出问题不会导致系统崩溃，开发难度相对较低，开发周期较短。于是，用户态文件系统FUSE成为当下的热门研究方向。

#### FUSE简介
用户空间文件系统（`Filesystem in Userspace`，简称`FUSE`）是一个面向类`Unix`计算机操作系统的软件接口，它使无特权的用户能够无需编辑内核代码而创建自己的文件系统。

`FUSE`包含两个组件：内核模块（在常规系统内核存储库中维护）和用户空间库（`libfuse`）。`FUSE`文件系统通常实现为与`libfuse`链接的独立应用程序。`libfuse`提供了以下功能：挂载文件系统，卸载文件系统，从内核读取请求以及将响应发送回。

在`Linux`中，对文件的访问都是统一通过VFS层提供的内核接口进行的（比如`open/read`），因此当一个进程（称为`"user"`）访问用户态文件系统时，依然需要途径`VFS`。当`VFS`接到`user`进程对文件的访问请求，并且判断出该文件是属于某个用户态文件系统，就会将这个请求转交给一个名为`"fuse"`的内核模块；而后，`"fuse"`将该请求传给用户态进程（如下图）。

![fuse](/docs/files/2_fuse.png)

`FUSE`作为用户态文件系统使用、调试方便，但由于其额外的内核态/用户态切换会影响性能。

#### 选择FUSE的原因

对于针对`NVMe`优化的文件系统，我们选择采用**用户态文件系统**来实现，原因如下：

- **相比于内核态文件系统，基于用户态的文件系统便于开发与调试；**
- **传统的内核态文件系统过于厚重，频繁的内核态与用户态文件的拷贝将严重影响效率；**
- **利用SPDK的特性，我们可以在用户态直接向NVMe SSD提交I/O请求，从而规避FUSE额外的内核态、用户态切换，真正提高效率。**

### 各类面向NVMe SSD 的用户态框架、驱动及文件系统

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

**综上所述, 面向 NVMe SSD 的用户态框架、驱动和文件系统的优势在于, 支持零拷贝, 以轮询方式访问来减少中断, 并针对 SSD 的某些特性做了相关的优化, 可以减少软件带来的开销。存在的共性问题在于文件系统布局无法兼顾空间利用率和读写性能, 未充分利用 NVMe SSD 多队列特性的性能优势, 难以支持多个应用程序对 SSD 的高效共享访问, 这些都制约了 NVMe SSD 的发展。**

## 技术依据

### SPDK

#### SPDK简介
`The Storage Performance Development Kit`（`SPDK`）提供了一套工具和库，用于编写高性能、可扩展的用户模式存储应用程序。它通过使用一些关键技术来实现高性能：

- 将所有必要的驱动程序移入用户空间。这样可以避免系统调用，并实现应用程序的零拷贝访问。
- 对硬件进行轮询，而不是使用中断，这降低了总延迟和延迟波动。
- 避免I/O路径中的所有锁，而是依靠消息传递。

`SPDK`的底层是一个用户空间、`polled`模式、异步、无锁的`NVMe`驱动。它提供了从用户空间应用的零拷贝、高度并行的直接访问。该驱动以C语言库的形式编写，且只有一个公共头。

`SPDK`还提供了一个完整的区块堆栈，作为用户空间库，执行大量与操作系统中区块堆栈相同的操作。这包括统一不同存储设备之间的接口，利用队列处理内存不足或I/O挂起等情况，以及逻辑分区的管理。

最后，`SPDK`提供了建立在这些组件之上的`NVMe-oF`、`iSCSI`和`vhost`服务器，它们能够通过网络或向其他进程提供磁盘服务。`NVMe-oF`和`iSCSI`的标准`Linux`内核启动器可以与这些目标互操作，`QEMU`也可以与`vhost`互操作。这些服务器的CPU的执行效率可以比其他实现高出一个数量级。这些目标可以作为如何实现高性能存储目标的例子，或者作为生产部署的基础。

#### 选择SPDK的原因

- `SPDK`通过针对性的优化，例如驱动程序移入用户空间、采用轮询等，能充分利用`NVMe`的性能；
- 由于`SPDK`直接与`NVMe`设备通信，能规避用户态文件系统`FUSE`多次内核态与用户态切换造成的性能损失。

### NVFUSE

#### NVFUSE简介

`NVFUSE`是一个结合了`SPDK`库的可嵌入的文件系统。`SPDK`库是英特尔最新推出的用户空间的`NVMe`驱动。使用了这种文件系统的应用可以直接提交I/O请求给`NVMe`的`SSD`。它提高了硬盘的性能和可靠性，还提供了类`POSIX`的接口（如`nvfuse_open`, `nvfuse_read`, `nvfue_write`, `nvfuseclose`这样的函数）。注意这个文件系统并不是利用众所周知的`FUSE`(`File System in Userspace`)库来实现`POSIX`的兼容性。

主要特性如下：
- 在用户空间中运行，不需要与内核驱动进行任何交互，因此不需要中断、上下文切换和内存复制。
- 作为库的可嵌入的文件系统，引进了类似`POSIX API`的新接口。
- 简单的文件系统布局，与`EXT`系列文件系统（如`EXT3`）相同。
- 通过`NVMe`规范所描述的`NVMe`元数据功能，改善了日志记录机制，提高了文件系统一致性和耐久性。

#### 选择NVFUSE的原因

`NVFUSE`是基于`SPDK`构建的文件系统，而且在用户空间中运行，与我们的方向相适应，适合作为我们所要设计的文件系统的基础，也便于之后使用`SPDK`进一步进行应用方面的优化。


### 测试工具

测试工具大多分为这几类：

- 合成IO测试：根据统计的真实负载发生规律，如请求的读写比例，大小，频率和分布等信息。建立响应的IO存取模型。在测试时产生符合存取模型的IO请求序列。发送给存储系统。
 - 基准测试集：使用基准测试集测试计算机系统的性能，一直是有效和精确的评价方法。针对存储系统的测试研究也大量使用基准测试集。例如存储性能委员会`SPC`为存储系统开发的基准测试集`SPC-1`,`SPC-2`。
 - 基于`Trace`的测试：`Trace `测试是搜集真实系统中所有的IO请求信息，并按照一定格式记录在`Trace`文件中，一般包括请求时间，请求类型和请求大小等。测试时，程序按照`Trace`文件中的记录想存储系统中发出IO请求。但是记录`Trace`信息会增加IO开销。

以下是一些文件系统测试工具：

1. [`pjd-fstest`(`posix` 接口兼容性测试)](https://github.com/pjd/pjdfstest): `fstest`是一套简化版的文件系统`POSIX`兼容性测试套件，它可以工作在`FreeBSD`, `Solaris`, `Linux`上用于测试`UFS`, `ZFS`, `ext3`, `XFS` 和 `NTFS-3G`等文件系统。`fstest`目前有3601个回归测试用例，测试的系统调用覆盖`chmod`, `chown`, `link`, `mkdir`, `mkfifo`, `open`, `rename`, `rmdir`, `symlink`, `truncate`, `unlink`。
2. [`IOZone`(读写模式测试)](https://www.iozone.org/): `IOZone`是目前应用非常广泛的文件系统测试标准工具，它能够产生并测量各种的操作性能，包括`read`, `write`, `re-read`, `re-write`, `read backwards`, `read strided`, `fread`, `fwrite`, `random read`, `pread`, `mmap`, `aio_read`, `aio_write`等操作。`IOZone`目前已经被移植到各种体系结构计算机和操作系统上，广泛用于文件系统性能测试、分析与评估的标准工具。
3. [`FIO`(顺序、随机IO测试)](https://github.com/axboe/fio): `flexible I/O tester`. `FIO`可以模拟给定的IO工作负载而无需编写量身定制的测试案例。它支持13种不同类型的I/O引擎（`sync`, `mmap`, `libaio`, `posixaio`, `SG v3`, `splice`, `null`, `network`, `syslet`, `guasi`, `solarisaio`等），`I/O priorities(for newer Linux kernels)`, `rate I/O`, `forked or threaded jobs`等等。`fio`可以支持块设备和文件系统测试，广泛用于标准测试、QA、验证测试等，支持`Linux`, `FreeBSD`, `NetBSD`, `OS X`, `OpenSolaris`, `AIX`, `HP-UX`, `Windows`等操作系统。
4. [`FIlebench`(文件系统应用负载生成测试)](https://github.com/filebench/filebench)`Filebench` 是一款文件系统性能的自动化测试工具，它通过快速模拟真实应用服务器的负载来测试文件系统的性能。它不仅可以仿真文件系统微操作（如 `copyfiles`, `createfiles`, `randomread`, `randomwrite` ），而且可以仿真复杂的应用程序（如 `varmail`, `fileserver`, `oltp`, `dss`, `webserver`, `webproxy` ）。` Filebench` 比较适合用来测试文件服务器性能，但同时也是一款负载自动生成工具，也可用于文件系统的性能。
5. [`IOR/mdtest`(利用并行IO来测试文件系统的IO性能和元数据性能)](https://github.com/hpc/ior) `IOR`是并行IO基准，可用于使用各种接口和访问模式来测试并行存储系统的性能。`IOR`存储库还包括`mdtest`基准测试，该基准测试专门测试不同目录结构下存储系统的峰值元数据速率。这两个基准测试均使用通用的并行I / O抽象后端，并依赖`MPI`进行同步。
6. [`dd-benchmark`](https://www.gnu.org/software/coreutils/manual/html_node/dd-invocation.html#dd-invocation) `dd`是`linux`内核程序，但可以用其测试文件系统的各种性能（因此不是自动测试程序，需要自己定义测试内容）

## 技术路线

### 文件系统设计

**Step1 通过FUSE搭建一个用户态文件系统，其文件结构采用类似EXT4的方式设计**

`FUSE`文件系统通常实现为与`libfuse`链接的独立应用程序。`libfuse`提供了挂载文件系统，卸载文件系统，从内核读取请求以及将响应返回的功能。`libfuse`提供了两个`API`：“高级”同步`API`和“低级”异步`API`。在这两种情况下，来自内核的传入请求都使用回调传递给主程序。

在我们的文件系统中，我们将通过"低级"`API`，即回调函数需与`inode`一起使用。	

通过`FUSE`的`API`我们将重新设计`mkdir`, `mknod`, `truncate`, `read`, `write`等基本文件操作，以及一些更底层的`inode`设置以及中断、管道和信号量的配置函数。

**Step2 在基本设计完成一个用户态文件系统后，通过SPDK改进其读写等操作性能**

由于上述文件系统底层仍通过`FUSE`所设计的`用户请求 -> VFS -> FUSE内核模块 -> 原路返回`的过程，因此并不能提升性能。我们将会参考`NVFUSE`的设计方式，通过修改用户态文件系统和部分`FUSE`系统函数，结合`SPDK`在用户态就可以发送I/O请求的特性，对文件系统的进行改进，使其加速在`NVMe SSD`上的文件操作。

例如对于`write`函数，在已经实现好的用户态文件系统中，是根据我们定义的模式调用`FUSE`提供的内核程序写数据到存储器，在经过这一步修改之后，就可以调用`SPDK`提供的`spdk_bdev_write()`程序直接将数据写入到设备。

![Design](/docs/files/Design.png)

**Step3 通过各种算法改进文件系统在NVMe SSD上的性能**

多篇学术界权威论文指出，优化我们的文件系统可以从下面几个部分入手：

- 传统文件系统不支持共享访问`SSD`，因此可以通过管理维护共享内存，使得多个应用程序可以访问多个`NVMe SSD`，从而加速访问`SSD`的性能；
- 在`Linux`内核中当前的`NVMe`驱动程序会为主机系统中的每个内核创建一个提交队列和一个完成队列，在我们的系统中，可以设计算法灵活分配I/O，并根据工作负载类别动态评估分离到不同的队列中；
- 在保证性能的同时兼顾调度的效率与公平性，可以类比网络领域的`WDQ`, `SFQ`算法或一些软实时调度程序，如`SCAN-EDF`和`Horizon`。

**Step4 测试文件系统**

上文已经给出了多种测试程序，在设计并优化好文件系统后，我们将运行多种测试程序和测试集来测试文件系统的性能。具体测试参数将在本节最后一部分给出。

	
### FUSE使用

这里介绍一个具体的实例演示`FUSE`的使用。本实例仅在该文件系统的根目录中显示一个固定的文件，`Hello-world`文件。
源代码如下：

```c
#define FUSE_USE_VERSION 26
#include <stdio.h>
#include <fuse.h>

/* A function is implemented here to traverse the directory, which is called back when the user executes "ls" in the directory, and here only a fixed file Hello-world is returned. */

static int test_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi)
{
    printf( "tfs_readdir	 path : %s ", path);
 
    return filler(buf, "Hello-world", NULL, 0);
}

/* Display file attribute */
static int test_getattr(const char* path, struct stat *stbuf)
{
    printf("tfs_getattr	 path : %s ", path);
    if(strcmp(path, "/") == 0)
        stbuf->st_mode = 0755 | S_IFDIR;
    else
        stbuf->st_mode = 0644 | S_IFREG;
    return 0;
}

static struct fuse_operations tfs_ops = {
   .readdir = test_readdir,
   .getattr = test_getattr,
};

int main(int argc, char *argv[])
{
     int ret = 0;
     ret = fuse_main(argc, argv, &tfs_ops, NULL);
     return ret;
}
```

编译运行，将上述内容复制到一个名为`main.c`文件中，然后执行如下命令：

 > `gcc -o fuse_user main.c -D_FILE_OFFSET_BITS=64 -lfuse`

假设在`/tmp`目录下面有一个`file_on_fuse_fs`目录，如果没有可以手动创建一个。然后执行如下命令可以在该目录挂载新的文件系统。

 >`./fuse_user /tmp/file_on_fuse_fs`

此时已经有一个新的文件系统挂载在`/tmp/file_on_fuse_fs`目录下面，可以通`mount`命令查看。

 > `mount`

可以看到输出内容中包含如下内容：

```c
fuse_user on /tmp/file_on_fuse_fs type fuse.fuse_user (rw,nosuid,nodev)
```

运行ls命令查看目录

 > `ls -alh /tmp/file_on_fuse_fs`

得到如下输出

```c
总用量 0
-rw-r--r-- 0 root root 0 1月    1 1970 Hello-world
```

即我们预期的内容，一个Hello-world文件

### 测试文件系统性能

可使用`fio`工具来进行I/O性能测试。`fio`工具可以直接使用`apt install fio`安装。
`f1io`工具参数如下：

> ```
> filename=/dev/emcpowerb　支持文件系统或者裸设备，-filename=/dev/sda2或-filename=/dev/sdb
> direct=1                 测试过程绕过机器自带的buffer，使测试结果更真实
> rw=randwread             测试随机读的I/O
> rw=randwrite             测试随机写的I/O
> rw=randrw                测试随机混合写和读的I/O
> rw=read                  测试顺序读的I/O
> rw=write                 测试顺序写的I/O
> rw=rw                    测试顺序混合写和读的I/O
> bs=4k                    单次io的块文件大小为4k
> bsrange=512-2048         同上，提定数据块的大小范围
> size=5g                  本次的测试文件大小为5g，以每次4k的io进行测试
> numjobs=30               本次的测试线程为30
> runtime=1000             测试时间为1000秒，如果不写则一直将5g文件分4k每次写完为止
> ioengine=psync           io引擎使用pync方式，如果要使用libaio引擎，需要yum install libaio-devel包
> rwmixwrite=30            在混合读写的模式下，写占30%
> group_reporting          关于显示结果的，汇总每个进程的信息
> 此外
> lockmem=1g               只使用1g内存进行测试
> zero_buffers             用0初始化系统buffer
> nrfiles=8                每个进程生成文件的数量
> ```

例如在本小组成员电脑的Ubuntu环境下测试1MB文件的随机读取速度：

> `fio -filename=/dev/nvme0n1p5 -direct=1 -iodepth 1 -thread -rw=randread -ioengine=psync -bs=4k -size=1M -numjobs=5 -runtime=180 -group_reporting -name=rand_100read_4k`

可得到如下测试结果：

> ```
> rand_100read_4k: (g=0): rw=randread, bs=(R) 4096B-4096B, (W) 4096B-4096B, (T) 4096B-4096B, ioengine=psync, iodepth=1
> ...
> fio-3.16
> Starting 5 threads
> 
> rand_100read_4k: (groupid=0, jobs=5): err= 0: pid=11739: Sat Apr 17 22:04:21 2021
>   read: IOPS=41.3k, BW=161MiB/s (169MB/s)(5120KiB/31msec)
>     clat (usec): min=26, max=159, avg=53.81, stdev=15.16
>      lat (usec): min=26, max=159, avg=53.85, stdev=15.16
>     clat percentiles (usec):
>      |  1.00th=[   27],  5.00th=[   28], 10.00th=[   30], 20.00th=[   45],
>      | 30.00th=[   53], 40.00th=[   55], 50.00th=[   55], 60.00th=[   57],
>      | 70.00th=[   58], 80.00th=[   61], 90.00th=[   69], 95.00th=[   79],
>      | 99.00th=[   98], 99.50th=[  103], 99.90th=[  151], 99.95th=[  159],
>      | 99.99th=[  159]
>   lat (usec)   : 50=20.39%, 100=78.83%, 250=0.78%
>   cpu          : usr=1.37%, sys=6.16%, ctx=1323, majf=0, minf=5
>   IO depths    : 1=100.0%, 2=0.0%, 4=0.0%, 8=0.0%, 16=0.0%, 32=0.0%, >=64=0.0%
>      submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
>      complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
>      issued rwts: total=1280,0,0,0 short=0,0,0,0 dropped=0,0,0,0
>      latency   : target=0, window=0, percentile=100.00%, depth=1
> 
> Run status group 0 (all jobs):
>    READ: bw=161MiB/s (169MB/s), 161MiB/s-161MiB/s (169MB/s-169MB/s), io=5120KiB (5243kB), run=31-31msec
> 
> Disk stats (read/write):
>   nvme0n1: ios=0/0, merge=0/0, ticks=0/0, in_queue=0, util=0.00%
> ```

文件系统的性能与使用场景有关。例如:

- 设备启动时对大量小文件的读取
- 安装应用程序对大量小文件的写入
- 备份数据库对单个大文件的复制

这些操作对硬盘的要求是不同的。为实现对不同环境的模拟，我们将使用不同的参数来进行测试。测试中考虑的环境参数包括：

- 读写方式（随机/连续）
- 用于并行I/O的线程数
- 用于测试I/O量大小
- 测试时间等

为便于对比，我们将在同一块支持NVMe的硬盘上，对不同的文件系统分别进行测试。

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