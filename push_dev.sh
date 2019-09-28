#!/bin/bash

# Prints all commands being executed
set -x 

# Usage: push_dev filepath message
# Where filepath is lab/vX
# and message is the git commit message
# To format a git message curly braces, e.g. {"this is a valid message"}

# Make sure change the file names and the directory
cp dev/myftp.c labs/lab2/v2
cp dev/myftpd.c labs/lab2/v2
 
git add labs/lab2/v2
git commit -m $2
git push
