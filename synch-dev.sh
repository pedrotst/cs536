#!/bin/bash

# Pass as argument which server you want to connect this to

# umount $(pwd)/dev
# If unmount does not work try the following:
diskutil unmount force $(pwd)/dev

rm -rf dev
mkdir dev

sshfs pdacost@$1.cs.purdue.edu:/homes/pdacost/gitprojects/cs546/dev dev
