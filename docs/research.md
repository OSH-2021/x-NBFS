# 调研报告

- [调研报告](#调研报告)
  - [小组成员](#小组成员)
  - [项目简介](#项目简介)
  - [项目背景](#项目背景)
    - [文件系统](#文件系统)
    - [NVMe](#nvme)
  - [立项依据](#立项依据)
    - [当前文件系统的不足](#当前文件系统的不足)
    - [修改文件系统的必要性](#修改文件系统的必要性)
  - [重要性和前瞻性分析](#重要性和前瞻性分析)
    - [针对NVMe优化的文件系统具有重要的现实意义](#针对nvme优化的文件系统具有重要的现实意义)
    - [改写文件系统对NVMe SSD读写性能等方面的提升](#改写文件系统对nvme-ssd读写性能等方面的提升)
  - [相关工作](#相关工作)
    - [其它文件系统](#其它文件系统)
    - [SPDK 存储应用开发套件](#spdk-存储应用开发套件)
  - [参考文献](#参考文献)

## 小组成员

- 陈耀祺
- 黄科鑫
- 梁峻滔
- 郑师程

## 项目简介

近几年存储行业发生了翻天覆地的变化，半导体存储登上了历史的舞台。和传统磁盘存储介质相比，半导体存储介质具有天然的优势。无论在可靠性、性能、功耗等方面都远远超越传统磁盘。目前常用的半导体存储介质是`NVMe SSD`，采用`PCIe`接口方式与主机进行交互，大大提升了性能，释放了存储介质本身的性能。

存储介质的革命一方面给存储系统性能提升带来了福音；另一方面对文件系统的设计带来了诸多挑战。原有面向磁盘设计的文件系统不能充分利用新型存储介质，面向新型存储介质需要重新设计更加合理的存储软件堆栈，发挥存储介质的性能，并且可以规避新介质带来的新问题。
本次实验项目将通过对现有文件系统进行针对性的优化，以实现性能的提升。

## 项目背景

### 文件系统

文件系统是操作系统用于明确存储设备或分区上的文件的方法和数据结构，即在存储设备上组织文件的方法。操作系统中负责管理和存储文件信息的软件机构称为文件管理系统，简称文件系统。文件系统由三部分组成：

- 文件系统的接口
- 对对象操纵和管理的软件集合
- 对象及属性

从系统角度来看，文件系统是对文件存储设备的空间进行组织和分配，负责文件存储并对存入的文件进行保护和检索的系统。具体地说，它负责为用户建立文件，存入、读出、修改、转储文件，控制文件的存取，当用户不再使用时撤销文件等。

![filesystem](/docs/files/1_filesystem.jpg)

文件系统使用文件和树形目录的抽象逻辑概念代替了硬盘和光盘等物理设备使用数据块的概念，用户使用文件系统来保存数据不必关心数据实际保存在硬盘（或者光盘）的地址为多少的数据块上，只需要记住这个文件的所属目录和文件名。在写入新数据之前，用户不必关心硬盘上的那个块地址没有被使用，硬盘上的存储空间管理（分配和释放）功能由文件系统自动完成，用户只需要记住数据被写入到了哪个文件中。

常见文件系统有：`NTFS`、`Ext4`、`XFS`、`F2FS`等。

- **主要属性**
  文件系统是一种用于向用户提供底层数据访问的机制。它将设备中的空间划分为特定大小的块（或者称为簇），一般每块512字节。数据存储在这些块中，大小被修正为占用整数个块。由文件系统软件来负责将这些块组织为文件和目录，并记录哪些块被分配给了哪个文件，以及哪些块没有被使用。不过，文件系统并不一定只在特定存储设备上出现。它是数据的组织者和提供者，至于它的底层，可以是磁盘，也可以是其它动态生成数据的设备（比如网络设备）。
  - 文件名：在文件系统中，文件名是用于定位存储位置。大多数的文件系统对文件名的长度有限制。在一些文件系统中，文件名是大小写不敏感；在另一些文件系统中则大小写敏感。大多现今的文件系统允许文件名包含非常多的`Unicode`字符集的字符。然而在大多数文件系统的界面中，会限制某些特殊字符出现在文件名中。
  - 元数据：其它文件保存信息常常伴随着文件自身保存在文件系统中。文件长度可能是分配给这个文件的区块数，也可能是这个文件实际的字节数。文件最后修改时间也许记录在文件的时间戳中。有的文件系统还保存文件的创建时间，最后访问时间及属性修改时间。
  - 安全访问：针对基本文件系统操作的安全访问可以通过访问控制列表或`capabilities`实现。研究表明访问控制列表难以保证安全，这也就是研发中的文件系统倾向于使用`capabilities`的原因。然而目前多数商业性的文件系统仍然使用访问控制列表。

- **磁盘文件系统**
  磁盘文件系统是一种设计用来利用数据存储设备来保存计算机文件的文件系统，最常用的数据存储设备是磁盘驱动器，可以直接或者间接地连接到计算机上。例如：`FAT、exFAT、NTFS、HFS、HFS+、ext2、ext3、ext4、ODS-5、btrfs、XFS、UFS、ZFS`。有些文件系统是日志文件系统或者追踪文件系统。

- **闪存文件系统**
  闪存文件系统是一种设计用来在闪存上储存文件的文件系统。随着移动设备的普及和闪存容量的增加，这类文件系统越来越流行。

  尽管磁盘文件系统也能在闪存上使用，但闪存文件系统是闪存设备的首选，理由如下：
  - 擦除区块：闪存的区块在重新写入前必须先进行擦除。擦除区块会占用相当可观的时间。因此，在设备空闲的时候擦除未使用的区块有助于提高速度，而写入数据时也可以优先使用已经擦除的区块。
  - 随机访问：由于在磁盘上寻址有很大的延迟，磁盘文件系统有针对寻址的优化，以尽量避免寻址。但闪存没有寻址延迟。
  - 写入平衡`（Wear levelling）`：闪存中经常写入的区块往往容易损坏。闪存文件系统的设计可以使数据均匀地写到整个设备。

  日志文件系统具有闪存文件系统需要的特性，这类文件系统包括`JFFS2`和`YAFFS`。也有为了避免日志频繁写入而导致闪存寿命衰减的非日志文件系统，如`exFAT`。


### NVMe

`NVM Express(NVMe)`，或称非易失性内存主机控制器接口规范`(Non-Volatile Memory Host Controller Interface Specification)`，是一个逻辑设备接口规范。它是基于设备逻辑接口的总线传输协议规范（相当于通讯协议中的应用层），用于访问通过`PCI Express(PCIe)`总线附加的非易失性存储器介质（例如采用闪存的固态硬盘驱动器），虽然理论上不一定要求`PCIe`总线协议。`NVMe`是一种协议，是一组允许`SSD`使用`PCIe`总线的软硬件标准；而`PCIe`是实际的物理连接通道。

此规范主要是为基于闪存的存储设备提供一个低延时、内部并发化的原生界面规范，也为现代CPU、电脑平台及相关应用提供原生存储并发化的支持，令主机硬件和软件可以充分利用固态存储设备的并行化存储能力。相比此前机械硬盘驱动器`(HDD)`时代的`AHCI`（`SATA`下的协议），`NVMe`降低了I/O操作等待时间、提升同一时间内的操作数、更大容量的操作队列等。

**NVMe工作原理**

`NVMe`使用的增强型`NVMHCI`基于成对的提交和完成队列机制：指令由主机软件放置在提交队列中。完成内容放入关联的控制器完成队列。多个提交队列可以利用相同的完成队列。提交和完成队列分配在主机内存中。存在用于设备管理目的的`Admin`提交和关联的完成队列和控制-例如创建和删除I/O提交和完成队列，中止命令，等等。只有属于管理命令集的命令才可以发布到管理提交中队列。

`NVMe`要求I/O命令集与I/O队列一起使用。该规范定义了一个I/O命令集，被命名为`NVM Command Set`。

![NVMe_controllor](/docs/files/1_NVMe_controllor.png)

主机软件会创建队列，最多不超过控制器支持的队列。通常数量创建的命令队列基于系统配置和预期的工作量。例如，在基于四核处理器的系统上，每个核可能有一个队列对`（queue pair）`，以避免锁定并确保数据结构是在适当的处理器内核的缓存中创建的。下图提供了图形队列机制的表示形式，显示了“提交队列”与“完成队列”之间的1：1映射完成队列。

**NVMe相较于AHCI的优势**

- 当数据从存储传输到服务器主机时，会进入I/O队列。传统的`AHCI`协议只能支持一个队列，一次只能接收32条数据。而`NVMe`存储支持最多64000个队列，每个队列有64000个条目。

![AHCI_NVMe](/docs/files/1_AHCI_NVMe.jpg)

- `NVMe`使用原生`PCIe`通道，免去了`SATA`与`SAS`接口的主机适配器与CPU通信所带来的延时。`NVMe`标准的延时只有`AHCI`的一半不到：`NVMe`精简了调用方式，执行命令时不需要读取寄存器；而`AHCI`每条命令则需要读取4次寄存器，一共会消耗8000次CPU循环，从而造成大概2.5微秒的延迟。

![NVMe_delay](/docs/files/1_NVMe_delay.jpg)

- `NVMe`支持同时从多核处理器接受命令和优先处理请求，这在企业级的重负载时优势明显。

![NVMe_multi](/docs/files/1_NVMe_multi.jpg)

- `NVMe`加入了自动功耗状态切换和动态能耗管理功能。设备从`Power State 0`闲置50ms后可以切换到`Power State 1`；继续闲置的话，在500ms后又会进入功耗更低的`Power State 2`，切换时会有短暂延迟。`SSD`在闲置时可以非常快速的控制在极低的水平，在功耗管理上`NVMe`标准的`SSD`会比`AHCI SSD`拥有较大优势。

![NVMe_power](/docs/files/1_NVMe_power.png)

## 立项依据

### 当前文件系统的不足

与为机械硬盘设计的`AHCI`协议不同,`NVMe`是专为固态硬盘构建的接口协议, 不仅可以发挥固态硬盘在快速读写方面的优势, 还可以充分利用多核CPU和大容量内存。然而，目前的文件系统对于`NVMe`的运用仍然存在以下不足：

- 传统内核空间的I/O栈开销成为性能瓶颈，因为它一开始是为`HDD`设计的。为了解决这个问题，很多研究者尝试通过在用户空间轮询的机制，以及减少不必要的操作等方法，来减少内核开销。英特尔提供了用户空间的文件系统，基于`SPDK`的`BlockFS`，但是这限制了`SSD`的一些功能，例如就地更新，以及多应用对`SSD`的共享访问。

- 多队列技术对于提升`NVMe`的性能十分重要。`NVMe`根据任务，计划的优先级和核心的数量等来分配不同的队列，但是`SSD`的读写性能不平衡，读取的速度比写入的速度要快了10~40倍。然而现有的文件系统并不能根据上述特征来区别写入队列和读取队列，这样就导致了读取性能收到了写入性能的牵制。

- `SSD`的读写是按页对齐的，如4K，但是擦除是按块对齐的，如2M。但是现有的文件系统并没有考虑到`SSD`这样的特性，这就导致了写入量增大，还增加了无意义的数据迁移。这也严重影响了读写性能。

传统的内核态文件系统过于厚重, 内核存储栈存在大量的拷贝、上下文切换以及中断等软件开销, 导致读写延迟较大, 无法充分发挥`NVMe SSD`的硬件性能。人们转向研究用户态文件系统, 试图通过在用户态使用轮询机制来减少开销, 并消除不必要的上下文切换。因此, 轻薄而高效的用户态文件系统逐渐成为`NVMe SSD`的重要选择, 亦成为学术界与工业界共同的研究热点。然而, 现有的面向`NVMe SSD`的用户态文件系统存在如下缺陷：

- 文件系统布局无法兼顾空间利用率和读写性能。现有的用户态文件系统往往将轻而薄作为设计目标, 因此在空间分配上的策略较为单一。例如, 对不同尺寸或流式写入的文件采用固定的分配方法, 导致空间利用率或性能低下。此外, 应用和文件系统各自实现日志, 也导致大量不必要的I/O。

- 未充分发挥`NVMe SSD`的多队列高性能特性。多队列技术是`NVMe`的一个重要的提高性能的方法。借助于多队列技术,`NVMe`实现按照任务、调度优先级和CPU核分配不同队列。但是,`SSD`具有读写不对称性, 大小文件的延迟同样存在差别, 现有的文件系统没有根据I/O请求的特性区分队列, 性能存在一定的提升空间。

- 不支持多个应用程序对`SSD`的共享访问。由于文件系统处于用户态, 未经过操作系统的块设备驱动, 因此, 应用程序只能绑定`SSD`进行读写访问。显然, 这与传统的内核态文件系统的使用有较大的差异, 无法实现多个应用程序对`SSD`的共享访问, 而这种访问在一些场景(如多个客户共享读取一个热点视频)中十分重要。

### 修改文件系统的必要性

通过改善物理接口以及增加命令数量和队列深度，`NVMe`可使存储基础架构充分利用基于闪存的存储。但同时`NVMe`也带来挑战：`NVMe`具有极高的延迟效率，因而可能暴露存储基础架构其他组件的弱点。而基础架构中任何薄弱环节都会增加延迟性并降低`NVMe`的价值。 在存储基础架构中，最容易出问题的是文件系统。现在我们应该重新考虑文件系统结构，特别是必须重新调整文件系统与`NVMe`存储的交互方式，以避免其成为主要瓶颈。

为何文件系统很重要？ 通常情况下，支持AI和高速工作负载的文件系统是横向扩展。横向扩展文件系统通常由多个存储服务器或节点组成，文件系统会聚合每个节点的内部存储，并将其作为单个存储池，供用户和应用程序访问。传统文件系统也可以是横向扩展，但它们是串行的，这意味着所有I/O都会通过主节点，AI和高速工作负载很容易被淹没，从而造成瓶颈。这些工作负载主要采用并行文件系统结构，使群集中的任何节点都能为用户或应用程序提供I/O服务，这也可提高网络效率。

**1、 提高存储系统的性能**
大多数`NVMe`存储系统都是针对块存储而设计。因此，它们可避免文件系统结构的性能开销。但是，在大多数情况下，文件系统会被添加到块存储系统中，以供AI和高速工作负载使用。大多数现代应用程序都依靠文件系统，特别是AI、机器学习和大数据分析处理等。 对于经过精心设计的基于​​块的`NVMe`存储系统，当添加文件系统时，仍然可能比基于块的`SAS`存储系统更快，不过，原始块存储和文件系统控制存储之间的性能下降会非常显着。因此，企业需要针对`NVMe`经过优化的文件系统。

**2、 缓解系统性能瓶颈**
传统存储软件栈采用堆叠式的方式，并且存在较多层级。每个层级封装成一个模块，构成了所谓的“模块化设计”。在面向磁盘的存储系统中，这种设计方式非常好，带来的价值是存储软件栈设计与实现的灵活性。用户需要增加一个功能的时候，可以在软件栈中堆叠一个模块，非常的高效，软件功能与质量可以得到很好的控制。但是，在面向高性能介质的存储系统中，存储盘已经不再是性能瓶颈点的主因，存储软件栈本身变成了严重的性能瓶颈点。而传统的软件栈在设计与实现的时候根本没有考虑自身的瓶颈问题、软件的效率问题，根本没有考虑如何高效使用CPU的问题。从而导致传统软件栈使得处理器的运行效率极低，成了性能瓶颈的主要因素。

![burden](/docs/files/burden.jpg)

尤其最近几年处理器往多核化方向发展，基于“存储是I/O密集型应用”为指导思想的存储软件栈，很少采用多核并发处理的设计思想，因此无法发挥多核化带来的价值。高性能存储介质恰恰需要多处理器的支持，在这一点上，传统软件栈的设计存在天然缺陷，其主要原因还是没有考虑到I/O性能瓶颈点的转移，所以，存储软件栈在背负较重的情况下，自身成为瓶颈点，性能低下在所难免。因此，修改文件系统可以避免这一瓶颈，从而提升系统的整体运行效率。

**3、 提升某些高负载应用的运行效率**
与其他任何工作负载类型相比，AI和高速度用例可充分利用`NVMe`。这些工作负载面临的挑战是应用程序通常通过文件系统访问存储。传统文件系统不会针对基于`NVMe`的驱动器优化其I/O。更快的节点硬件和`NVMe`驱动器可提供更高的性能，但文件系统结构不允许硬件充分发挥其潜力，因此，需要修改文件系统以避免这一瓶颈。

**4、 传统文件系统已不再适用于NVMe SSD**
包括文件系统在内的传统存储软件栈的设计是面向磁盘介质的，磁盘介质与高性能`NVMe SSD`相比存在截然不同的特性。对于`NVMe SSD`而言不存在随机访问性能抖动的问题；但是会存在业务I/O影响写放大的问题，以及个别SSD存在写后读性能极差等问题。面向磁盘设计的一些机制对`SSD`而言没有价值，并且在有些应用场景下会极大的影响整体性能。例如似乎可以增加随机访问性能的`Page cache`，在`NVMe SSD`上会对业务性能造成影响。在随机访问的情况下，`Page cache`的命中率比较低，并且会不断的在`SSD`与内存之间进行`page`页的换入换出，这种频繁的换入换出操作会增加大量的无用操作，在软件设计存在竞争锁的情况下，文件系统的性能表现会大打折扣。

![compare](/docs/files/compare.jpg)

上图对比了`4KB`与`8KB`在随机访问情况下挂载`Ext4`文件系统与裸盘的性能差异。从图中可以看出，在`4KB`与`8KB`随机读情况下，文件系统的性能要远远低于裸盘性能。此时我们发现文件系统在忙于`Page Cache`的换入换出操作。

因此，有必要对文件系统做出修改，以实现`NVMe SSD`的利用最大化。

## 重要性和前瞻性分析

### 针对NVMe优化的文件系统具有重要的现实意义

**1、 NVMe SSD的普及引发对文件系统的有关需求**
`NVMe SSD`相较于传统的`HDD`或`SATA SSD`具有更高的读写速度，更低的延迟，随着`NVMe SSD`成本的下降，`NVMe SSD`的市场占有率不断提高。2020年全球`HDD`机械硬盘的出货量为3.5亿个，`SSD`固态硬盘出货量未3.2亿个。预计2021年，`SSD`硬盘全球出货量将反超`HDD`，达到3.6亿个。`SSD`固态硬盘保持高速增长，2018年全球出货量突破2亿个，增长近四成。与此相比的是`HDD`出货量连续5年的下跌。

![NVMe_future](/docs/files/1_NVMe_future.png)

但是当前文件系统普遍没有针对`NVMe`做出优化,传统软件栈不仅没有优化高性能存储介质，反而带来了性能以及寿命等方面的影响。所以，`NVMe SSD`在数据中心等应用中大规模使用时，文件系统需要做出深层次变革。

**2、 解决AI、数据库等某些应用的性能瓶颈**
而在未来，由于AI、数据库等数据密集型应用的爆发，`NVMe SSD`的需求将会继续增加。然而，与之匹配的文件系统却迟迟没有大规模应用，当前的文件系统普遍存在无法兼顾空间利用率和读写性能、读写不匹配问题、未充分发挥`NVMe SSD`的多队列高性能特性等等问题，为了满足日益增长的需要,需要将文件系统这最后一块“短板”补上。

### 改写文件系统对NVMe SSD读写性能等方面的提升

通过改善物理接口以及增加命令数量和队列深度，`NVMe`可使存储基础架构充分利用基于闪存的存储。但同时`NVMe`也带来挑战：`SSD`与`NVMe`高效的配合，使存储器的硬件时延显著降低, 导致存储软件的时延成为存储系统整体时延的主要部分，而架构中任何薄弱环节都会增加延迟性并降低`NVMe`的价值。 在存储基础架构中，最容易出问题的是文件系统，需要重新调整文件系统与`NVMe`存储的交互方式，以避免其成为主要瓶颈。

通过对已有的文件系统的缺陷进行有针对性的改写，如修改I/O队列调度方式以克服读写性能不平衡带来的问题、考虑`SSD`读写单元与擦除单元大小不一致的特性以减少无意义的写入量和数据迁移、采用更灵活的空间分配策略、修改应用访问机制等，使得`NVMe SSD`的多队列高性能特性得到充分发挥，提高读写性能和空间利用率，实现允许多个应用程序对`SSD`的共享访问等。

下表展示了针对`NVMe`优化的基于用户态的文件系统`UHSFS`与传统的`F2FS、XFS、Ext4`的性能对比：

![UHSFS_test](/docs/files/UHSFS_test.jpg)

与内核态文件系统性能较优的`F2FS`相比,`UHSFS`的性能提升`2.2%~25.5%`。对于`64KB`顺序读写, 大部分文件系统都能达到带宽上限,`UHSFS`的吞吐量略高于内核态文件系统, 这是由于`UHSFS`在用户态实现, 中断和代码开销更小。在`4KB`随机读写场景中,`UHSFS`的`IOPS`明显高于其他文件系统,`4KB`随机读`IOPS`领先排名第二的`F2FS`约`16.6%`,`4KB`随机写则领先约 `25.5%`。`UHSFS`的性能优势主要在于,`UHSFS`将所有元数据缓存在内存中, 查找文件的开销大幅降低;`UHSFS`基于用户态`NVMe`驱动, 相对于内核态文件系统, 减少了上下文切换和中断造成的开销。

由此可见，修改文件系统，就可以进一步释放`NVMe SSD`的性能，进而提高存储系统的读写能力。

## 相关工作

### 其它文件系统

- **XFS**
  `XFS`是由`Silicon Graphics Inc（SGI）`于1993年创建的高性能64位日记文件系统。从`SGI 5.3`版本开始，它是`SGI IRIX`操作系统中的默认文件系统。`XFS`于2001年移植到Linux内核。截至2014年6月，大多数Linux发行版均支持`XFS`，其中一些发行版将其用作默认文件系统。

  由于`XFS`的设计基于分配组（一种使用XFS的物理卷的细分类型，也简称为AG），因此`XFS`在执行并行输入/输出I/O操作方面表现出色。因此，当跨越多个物理存储设备时，`XFS`可以实现I/O线程，文件系统带宽，文件大小以及文件系统本身的极佳可伸缩性。`XFS`通过采用元数据日记功能和支持写障碍来确保数据的一致性。通过扩展使用存储在`B+`树中的数据结构执行空间分配 ，提高了文件系统的整体性能，尤其是在处理大文件时。延迟分配有助于防止文件系统碎片化；还支持在线碎片整理。`XFS`独有的功能是以预定速率预先分配I/O带宽。这适用于许多实时应用程序。但是，此功能仅在`IRIX`上受支持，并且仅在专用硬件上受支持。

  由于`XFS`在并行I/O方面的优势，对于基于`NVMe`的`SSD`，它在处理大量文件的随机读写时相比其他文件系统更快。

- **Ext4**
  第四代扩展文件系统`（Fourth extended filesystem，缩写为ext4）`是Linux系统下的日志文件系统，是`ext3`文件系统的后继版本。

  `ext4`对于大型文件系统做了一定的优化（支持最大`1 exbibyte（EiB）`大小的卷和具有标准`4 KiB`块大小的最大`16 tebibytes（TiB）`的单个文件），并引入了`Extents`文件存储方式以加速大文件存储。同时`ext4`对于磁盘碎片的整理方面一如了各种避免磁盘碎片和整理碎片的技术（例如延时分配）。在16年发布到Linux内核4.9的补丁中，通过文件锁的方式，添加了`ext4`并行`DIO`读取的支持。

- **F2FS**
  `F2FS（Flash-Friendly File System）`是一闪存文件系统，主要由金载极`（韩语：김재극）`在三星集团研发，适合Linux内核使用。

  此文件系统起初是为了`NAND`闪存的存储设备设计（诸如固态硬盘、eMMC和SD卡），这些设备广泛存在于自移动设备至服务器领域。并且三星应用了日志结构文件系统的概念，使它更适合用于存储设备。

  `F2FS`针对闪存做了多种优化，例如使用检查点方案来维护文件系统的完整性，并在后台闲置时进行清理碎片。

### SPDK 存储应用开发套件

`SPDK（Storage Performance Development Kit）`提供了一组用于编写高性能、可伸缩、用户态存储应用程序的工具和库。

`SPDK`的基础是用户态、轮询、异步、无锁`NVMe`驱动。这提供了从用户空间应用程序直接访问`SSD`的零拷贝、高度并行的访问。驱动程序被编写为带有一个公共头文件的C语言库。
`SPDK`进一步提供了一个完整的块堆栈，作为一个用户空间库，它执行许多与操作系统中的块堆栈相同的操作。这包括统一不同存储设备之间的接口、通过队列来处理内存不足或I/O挂起等情况以及逻辑卷管理。

![SPDK](/docs/files/1_SPDK.png)

最后，`SPDK`提供`NVMe-oF`,`iSCSI`,和`vhost`。在这些组件之上构建的服务器,能够通过网络或其他进程为磁盘提供服务。`NVMe`和`iSCSI`的标准Linux内核启动器与这些target交互, 以及与`QEMU`和虚拟主机进行交互。与其他实现相比，这些服务器的CPU效率可以提高一个数量级。这些target可以用作实现高性能存储目标的范例，也可以用作生产部署的基础。

## 参考文献

- File system. (2021, March 29). Retrieved April 14, 2021, from https://en.wikipedia.org/wiki/File_system/

- NVM express. (2021, March 26). Retrieved April 14, 2021, from https://en.wikipedia.org/wiki/NVM_Express

- White papers. (2016, August 05). Retrieved April 14, 2021, from https://nvmexpress.org/white-papers/

- Y. T. Jin, S. Ahn and S. Lee, "Performance Analysis of NVMe SSD-Based All-flash Array Systems," 2018 IEEE International Symposium on Performance Analysis of Systems and Software (ISPASS), Belfast, UK, pp. 12-21, 2018.

- A. Tavakkol et al., "FLIN: Enabling Fairness and Enhancing Performance in Modern NVMe Solid State Drives," 2018 ACM/IEEE 45th Annual International Symposium on Computer Architecture (ISCA), Los Angeles, CA, USA, pp. 397-410, 2018.

- Y. Tu, Y. Han, Z. Chen, Z. Chen and B. Chen, "URFS: A User-space Raw File System based on NVMe SSD," 2020 IEEE 26th International Conference on Parallel and Distributed Systems (ICPADS), Hong Kong, pp. 494-501, 2020.

- 看文件系统结构如何降低nvme性能. (2019, January 02). Retrieved April 14, 2021, from https://searchstorage.techtarget.com.cn/6-27904/

- XFS. (2021, March 04). Retrieved April 14, 2021, from https://en.wikipedia.org/wiki/XFS

- Ext4. (2020, December 18). Retrieved April 14, 2021, from https://en.wikipedia.org/wiki/Ext4

- F2FS. (2021, March 02). Retrieved April 14, 2021, from https://en.wikipedia.org/wiki/F2FS

- Written by Michael Larabel in Storage on 7 January 2019. Page 1 of 4. 61 Comments. (2019, January 7). Linux 5.0 File-System Benchmarks: Btrfs vs. ext4 VS. F2FS Vs. xfs. Retrieved April 14, 2021, from https://www.phoronix.com/scan.php?page=article&item=linux-50-filesystems&num=1

- Storage performance development kit. Retrieved April 14, 2021, from https://spdk.io/