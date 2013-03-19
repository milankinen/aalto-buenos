# Makefile for the drivers module

# Set the module name
MODULE := fs

FILES := vfs.c tfs.c sfs.c filesystems.c

SRC += $(patsubst %, $(MODULE)/%, $(FILES))
