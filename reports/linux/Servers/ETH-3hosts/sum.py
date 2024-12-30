import os
import re
from collections import defaultdict

# 指定顶层目录和汇总文件
top_directory = "."  # 替换为你的顶层目录路径
summary_file = "summary_output.txt"  # 汇总文件路径

# 用于存储文件名对应的时间列表
summary_data = defaultdict(list)

# 遍历顶层目录中的二级子目录，按子目录名排序
for subdir in sorted(next(os.walk(top_directory))[1]):  # 获取一级子目录并按名称排序
    subdir_path = os.path.join(top_directory, subdir)
    for root, _, files in os.walk(subdir_path):  # 遍历每个子目录下的文件
        for file in files:
            if file == "output_times_with_filenames.txt":  # 找到目标文件
                file_path = os.path.join(root, file)
                try:
                    # 读取目标文件内容
                    with open(file_path, "r") as f:
                        for line in f:
                            # 匹配文件名和时间格式
                            match = re.match(r"(\S+)\s+(\d+)m([\d\.]+)s", line)
                            if match:
                                filename = match.group(1)
                                minutes = int(match.group(2))
                                seconds = float(match.group(3))
                                total_seconds = minutes * 60 + seconds
                                summary_data[filename].append(f"{total_seconds:.3f}")
                except Exception as e:
                    print(f"无法读取文件 {file_path}: {e}")

# 将汇总结果写入输出文件
try:
    with open(summary_file, "w") as f:
        for filename, times in summary_data.items():
            f.write(f"{filename} {' '.join(times)}\n")
    print(f"汇总完成！数据已保存到 {summary_file}")
except Exception as e:
    print(f"无法写入文件 {summary_file}: {e}")

