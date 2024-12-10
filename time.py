import subprocess
import time
import sys

# 要执行的命令
command = "./" + sys.argv[1]

# 启动进程
process = subprocess.Popen(command, shell=True)
timeout = int(sys.argv[2])

# 记录开始时间
start_time = time.time()

# 检查进程是否在超时时间内结束
while process.poll() is None:
    time_elapsed = time.time() - start_time
    if time_elapsed > timeout:
        # print("进程超时，将被终止")
        process.terminate()  # 或者使用 process.kill() 来强制终止进程
        break

# 等待进程结束
process.wait()

print(time_elapsed)