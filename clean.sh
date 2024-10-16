#!/bin/bash
ARCH=linux
MODE=WLAN
TIMEOUT=20

# 删除reports文件夹及其子文件夹
if [ ! -d ./reports ]; then
    echo "delete reports dir..."
    rm -rf reports/*
fi


echo -e "\nmake clean libjia.a..."
cd ./lib/$ARCH || exit
make clean
cd ../../apps || exit
sleep 1

echo -e "\nmake clean all apps..."
for dir in */; do
    cd ./$dir/$ARCH || exit
    echo "clean ${dir%/}..."
    make clean
    cd ../..
done
sleep 1
