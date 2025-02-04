NAME           := kernel
SRCS           := framework/main.c $(shell find -L ./src/ -name "*.c")
INC_PATH       := include/ framework/

CFLAGS += -g

export MODULE := L1
export AM_HOME := $(PWD)/../abstract-machine

ifeq ($(ARCH),)
export ARCH    := x86_64-qemu
endif

test:
	@gcc $(shell find src/pmm.c)  \
		$(shell find test/ -name "*.c") \
		-Iframework -I../abstract-machine/klib/include -lpthread -g\
		-o build/test

log:
	make -nB \
		| grep -ve '^\(\#\|echo\|mkdir\|make\)' \
		| sed "s#$(AM_HOME)#\AM#g" \
		| sed "s#$(PWD)#.#g" \
		> $(BUILD_DIR)/compile.log

debug:
	qemu-system-x86_64 -s -S -machine accel=tcg -smp "4,sockets=1" \
		-drive format=raw,file=build/kernel-x86_64-qemu &
	gdb -x debug.py

include $(AM_HOME)/Makefile
include ../oslabs.mk

#image: git
