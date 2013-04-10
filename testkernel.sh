#!/bin/sh

sh prepdisk.sh

echo "Running kernel tests..."
# start kernel with given program
yams buenos kernel_tests

echo "Kernel tests over!"
