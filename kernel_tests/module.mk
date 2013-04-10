# Makefile for the kernel module

# Set the module name
MODULE := kernel_tests


FILES := make_water.c lock_tests.c cond_tests.c thread_sleep_tests.c canal.c \
 thread_priority_tests.c test_network.c test_sfs.c

SRC += $(patsubst %, $(MODULE)/%, $(FILES))

