DEFAULT_CC=cc
ifeq ($(origin CC),default)
CC=cc
endif

DEFAULT_CXX=c++
ifeq ($(origin CXX),default)
CXX=c++
endif

DEFAULT_LD=ld
ifeq ($(origin LD),default)
LD=ld
endif

CCAR=ar
CC_TYPE=gcc
LD_TYPE=bfd
