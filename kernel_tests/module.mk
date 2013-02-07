# Makefile for the kernel module

# Set the module name
MODULE := kernel_tests


FILES := makeWater.c lock_tests.c cond_tests.c thread_sleep_tests.c

SRC += $(patsubst %, $(MODULE)/%, $(FILES))

