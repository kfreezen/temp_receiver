#!/bin/sh

# Extract the tar.gz
tar -zxf receiver-latest.tar.gz
make clean
make && systemctl restart receiver # receiver is the name of the receiver service.

# And now latest receiver is running.
