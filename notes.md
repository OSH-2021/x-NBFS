## Notes

#### 如何编译一个使用SPDK的程序
根据`spdk_root/Makefile`和`spdk_root/mk/spdk.app.mk`中的代码，在`spdk_root`中输入`make`命令后，`spdk.app.mk`会被调用，其中有这样一段：

   ```makefile
   ifneq (,$(findstring $(SPDK_ROOT_DIR)/app,$(CURDIR)))
   	APP := $(APP_NAME:%=$(SPDK_ROOT_DIR)/build/bin/%)
   else
   ifneq (,$(findstring $(SPDK_ROOT_DIR)/examples,$(CURDIR)))
   	APP := $(APP_NAME:%=$(SPDK_ROOT_DIR)/build/examples/%)
   endif
   ```

   可以猜测，只需要将代码文件放到`spdk_root`下的`app`目录下，再在`spdk_root`下执行`make`，就会自动编译。（当然首先要在各级目录加上`Makefile`）

   **测试**：

   首先在`app`文件夹里创建一个`my_test`，并在`app`目录下的`Makefile`中添加新增的目录：

   ```makefile
   DIRS-y += trace
   ...
   DIRS-y += spdk_lspci
   DIRS-y += my_test # 新增的文件夹
   ```

   然后再`app/my_test`文件夹中创建`my_test.c`和`Makefile`文件，c文件为代码文件，`Makefile`中内容为：

   ```makefile
   SPDK_ROOT_DIR := $(abspath $(CURDIR)/../..)
   include $(SPDK_ROOT_DIR)/mk/spdk.common.mk
   include $(SPDK_ROOT_DIR)/mk/spdk.modules.mk
   
   APP = my_test
   
   C_SRCS := my_test.c
   
   #所用的lib
   #SPDK_LIB_LIST = $(ALL_MODULES_LIST) event event_bdev
   
   include $(SPDK_ROOT_DIR)/mk/spdk.app.mk
   ```

   最后在`spdk_root`下输入`make`，输入`sudo build/bin/my_test`就可以成功运行。

#### `hello_bdev`的运行

编译完成后，`hello_bdev`位于`build/examples/`，其运行需要传递配置文件，将配置文件和程序放在同一目录下，运行

> `sudo ./hello_bdev -c bdev.json`

可以得到输出：

> ```
> [2021-07-05 14:15:32.388291] Starting SPDK v21.07-pre git sha1 eb5a11139 / DPDK 21.02.0 initialization...
> [2021-07-05 14:15:32.390057] [ DPDK EAL parameters: [2021-07-05 14:15:32.390700] hello_bdev [2021-07-05 14:15:32.391869] --no-shconf [2021-07-05 14:15:32.392551] -c 0x1 [2021-07-05 14:15:32.393104] --log-level=lib.eal:6 [2021-07-05 14:15:32.393732] --log-level=lib.cryptodev:5 [2021-07-05 14:15:32.394451] --log-level=user1:6 [2021-07-05 14:15:32.395156] --iova-mode=pa [2021-07-05 14:15:32.395694] --base-virtaddr=0x200000000000 [2021-07-05 14:15:32.396264] --match-allocations [2021-07-05 14:15:32.396768] --file-prefix=spdk_pid1115 [2021-07-05 14:15:32.397267] ]
> EAL: No legacy callbacks, legacy socket not created
> [2021-07-05 14:15:32.415167] app.c: 535:spdk_app_start: *NOTICE*: Total cores available: 1
> [2021-07-05 14:15:32.646398] reactor.c: 929:reactor_run: *NOTICE*: Reactor started on core 0
> [2021-07-05 14:15:32.646780] accel_engine.c: 853:spdk_accel_engine_initialize: *NOTICE*: Accel engine initialized to use software engine.
> [2021-07-05 14:15:32.751727] rpc.c: 186:spdk_rpc_listen: *ERROR*: RPC Unix domain socket path /var/tmp/spdk.sock in use. Specify another.
> [2021-07-05 14:15:32.758119] rpc.c:  71:spdk_rpc_initialize: *ERROR*: Unable to start RPC service at /var/tmp/spdk.sock
> [2021-07-05 14:15:32.758171] hello_bdev.c: 250:hello_start: *NOTICE*: Successfully started the application
> [2021-07-05 14:15:32.758180] hello_bdev.c: 259:hello_start: *NOTICE*: Opening the bdev Malloc0
> [2021-07-05 14:15:32.758188] hello_bdev.c: 272:hello_start: *NOTICE*: Opening io channel
> [2021-07-05 14:15:32.758226] hello_bdev.c: 167:hello_write: *NOTICE*: Writing to the bdev
> [2021-07-05 14:15:32.758238] hello_bdev.c: 144:write_complete: *NOTICE*: bdev io write completed successfully
> [2021-07-05 14:15:32.758244] hello_bdev.c: 111:hello_read: *NOTICE*: Reading io
> [2021-07-05 14:15:32.758251] hello_bdev.c:  91:read_complete: *NOTICE*: Read string from bdev : Hello World!
> 
> [2021-07-05 14:15:32.758258] hello_bdev.c: 100:read_complete: *NOTICE*: Stopping app
> EAL: device event monitor already stopped
> ```

#### [`hello_bdev`代码](https://github.com/spdk/spdk/blob/master/examples/bdev/hello_world/hello_bdev.c)分析

- `struct spdk_app_opts`: 是事件框架的初始化选项，通过调用[`spdk_app_opts_init(opts,sizeof(opts)) `]((https://spdk.io/doc/event_8h.html#a65555def11b52c0b7bea010ab24e25d8))可以生成初始值，包含了基本的名称、配置等信息
- `int spdk_app_start(struct spdk_app_opts *opts_user, spdk_msg_fn start_fn, void *ctx)`: 根据已经初始化的`opts`开始运行`spdk`框架，开始之后会运行`start_fn`函数（新线程），函数参数为`ctx`，且函数本体阻塞直到出错或退出。
- `int spdk_bdev_open_ext (const char *bdev_name, bool write, spdk_bdev_event_cb_t event_cb, void *event_ctx, struct spdk_bdev_desc **desc)`: 打开一个block device 用于IO操作，其中的`spdk_bdev_event_cb_t `是用于处理特殊事件状态的函数指针，**`desc`是块设备的描述符**
- `spdk_bdev_desc_get_bdev(), spdk_bdev_get_io_channel(), spdk_bdev_get_block_size(), spdk_bdev_get_buf_align()`: 根据块设备描述符获取块设备结构体、`io_channel`结构体，根据块设备结构体获取块的大小或最小IO Buffer地址对齐（以字节为单位）
- `void* spdk_dma_zmalloc (size_t size, size_t align, uint64_t *phys_addr)`: 分配一个具有给定大小和对齐方式的固定内存缓冲区，缓冲区将被初始为0；`phys_addr`用于记录分配的内存物理地址（可置为NULL丢弃该值）
- `bool spdk_bdev_is_zoned()`: 检查块设备是否支持分区命名空间

- `int spdk_bdev_zone_management()`: 向块设备提交一个分区管理请求，返回值可能为0（成功），或`-ENOMEM`表示IO缓冲暂时无法分配（需要等待），程序中有这样一段处理方法：

  ```C
  //hello_context包含块设备、文件数据等，其中有一个结构体spdk_bdev_io_wait_entry bdev_io_wait
  if (rc == -ENOMEM) {
  		SPDK_NOTICELOG("Queueing io\n");
  		/* In case we cannot perform I/O now, queue I/O */
  		hello_context->bdev_io_wait.bdev = hello_context->bdev;
  		hello_context->bdev_io_wait.cb_fn = hello_reset_zone;
  		hello_context->bdev_io_wait.cb_arg = hello_context;
  		spdk_bdev_queue_io_wait(hello_context->bdev, hello_context->bdev_io_channel,
  					&hello_context->bdev_io_wait);
  	}
  ```

- `struct spdk_bdev_io_wait_entry`: **将条目添加到调用线程的队列中，以便在`spdk_bdev_io`可用时收到通知。** 当`bdev I/O Submit Functions` 之一返回` -ENOMEM `时，表示`spdk_bdev_io`缓冲池没有可用的缓冲区。当调用线程上的缓冲区可用时，可以调用此函数来注册要通知的回调。 回调函数将始终在调用此函数的同一线程上调用。 该函数只能在`bdev I/O `提交函数之一返回` -ENOMEM`后立即调用。

- `int spdk_bdev_write()`: 通过缓冲区的数据向`bdev`提交一个写请求，本函数还提供了一个写完成后调用其他函数的接口，且同样可以用上述`io_wait`方法等待；此外，当需要向块设备写数据时，还可以使用[其他函数实现](https://spdk.io/doc/group__bdev__io__submit__functions.html#gaa740a114ef34d6a2f126d4e3a9dd9e9b)不同功能。

- `int spdk_bdev_read()`: 向`bdev`提交一个读请求，同样有完成之后调用其他函数以及等待的接口。

- 释放与关闭：

  - `spdk_bdev_free_io(struct spdk_bdev_io* bdev_io)`: 释放一个IO请求
  - `spdk_put_io_channel(struct spdk_io_channel* ch)`: 释放一个IO通道（异步执行）
  - `spdk_bdev_close(struct spdk_bdev_desc* desc)`: 通过快设备描述符关闭一个块设备
  - `void spdk_app_stop(int rc)`: 关闭app，之后`spdk_app_start`会不再阻塞并返回`rc`的值
  - `void spdk_dma_free(void *buf)`: 释放之前分配的内存缓冲区
  - `void spdk_app_fini (void)`: 完整事件框架的最后一步，关闭`spdk app`

  `hello_bdev`代码逻辑（其实可以画个流程图但是我累了**`orz`**）：先初始化`app_opts`并用此调用`spdk_app_start`开启app, 之后start函数会阻塞，而子线程会执行所调用的函数。之后程序依次打开（类似文件操作的`open`），获取`bdev, desc, io_channel`等参数，写数据，读数据，最后释放资源。

----

- **有关`SSD`内部**
  - 读写不平衡
  - SSD向主机开放公共接口，使其看起来像是一组固定大小的块组成，但实际上这些块只是逻辑结构，SSD更新数据时要通过*闪存转换层（ FTL）*将逻辑块映射到物理位置
  - 垃圾回收机制，SSD通过内部的日志将部分擦除块的数据移动到别的地方

- **SPDK提交IO请求**：SPDK允许用户提交比硬件队列实际可以容纳的更多请求并自动排队

- **`bdev`介绍**：SPDK块设备层(`bdev`)，提供实现类似操作系统块存储层的API，提供了：

  - 一种可插拔模块API，用于实现与不同类型的块存储设备接口的块设备。
  - `NVMe，malloc（ramdisk），Linux AIO，virtio-scsi，Ceph RBD，Pmem 和Vhost-SCSI Initiator`等驱动程序模块。
  - 用于枚举和声明SPDK 块设备，然后在这些设备上执行操作（读取，写入，取消映射等）
    的应用程序API。
  - 堆栈块设备以创建复杂I / O 管道的工具，包括逻辑卷管理（`lvol`）和分区支持（`GPT`）。
  - 通过JSON-RPC 配置块设备。
  - 请求排队，超时和重置处理。
  - 多个无锁队列，用于将I / O 发送到块设备。