NAME := kernel
SRCS := kernel.c
export MODULE  := L0
export AM_HOME := $(PWD)/../abstract-machine
ifeq ($(ARCH),)
export ARCH := x86_64-qemu
endif

log:
	make -nB \
		| grep -ve '^\(\#\|echo\|mkdir\|make\)' \
		| sed "s#$(AM_HOME)#\AM#g" \
		| sed "s#$(PWD)#.#g" \
		> $(BUILD_DIR)/compile.log


include $(AM_HOME)/Makefile
include ../oslabs.mk
image: git
