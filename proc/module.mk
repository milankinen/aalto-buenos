# Makefile for the kernel module

# Set the module name
MODULE := proc


FILES := exception.c elf.c process.c syscall.c syscall_handler_fs.c syscall_handler_proc.c \
		syscall_helpers.c

SRC += $(patsubst %, $(MODULE)/%, $(FILES))

