#!/bin/bash
ARCH=linux
MODE=WLAN
TIMEOUT=25

# 创建reports及其子文件夹文件夹
if [ ! -d ./reports ]; then
    echo "make reports dir..."
    mkdir reports
fi
if [ ! -d ./reports/$ARCH ]; then
    echo "make reports/$ARCH dir..."
    mkdir reports/$ARCH
fi
if [ ! -d ./reports/$ARCH/$MODE ]; then
    echo "make reports/$ARCH/$MODE dir..."
    mkdir reports/$ARCH/$MODE
fi

# 创建libjia库
echo -e "\nmake libjia.a..."
cd ./lib/$ARCH || exit
make all
cd ../../apps || exit
sleep 1

# 编译app
echo -e "\nmake all apps..."
for dir in */; do
    cd ./$dir/$ARCH || exit
    echo "compile ${dir%/}..."
    make all >> /dev/null
    cd ../..
done
sleep 1

# 拷贝.jiahosts到所有文件夹下(从pi出发)
echo -e "\ncopy .jiahosts..."
for dir in */; do
    cd ./$dir/$ARCH || exit
    cp ../../.jiahosts .
    cd ../..
done
sleep 1

# 拷贝 system.conf 文件到所有文件夹下
echo -e "\ncopy system.conf..."
for dir in */; do
    cd ./$dir/$ARCH || exit
    cp ../../system.conf .
    cd ../..
done
sleep 1

# 请求用户输入密码
# read -s -p "Please enter your sudo password:" password

# 运行所有apps
for dir in */; do

    # 进入文件夹并在reports下创建对应的log文件
    echo -e "\nrunning ${dir%/}..."
    cd ./$dir/$ARCH || exit
    if [ ! -f ../../../reports/$ARCH/$MODE/${dir%/} ]; then
        echo "touch reports/$ARCH/$MODE/${dir%/} file..."
        touch ../../../reports/$ARCH/$MODE/${dir%/}
    fi

    #运行程序
    # echo "$password" | sudo -S ./"${dir%/}" >> ../../../reports/$ARCH/${dir%/}
    ./"${dir%/}" > ../../../reports/$ARCH/$MODE/${dir%/} &

    #获得进程pid号并进行监视，防止进程超时
    pid=$!
    sleep $TIMEOUT
    if ps -p $pid > /dev/null
    then
        # 如果进程仍在运行，则终止该进程
        echo "Terminating process with PID $pid"
        kill $pid
    else
        echo "Process with PID $pid has completed within the time limit"
    fi

    cd ../..
    sleep 5
done
