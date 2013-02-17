#!/bin/sh

# remove previous filesystem
rm store.file

echo "Creating file system storage..."
# create new filesystem
./util/tfstool create store.file 2048 disk1

# add our userland programs to it
util/tfstool write store.file tests/halt halt
# TODO: all own programs...

echo "Starting program $1..."
# start kernel with given program
yams buenos "initprog=[disk1]$1"

