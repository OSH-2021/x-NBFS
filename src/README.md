# NBFS使用教程

### Step1 编译

```
./configure --with-fuse
make
```

### Step2 获取空闲NVMe设备，并初始化文件系统

在`SPDK`目录下输入命令：

```
HUGEMEM=5120 scripts/setup.sh
scripts/gen_nvme.sh --json-with-subsystems > rocksdb.json
test/blobfs/mkfs/mkfs rocksdb.json Nvme0n1
```

### Step3 使用FUSE

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

### Step4 运行

在`Step3`之后，终端会被阻塞，新打开一个终端，使用cd命令进入挂载点`/mnt/fuse`,接着可以使用常见的`shell`指令对文件操作。

支持的命令有：

- ls
- echo 文件内容>文件名
- rm
- mv
- ...
