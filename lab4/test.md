# 树莓派Ceph部署I/O测试

## 测试步骤

### Step1:创建测试存储池

在管理节点`（admin）`利用`ceph`自带工具`rados`创建测试池
```
rados mkpool {Pool Name}
```
上述`Pool Name`可自己替换。你可以利用：
```
rados lspools
```
查看当前分配的存储池，也可以使用：
```
rados rmpool {Pool Name} {Pool Name} --yes-i-really-really-mean-it
```
删除创建的存储池。

### Step2:读写测试

利用`rados bench`进行测试，该工具的语法为：
```
rados bench -p <pool_name> <seconds> <write|seq|rand> -b <block size> -t --no-cleanup
```

- **pool_name**：测试所针对的存储池；
- **seconds**：测试所持续的秒数；
- **write | seq | rand**：操作模式，`write`表示写、`seq`表示顺序读、`rand`表示随机读；
- **-b**：`block size`，即块大小，默认为`4M`；
- **-t**：读/写并行数，默认为`16`；
- **--no-cleanup**：表示测试完成后不删除测试用数据。在做读测试之前，需要使用该参数来运行一遍写测试来产生测试数据，在全部测试结束后可以运行 `rados -p <pool_name> cleanup` 来清理所有测试数据。

使用该工具时需要`root`权限.

### 注意事项

可以利用
```
rados -p test_buffer cleanup
rados rmpool test_buffer test_buffer --yes-i-really-really-mean-it
```
清除缓存与缓存池。

清除存储池时可能会报错：
```
Check your monitor configuration - `mon allow pool delete` is set to false by default, change it to true to allow deletion of pools
```
可以更改配置文件`ceph.conf`，使用`vim`添加：
```
[mon]
mon allow pool delete = true
```
重启服务
```
systemctl restart ceph-mon.target
```
即可删除存储池。

## 测试结果

### 单节点

创建名为`test_buffer`的存储池，则使用以下指令测试读写性能：
```
rados bench -p test_buffer 10 write --no-cleanup
```
得到如下结果：
```
hints = 1
Maintaining 16 concurrent writes of 4194304 bytes to objects of size 4194304 for up to 10 seconds or 0 objects
Object prefix: benchmark_data_rasp7_11728
  sec Cur ops   started  finished  avg MB/s  cur MB/s last lat(s)  avg lat(s)
    0       0         0         0         0         0           -           0
    1      16        16         0         0         0           -           0
    2      16        16         0         0         0           -           0
    3      16        16         0         0         0           -           0
    4      16        32        16    15.997        16     3.85778     3.85377
    5      16        32        16   12.7973         0           -     3.85377
    6      16        32        16   10.6643         0           -     3.85377
    7      16        32        16   9.14077         0           -     3.85377
    8      16        48        32   15.9961        16     3.73332     3.79397
    9      16        48        32   14.2186         0           -     3.79397
   10      16        48        32   12.7968         0           -     3.79397
   11      16        49        33    11.997   1.33333     3.43131     3.78298
Total time run:         11.253826
Total writes made:      49
Write size:             4194304
Object size:            4194304
Bandwidth (MB/sec):     17.4163
Stddev Bandwidth:       6.4247
Max bandwidth (MB/sec): 16
Min bandwidth (MB/sec): 0
Average IOPS:           4
Stddev IOPS:            1
Max IOPS:               4
Min IOPS:               0
Average Latency(s):     3.60507
Stddev Latency(s):      0.517278
Max latency(s):         3.85778
Min latency(s):         0.269438
```

输入
```
rados bench -p test_buffer 10 seq
```
得到结果：
```
hints = 1
  sec Cur ops   started  finished  avg MB/s  cur MB/s last lat(s)  avg lat(s)
    0       0         0         0         0         0           -           0
    1      16        18         2    7.9957         8    0.466441    0.454914
    2      16        26        10   19.9904        32     1.39289     1.27397
    3      16        32        16   21.3249        24     1.40679     1.55934
    4      16        37        21   20.9926        20    0.835225     1.57157
    5      16        43        27   21.5929        24     3.38773     1.78612
    6      16        48        32   21.3266        20     1.82301     1.90556
    7      16        49        33   18.8512         4     1.26512     1.88615
    8      15        49        34    16.994         4     1.50982     1.87509
    9       2        49        47   20.8816        52     6.84027     2.73826
Total time run:       9.030218
Total reads made:     49
Read size:            4194304
Object size:          4194304
Bandwidth (MB/sec):   21.7049
Average IOPS:         5
Stddev IOPS:          3
Max IOPS:             13
Min IOPS:             1
Average Latency(s):   2.86559
Max latency(s):       8.35199
Min latency(s):       0.443386
```

有测试结果知，写速度小于读速度。

### 多节点

创建名为`test`的存储池，则使用以下指令测试读写性能：
```
rados bench -p test 10 write --no-cleanup
```
得到如下结果：
```
hints = 1
Maintaining 16 concurrent writes of 4194304 bytes to objects of size 4194304 for up to 10 seconds or 0 objects
Object prefix: benchmark_data_rasp9_4338
  sec Cur ops   started  finished  avg MB/s  cur MB/s last lat(s)  avg lat(s)
    0       0         0         0         0         0           -           0
    1      16        16         0         0         0           -           0
    2      16        16         0         0         0           -           0
    3      16        16         0         0         0           -           0
    4      16        16         0         0         0           -           0
    5      16        16         0         0         0           -           0
    6      16        16         0         0         0           -           0
    7      16        16         0         0         0           -           0
    8      16        16         0         0         0           -           0
    9      16        16         0         0         0           -           0
   10      16        16         0         0         0           -           0
   11      16        16         0         0         0           -           0
   12      16        16         0         0         0           -           0
   13      16        16         0         0         0           -           0
   14      16        16         0         0         0           -           0
   15      16        16         0         0         0           -           0
   16      16        16         0         0         0           -           0
   17      16        16         0         0         0           -           0
   18      16        16         0         0         0           -           0
   19      16        16         0         0         0           -           0
2021-07-12 22:20:38.890527 min lat: 9999 max lat: 0 avg lat: 0
  sec Cur ops   started  finished  avg MB/s  cur MB/s last lat(s)  avg lat(s)
   20      16        16         0         0         0           -           0
   21      16        16         0         0         0           -           0
   22      16        16         0         0         0           -           0
   23      16        16         0         0         0           -           0
   24      16        16         0         0         0           -           0
   25      16        16         0         0         0           -           0
   26      16        16         0         0         0           -           0
   27      16        16         0         0         0           -           0
   28      16        16         0         0         0           -           0
   29      16        16         0         0         0           -           0
   30      16        16         0         0         0           -           0
   31      16        17         1  0.128999  0.129032     30.0679     30.0679
   32      15        17         2  0.249936         4     31.1295     30.5987
   33      15        17         2  0.242362         0           -     30.5987
   34      15        17         2  0.235233         0           -     30.5987
   35      15        17         2  0.228512         0           -     30.5987
   36      15        17         2  0.222165         0           -     30.5987
   37      15        17         2   0.21616         0           -     30.5987
   38      15        17         2  0.210471         0           -     30.5987
   39      14        17         3  0.307612  0.571429     39.0033     33.4002
2021-07-12 22:20:58.896110 min lat: 30.0679 max lat: 39.3691 avg lat: 34.8924
  sec Cur ops   started  finished  avg MB/s  cur MB/s last lat(s)  avg lat(s)
   40      13        17         4  0.399895         4     39.3691     34.8924
   41      12        17         5  0.487674         4     40.8165     36.0773
   42      12        17         5  0.476063         0           -     36.0773
   43      10        17         7  0.650988         4     42.3846     37.8437
   44      10        17         7  0.636193         0           -     37.8437
   45       9        17         8   0.71092         2     44.1692     38.6344
   46       8        17         9  0.782399         4     45.8449     39.4356
   47       8        17         9  0.765752         0           -     39.4356
   48       7        17        10   0.83311         2     47.1388     40.2059
   49       6        17        11  0.897718         4     48.9623     41.0019
   50       6        17        11  0.879764         0           -     41.0019
   51       3        17        14   1.09774         6     50.0935     42.9497
   52       2        17        15   1.15354         4     51.4813     43.5185
   53       2        17        15   1.13177         0           -     43.5185
Total time run:         53.940189
Total writes made:      17
Write size:             4194304
Object size:            4194304
Bandwidth (MB/sec):     1.26066
Stddev Bandwidth:       1.56945
Max bandwidth (MB/sec): 6
Min bandwidth (MB/sec): 0
Average IOPS:           0
Stddev IOPS:            0
Max IOPS:               1
Min IOPS:               0
Average Latency(s):     42.9328
Stddev Latency(s):      8.28509
Max latency(s):         53.2106
Min latency(s):         23.8699
```

输入
```
rados bench -p test 10 seq
```
得到结果：
```
hints = 1
  sec Cur ops   started  finished  avg MB/s  cur MB/s last lat(s)  avg lat(s)
    0       0         0         0         0         0           -           0
    1      16        16         0         0         0           -           0
    2      16        16         0         0         0           -           0
    3      16        16         0         0         0           -           0
    4      16        17         1  0.999659         1     3.13949     3.13949
    5      16        17         1  0.799743         0           -     3.13949
    6      15        17         2   1.33292         2     5.35279     4.24614
    7      15        17         2   1.14252         0           -     4.24614
    8      15        17         2  0.999713         0           -     4.24614
    9      15        17         2  0.888642         0           -     4.24614
   10      15        17         2  0.799729         0           -     4.24614
   11      15        17         2  0.726993         0           -     4.24614
   12      15        17         2  0.666418         0           -     4.24614
   13      14        17         3  0.922742  0.571429     12.6898     7.06068
   14      14        17         3  0.856839         0           -     7.06068
   15      13        17         4    1.0663         2     14.1281     8.82753
   16      13        17         4  0.999664         0           -     8.82753
   17      13        17         4  0.940866         0           -     8.82753
   18      13        17         4  0.888579         0           -     8.82753
   19      13        17         4  0.841817         0           -     8.82753
2021-07-12 22:19:12.263715 min lat: 3.13949 max lat: 19.9771 avg lat: 15.9126
  sec Cur ops   started  finished  avg MB/s  cur MB/s last lat(s)  avg lat(s)
   20       6        17        11   2.19926       5.6     19.9771     15.9126
   21       6        17        11   2.09454         0           -     15.9126
   22       6        17        11   1.99934         0           -     15.9126
   23       6        17        11   1.91242         0           -     15.9126
   24       6        17        11   1.83274         0           -     15.9126
   25       6        17        11   1.75944         0           -     15.9126
   26       6        17        11   1.69177         0           -     15.9126
Total time run:       26.144628
Total reads made:     17
Read size:            4194304
Object size:          4194304
Bandwidth (MB/sec):   2.60092
Average IOPS:         0
Stddev IOPS:          0
Max IOPS:             1
Min IOPS:             0
Average Latency(s):   19.3318
Max latency(s):       26.1389
Min latency(s):       3.13949
```

输入
```
rados bench -p test 10 rand
```
得到结果：
```
hints = 1
  sec Cur ops   started  finished  avg MB/s  cur MB/s last lat(s)  avg lat(s)
    0       0         0         0         0         0           -           0
    1      16        16         0         0         0           -           0
    2      16        16         0         0         0           -           0
    3      15        16         1   1.33282   1.33333     2.99502     2.99502
    4      16        18         2   1.99928         4     3.08755     3.04128
    5      16        18         2   1.59948         0           -     3.04128
    6      16        19         3   1.99938         2     5.68831     3.92362
    7      16        20         4   2.28503         4     6.22407     4.49874
    8      16        20         4   1.99941         0           -     4.49874
    9      16        21         5   2.22158         2     8.71574     5.34214
   10      16        21         5   1.99943         0           -     5.34214
   11      16        22         6   2.18122         2     10.0802     6.13182
   12      16        22         6   1.99945         0           -     6.13182
   13      16        22         6   1.84565         0           -     6.13182
   14      16        22         6   1.71383         0           -     6.13182
   15      16        22         6   1.59957         0           -     6.13182
   16      16        22         6   1.49961         0           -     6.13182
   17      15        22         7   1.64663  0.666667     16.3312     7.58887
   18      14        22         8   1.77732         4     17.7654     8.86094
   19      14        22         8   1.68378         0           -     8.86094
2021-07-12 22:19:58.351745 min lat: 2.99502 max lat: 19.2595 avg lat: 10.0163
  sec Cur ops   started  finished  avg MB/s  cur MB/s last lat(s)  avg lat(s)
   20      13        22         9   1.79954         2     19.2595     10.0163
   21      12        22        10   1.90426         4     20.8385     11.0986
   22      12        22        10   1.81771         0           -     11.0986
   23      11        22        11   1.91255         2      22.548     12.1394
   24      11        22        11   1.83286         0           -     12.1394
   25      11        22        11   1.75955         0           -     12.1394
   26      11        22        11   1.69187         0           -     12.1394
   27      11        22        11   1.62921         0           -     12.1394
   28      11        22        11   1.57102         0           -     12.1394
   29      11        22        11   1.51685         0           -     12.1394
   30       6        22        16   2.13279   2.85714     20.7779     16.8278
   31       6        22        16   2.06399         0           -     16.8278
   32       5        22        17   2.12446         2     28.0814     17.4898
   33       4        22        18   2.18126         4     26.9966     18.0179
   34       4        22        18   2.11711         0           -     18.0179
Total time run:       34.362949
Total reads made:     22
Read size:            4194304
Object size:          4194304
Bandwidth (MB/sec):   2.5609
Average IOPS:         0
Stddev IOPS:          0
Max IOPS:             1
Min IOPS:             0
Average Latency(s):   20.2445
Max latency(s):       34.3534
Min latency(s):       2.99502
```

有测试结果知，写速度小于读速度；由于网络通信延迟过高，再加上机器温度供电影响，多节点性能反而小于单节点。