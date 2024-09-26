#!/bin/bash
PROGRAM=sor
ARCH=linux
cd ./lib/$ARCH || exit
make all
cd ../../apps/$PROGRAM/$ARCH || exit
make all