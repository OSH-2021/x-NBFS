## Notes

####  SPDK

 - **如何编译一个使用SPDK的程序**：根据`spdk_root/Makefile`和`spdk_root/mk/spdk.app.mk`中的代码，在`spdk_root`中输入`make`命令后，`spdk.app.mk`会被调用，其中有这样一段：

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

- **如何成功运行v\*\*ay**: 各个平台能够成功的应用程序：

  - iOS：小火箭
  - Windows：v2rayN
  - Linux：

  

