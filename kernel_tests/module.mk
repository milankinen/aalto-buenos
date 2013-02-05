# Makefile for the kernel module

# Set the module name
MODULE := kernel_tests


FILES := makeWater.c

SRC += $(patsubst %, $(MODULE)/%, $(FILES))

