# Makefile for the kernel module

# Set the module name
MODULE := kernel_tests


<<<<<<< HEAD
FILES := makeWater.c lock_tests.c cond_tests.c
=======
FILES := make_water.c lock_tests.c cond_tests.c
>>>>>>> 7cee4311fa57dae7867e2e3b616733a08958b146

SRC += $(patsubst %, $(MODULE)/%, $(FILES))

