#!/bin/bash

# Prints all commands being executed
set -x 

# Usage: push_dev message
# and message is the git commit message
# To format a git message curly braces, e.g. {"this is a valid message"}

#TODO: Change this variable for the current version being worked

LABVER=labs/lab2/v3

# Make sure change the file names and the directory
cp dev/myftp.c $LABVER
cp dev/myftpd.c $LABVER
 
git add $LABVER
git commit -m "$1"
git push
