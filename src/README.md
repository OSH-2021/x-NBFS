# NBFS源代码

## spdk_NBFS

- `spdk_NBFS`基于`SPDK`的`NBFS`通用接口版本,实现代码位于:

```
spdk_NBFS/lib/NBFS/NBFS.c
```

- `B+`树库函数源码位于:
```
`spdk_NBFS/include/spdk/Bplus_Tree.h
```

## NBFS2

`NBFS2`是`NBFS`的专用接口版本，不同于`spdk_NBFS`，`NBFS2`中为专用接口源码以及测试代码，它门们也需要配合`SPDK`使用。