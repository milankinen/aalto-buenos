#!/bin/sh

sh prepdisk.sh

echo "Starting program $1..."
# start kernel with given program
yams buenos "initprog=[disk1]$1"

