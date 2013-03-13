#!/bin/sh

# remove previous filesystem
rm store.file

echo "Creating file system storage..."
# create new filesystem
./util/tfstool create store.file 4096 disk1

# add our userland programs to it
util/tfstool write store.file tests/halt halt
util/tfstool write store.file tests/test_adventure test_adventure
util/tfstool write store.file tests/test_join test_join
util/tfstool write store.file tests/test_panic test_panic
util/tfstool write store.file tests/test_fs_syscall test_fs_syscall
util/tfstool write store.file tests/test.txt test.txt
util/tfstool write store.file tests/write write
util/tfstool write store.file tests/shell shell
util/tfstool write store.file tests/cat cat
util/tfstool write store.file tests/touch touch
util/tfstool write store.file tests/append append
util/tfstool write store.file tests/bigfile.txt bigfile.txt


util/tfstool write store.file tests/run_all_tests run_all_tests

echo "Starting program $1..."
# start kernel with given program
yams buenos "initprog=[disk1]$1"

