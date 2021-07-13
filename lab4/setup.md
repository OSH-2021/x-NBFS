# 树莓派部署ceph

## 单机部署

### Step0:给树莓派安装操作系统并使用SSH连接

略

### Step1:安装基本工具

更新系统源
```
sudo apt update
```

安装`ceph-deploy`
```
sudo apt-get install ceph-deploy
```

安装`vim`
```
sudo apt install vim
```

安装`lvm2`
```
sudo apt install lvm2
```

### Step2:初始化

创建工作目录(名称可以任意)
```
mkdir test
cd test
```

初始化(`Host Name`为部署机的名称)，**注意：在`/etc/hosts`中，把回环地址对应的`hostname`给删除掉。再添加一行真正的`ip`地址和`hostname`的对应关系**
```
ceph-deploy new {Host Name}
```

由于是单机部署，修改`ceph.conf`文件
```
vim ceph.conf
```
然后将
```
osd pool default size = 1
osd pool default min size = 1
```
添加到文件中，保存并退出。

执行以下命令初始化`ceph`，并部署`admin`、`monitor`节点

```
ceph-deploy install --release {Ceph Version Name} {Host Name}
ceph-deploy mon create-initial
ceph-deploy admin {Host Name}
sudo chmod +r /etc/ceph/ceph.client.admin.keyring
ceph-deploy mgr create {Host Name}
```
与前面一样，`Host Name`为部署机的名称，`Ceph Version Name`为你要安装的ceph的版本名称。

可以使用
```
ceph --version
```
查看`ceph`的版本，并确认已安装完成。

### Step3:分配存储区域

使用命令
```
fdisk -l
```
查看系统分区，然后使用`fdisk`工具选择合适的位置创建用于挂载`ceph`的分区，以下以`/dev/test`为例。

部署`OSD`节点
```
pvcreate /dev/test
vgcreate  ceph-pool /dev/test
lvcreate -n osd0.wal -L 1G ceph-pool
lvcreate -n osd0.db -L 1G ceph-pool
lvcreate -n osd0 -l 100%FREE ceph-pool

ceph-deploy osd create \
    --data ceph-pool/osd0 \
    --block-db ceph-pool/osd0.db \
    --block-wal ceph-pool/osd0.wal \
    --bluestore {Host Name}
```

至此，已完成`Ceph`的单机部署，你可以使用命令
```
ceph -s
```
查看系统状态，或使用以下命令：
```
ceph mgr module enable dashboard
ceph mgr services
```
创建网页版界面，该指令将会生成网页端端口，将主机`IP`地址附带上端口在浏览器中打开，即可看到图形化界面。

## 分布式多节点部署

**注：使用到的四个树莓派分别命名为 rasp6 rasp7 rasp8 rasp9，其中rasp6、rasp7为存储结点，rasp8为监视节点，rasp9为管理节点。**

### Step0:给树莓派安装操作系统并使用SSH连接

略

### Step1:准备工作

修改`hostname`
```
 (rasp6)#hostnamectl set-hostname rasp6
 (rasp7)#hostnamectl set-hostname rasp7
 (rasp8)#hostnamectl set-hostname rasp8
 (rasp9)#hostnamectl set-hostname rasp9
```

利用手机热点组建局域网，查询每个节点的内网`ip`地址
```
ifconfig
```

修改`hosts`
```
sudo vi /etc/hosts
```

按照节点的`ip`添加如下信息
```
192.168.42.143 rasp6
192.168.42.24 rasp7
192.168.42.43 rasp8
192.168.42.32 rasp9
```

更新系统源
```
sudo apt update
```

安装软件包
```
sudo apt-get install ceph-deploy vim lvm2
```

在`rasp9`生成密钥对
```
ssh-keygen
```

实现`rasp9`对其它节点的无密码登录
```
ssh-copy-id pi@rasp6
ssh-copy-id pi@rasp7
ssh-copy-id pi@rasp8
```

### Step2:初始化

创建工作目录(名称可以任意)
```
mkdir test
cd test
```

在rasp9执行以下命令初始化`Ceph`，并部署`admin`、`monitor`节点

```
ceph-deploy install rasp6 rasp7 rasp8 rasp9
ceph-deploy new rasp8
```

修改`ceph.conf`文件
```
vim ceph.conf
```
然后将
```
osd pool default size = 2
```
添加到文件中，保存并退出。

初始化其它节点
```
ceph-deploy mon create-initial
ceph-deploy admin rasp9
sudo chmod +r /etc/ceph/ceph.client.admin.keyring
ceph-deploy mgr create rasp9
```

### Step3:对每个OSD节点分配存储区域

使用命令
```
fdisk -l
```
查看系统分区，然后使用`fdisk`工具选择合适的位置创建用于挂载`Ceph`的分区，以下以`/dev/test`为例。

对对应的设备进行操作，划分一块区域作为存储设备
```
sudo fdisk /dev/mmcblk0
```

将这部分区域作为存储节点
```
ceph-deploy osd create rasp6 --data /dev/mmcblk0p3
ceph-deploy osd create rasp7 --data /dev/mmcblk0p3
```

至此，已完成`Ceph`的单机部署，你可以使用命令
```
ceph -s
```
查看系统状态，或使用以下命令：
```
ceph mgr module enable dashboard
ceph mgr services
```
创建网页版界面，该指令将会生成网页端端口，将主机`IP`地址附带上端口在浏览器中打开，即可看到图形化界面。

![pictures](/lab4/files/find_ultra_ceph_king.png)