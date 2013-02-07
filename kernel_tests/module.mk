# Makefile for the kernel module

# Set the module name
MODULE := kernel_tests


FILES := make_water.c lock_tests.c cond_tests.c

SRC += $(patsubst %, $(MODULE)/%, $(FILES))

