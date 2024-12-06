#!/bin/bash
ARCH=linux
MODE=IPoIB_LOCKFREE1
TIMEOUT=30

CLEAN=false
ALLTEST=true
RUN=true
tests=("lu" "ep")

run_app() {
    # 进入文件夹并在reports下创建对应的log文件
    if [ ! -f ./reports/$ARCH/$MODE/"$1" ]; then
        echo "touch reports/$ARCH/$MODE/$1 file..."
        touch ./reports/$ARCH/$MODE/"$1"
    fi

    # 运行程序
    cd ./apps/"$1"/$ARCH || exit
    { time ./"$1"; } >& "../../../reports/$ARCH/$MODE/""$1""" &
    local pid=$!
    cd ../../..
    echo "$pid"
}

listen() {
    time=0
    while [[ $time -lt $TIMEOUT ]]; do
        # 检查进程是否存在
        # echo "$time"
        if ! ps -p "$1" > /dev/null; then
            echo "Process with PID $1 has completed within the time limit"
            return 0
        fi
        ((time++))
        sleep 1
    done
    echo "Terminating process with PID $1"
    kill "$1"
    return 1
}

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
if $CLEAN; then
    make clean -C ./lib/$ARCH
fi
make all -C ./lib/$ARCH
sleep 1

# 编译app
echo -e "\nmake all apps..."
for dir in apps/*/; do
    echo "compile ${dir%/}..."
    if $CLEAN; then
        make clean -C ./"${dir%/}"/$ARCH
    fi
    make all -C ./"${dir%/}"/$ARCH
done
sleep 1

# 拷贝.jiahosts到所有文件夹下(从pi出发)
echo -e "\ncopy .jiahosts..."
for dir in apps/*/ ; do
        cp apps/.jiahosts "${dir%/}"/$ARCH/
done
sleep 1

# 拷贝 system.conf 文件到所有文件夹下
echo -e "\ncopy system.conf..."
for dir in apps/*/; do
        cp apps/system.conf "${dir%/}"/$ARCH/
done
sleep 1

# 请求用户输入密码
# read -s -p "Please enter your sudo password:" password

# 运行所有app || 运行指定app
if $RUN; then
    if $ALLTEST; then
        for dir in apps/*/; do
            echo -e "\nrunning ${dir%/}..."
            # shellcheck disable=SC2046
            pid=$(run_app $(basename "${dir%/}"))
            listen "$pid"

            # 等待5秒后继续运行下一个程序
            sleep 1
        done
    else
        for test in "${tests[@]}"; do
            echo -e "\nrunning ${test}..."
            pid=$(run_app "$test")
            listen "$pid"

            # 等待5秒后继续运行下一个程序
            sleep 1
        done
    fi
fi
