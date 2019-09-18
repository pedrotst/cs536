#!/bin/bash

# Pass as argument which server you want to connect this to

umount $(pwd)/dev
rm -rf dev

mkdir dev
sshfs pdacost@$1.cs.purdue.edu:/homes/pdacost/my_gits/cs546/dev dev
