# 可行性报告

- [可行性报告](#可行性报告)
    - [项目介绍](#项目介绍)
    - [理论依据](#理论依据)
      - [文件系统](#文件系统)
      - [NVMe](#nvme)
      - [FUSE](#fuse)
    - [技术依据](#技术依据)
      - [SPDK](#spdk)
      - [NVFUSE](#nvfuse)
      - [测试工具](#测试工具)
    - [技术路线](#技术路线)
      - [文件系统设计](#文件系统设计)
      - [FUSE使用](#fuse使用)
      - [测试](#测试)
    - [参考文献](#参考文献)

### 项目介绍

近几年存储行业发生了翻天覆地的变化，半导体存储登上了历史的舞台。和传统磁盘存储介质相比，半导体存储介质具有天然的优势。无论在可靠性、性能、功耗等方面都远远超越传统磁盘。目前常用的半导体存储介质是`NVMe SSD`，采用PCIe接口方式与主机进行交互，大大提升了性能，释放了存储介质本身的性能。

存储介质的革命一方面给存储系统性能提升带来了福音；另一方面对文件系统的设计带来了诸多挑战。原有面向磁盘设计的文件系统不能充分利用新型存储介质，传统的内核态文件系统过于厚重, 内核存储栈存在大量的拷贝、上下文切换以及中断等软件开销, 导致读写延迟较大, 无法充分发挥`NVMe SSD`的硬件性能。 

本次实验项目将利用`SPDK`以及用户态文件系统`FUSE`设计基于用户态的文件系统，以提升文件系统性能与效率。

### 理论依据

#### 文件系统

文件系统是操作系统用于明确存储设备或分区上的文件的方法和数据结构，即在存储设备上组织文件的方法。操作系统中负责管理和存储文件信息的软件机构称为文件管理系统，简称文件系统。文件系统由三部分组成：

 - 文件系统的接口
 - 对对象操纵和管理的软件集合
 - 对象及属性

从系统角度来看，文件系统是对文件存储设备的空间进行组织和分配，负责文件存储并对存入的文件进行保护和检索的系统。具体地说，它负责为用户建立文件，存入、读出、修改、转储文件，控制文件的存取，当用户不再使用时撤销文件等。

文件系统使用文件和树形目录的抽象逻辑概念代替了硬盘和光盘等物理设备使用数据块的概念，用户使用文件系统来保存数据不必关心数据实际保存在硬盘（或者光盘）的地址为多少的数据块上，只需要记住这个文件的所属目录和文件名。在写入新数据之前，用户不必关心硬盘上的那个块地址没有被使用，硬盘上的存储空间管理（分配和释放）功能由文件系统自动完成，用户只需要记住数据被写入到了哪个文件中。

**主要属性**
文件系统是一种用于向用户提供底层数据访问的机制。它将设备中的空间划分为特定大小的块（或者称为簇），一般每块512字节。数据存储在这些块中，大小被修正为占用整数个块。由文件系统软件来负责将这些块组织为文件和目录，并记录哪些块被分配给了哪个文件，以及哪些块没有被使用。不过，文件系统并不一定只在特定存储设备上出现。它是数据的组织者和提供者，至于它的底层，可以是磁盘，也可以是其它动态生成数据的设备（比如网络设备）。
- *文件名*：在文件系统中，文件名是用于定位存储位置。大多数的文件系统对文件名的长度有限制。在一些文件系统中，文件名是大小写不敏感；在另一些文件系统中则大小写敏感。大多现今的文件系统允许文件名包含非常多的`Unicode`字符集的字符。然而在大多数文件系统的界面中，会限制某些特殊字符出现在文件名中。
- *元数据*：其它文件保存信息常常伴随着文件自身保存在文件系统中。文件长度可能是分配给这个文件的区块数，也可能是这个文件实际的字节数。文件最后修改时间也许记录在文件的时间戳中。有的文件系统还保存文件的创建时间，最后访问时间及属性修改时间。
- *安全访问*：针对基本文件系统操作的安全访问可以通过访问控制列表或`capabilities`实现。研究表明访问控制列表难以保证安全，这也就是研发中的文件系统倾向于使用`capabilities`的原因。然而目前多数商业性的文件系统仍然使用访问控制列表。

#### NVMe

`NVM Express(NVMe)`，或称非易失性内存主机控制器接口规范`(Non-Volatile Memory Host Controller Interface Specification)`，是一个逻辑设备接口规范。它是基于设备逻辑接口的总线传输协议规范（相当于通讯协议中的应用层），用于访问通过`PCI Express(PCIe)`总线附加的非易失性存储器介质（例如采用闪存的固态硬盘驱动器），虽然理论上不一定要求`PCIe`总线协议。`NVMe`是一种协议，是一组允许`SSD`使用`PCIe`总线的软硬件标准；而`PCIe`是实际的物理连接通道。

此规范主要是为基于闪存的存储设备提供一个低延时、内部并发化的原生界面规范，也为现代CPU、电脑平台及相关应用提供原生存储并发化的支持，令主机硬件和软件可以充分利用固态存储设备的并行化存储能力。相比此前机械硬盘驱动器`(HDD)`时代的`AHCI`（`SATA`下的协议），`NVMe`降低了I/O操作等待时间、提升同一时间内的操作数、更大容量的操作队列等。

**`NVMe`工作原理**
`NVMe`使用的增强型`NVMHCI`基于成对的提交和完成队列机制：指令由主机软件放置在提交队列中。完成内容放入关联的控制器完成队列。多个提交队列可以利用相同的完成队列。提交和完成队列分配在主机内存中。存在用于设备管理目的的`Admin`提交和关联的完成队列和控制-例如创建和删除I/O提交和完成队列，中止命令，等等。只有属于管理命令集的命令才可以发布到管理提交中队列。

`NVMe`要求I/O命令集与I/O队列一起使用。该规范定义了一个I/O命令集，被命名为`NVM Command Set`。

主机软件会创建队列，最多不超过控制器支持的队列。通常数量创建的命令队列基于系统配置和预期的工作量。例如，在基于四核处理器的系统上，每个核可能有一个队列对`（queue pair）`，以避免锁定并确保数据结构是在适当的处理器内核的缓存中创建的。下图提供了图形队列机制的表示形式，显示了“提交队列”与“完成队列”之间的1：1映射完成队列。

#### FUSE

用户空间文件系统（`Filesystem in Userspace`，简称`FUSE`）是一个面向类`Unix`计算机操作系统的软件接口，它使无特权的用户能够无需编辑内核代码而创建自己的文件系统。

**为什么需要`FUSE`？**
现存文件系统难以满足用户的特别需求。传统上操作系统在内核层面上对文件系统提供支持，但通常内核态的代码难以调试，内核下定制开发文件系统难度较高，生产率较低，在用户空间可定制实现文件系统是有需求的。并且，用户空间下调试工具丰富，出问题不会导致系统崩溃，开发难度相对较低，开发周期较短。

**工作原理**
`FUSE`包含两个组件：内核模块（在常规系统内核存储库中维护）和用户空间库（`libfuse`）。`FUSE`文件系统通常实现为与`libfuse`链接的独立应用程序。`libfuse`提供了以下功能：挂载文件系统，卸载文件系统，从内核读取请求以及将响应发送回。

在`Linux`中，对文件的访问都是统一通过VFS层提供的内核接口进行的（比如`open/read`），因此当一个进程（称为`"user"`）访问用户态文件系统时，依然需要途径`VFS`。当`VFS`接到`user`进程对文件的访问请求，并且判断出该文件是属于某个用户态文件系统，就会将这个请求转交给一个名为`"fuse"`的内核模块；而后，`"fuse"`将该请求传给用户态进程（如下图）。

![fuse](/docs/files/2_fuse.png)

`FUSE`作为用户态文件系统使用、调试方便，但由于其额外的内核态/用户态切换会影响性能。**因此在这里我们需要在使用`FUSE`的同时利用`SPDK`的特性，即可以在用户态直接向`NVMe SSD`提交I/O请求，从而使得文件系统在简便的同时兼顾高效。**

### 技术依据

#### SPDK

`The Storage Performance Development Kit`（`SPDK`）提供了一套工具和库，用于编写高性能、可扩展的用户模式存储应用程序。它通过使用一些关键技术来实现高性能：

- 将所有必要的驱动程序移入用户空间。这样可以避免系统调用，并实现应用程序的零拷贝访问。
- 对硬件进行轮询，而不是使用中断，这降低了总延迟和延迟波动。
- 避免I/O路径中的所有锁，而是依靠消息传递。

`SPDK`的底层是一个用户空间、`polled`模式、异步、无锁的`NVMe`驱动。它提供了从用户空间应用的零拷贝、高度并行的直接访问。该驱动以C语言库的形式编写，且只有一个公共头。

`SPDK`还提供了一个完整的区块堆栈，作为用户空间库，执行大量与操作系统中区块堆栈相同的操作。这包括统一不同存储设备之间的接口，利用队列处理内存不足或I/O挂起等情况，以及逻辑分区的管理。

最后，`SPDK`提供了建立在这些组件之上的`NVMe-oF`、`iSCSI`和`vhost`服务器，它们能够通过网络或向其他进程提供磁盘服务。`NVMe-oF`和`iSCSI`的标准`Linux`内核启动器可以与这些目标互操作，`QEMU`也可以与`vhost`互操作。这些服务器的CPU的执行效率可以比其他实现高出一个数量级。这些目标可以作为如何实现高性能存储目标的例子，或者作为生产部署的基础。

#### NVFUSE
`NVFUSE`是一个结合了`SPDK`库的可嵌入的文件系统。`SPDK`库是英特尔最新推出的用户空间的`NVMe`驱动。使用了这种文件系统的应用可以直接提交I/O请求给`NVMe`的`SSD`。它提高了硬盘的性能和可靠性，还提供了类`POSIX`的接口（如`nvfuse_open`, `nvfuse_read`, `nvfue_write`, `nvfuseclose`这样的函数）。注意这个文件系统并不是利用众所周知的`FUSE`(`File System in Userspace`)库来实现`POSIX`的兼容性。

主要特性如下：
- 在用户空间中运行，不需要与内核驱动进行任何交互，因此不需要中断、上下文切换和内存复制。
- 作为库的可嵌入的文件系统，引进了类似`POSIX API`的新接口。
- 简单的文件系统布局，与`EXT`系列文件系统（如`EXT3`）相同。
- 通过`NVMe`规范所描述的`NVMe`元数据功能，改善了日志记录机制，提高了文件系统一致性和耐久性。

#### 测试工具

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

### 技术路线

#### 文件系统设计

我们将要设计一个基于`FUSE`和`SPDK`的用户态文件系统，其主要应用于优化文件系统在`NVMe SSD`上的文件速度:
1. 通过结合`libfuse`便于设计用户态文件系统和`spdk`支持用户态直接提交I/O请求的特点，初步构建一个提供`POSIX`类似的`API`和类似`EXT4`文件结构架构的文件系统

2. 结合`NVMe`的特性，设计算法优化文件系统的应用（以下为初步的设计思路）：
	 - 传统文件系统不支持共享访问`SSD`，因此可以通过管理维护共享内存，使得多个应用程序可以访问多个`NVMe SSD`，从而加速访问`SSD`的性能；
	 - 在`Linux`内核中当前的`NVMe`驱动程序会为主机系统中的每个内核创建一个提交队列和一个完成队列，在我们的系统中，可以设计算法灵活分配IO，并根据工作负载类别动态评估分离到不同的队列中；
	 - 在保证性能的同时兼顾调度的效率与公平性，可以类比网络领域的`WDQ`,`SFQ`算法或一些软实时调度程序，如`SCAN-EDF`和`Horizon`。
	
#### FUSE使用

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

#### 测试

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

### 参考文献

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

