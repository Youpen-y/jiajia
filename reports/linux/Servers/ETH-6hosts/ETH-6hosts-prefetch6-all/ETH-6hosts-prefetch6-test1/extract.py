
    
import os
import re

# 指定目标目录和输出文件
input_directory = "."  # 替换为你的目录路径
output_file = "output_times_with_filenames.txt"  # 提取的时间和文件名保存到这个文件

# 用于存储文件名和提取的时间
extracted_data = []

# 遍历目录下所有文件
for root, _, files in os.walk(input_directory):
    for file in files:
        file_path = os.path.join(root, file)
        try:
            # 读取文件内容
            with open(file_path, "r") as f:
                for line in f:
                    # 匹配以 'real' 开头的行，并提取时间部分
                    match = re.match(r"real\s+\d+m\d+\.\d+s", line)
                    if match:
                        time = match.group().split("\t")[-1]  # 提取时间部分
                        extracted_data.append(f"{file} {time}")
        except Exception as e:
            print(f"无法读取文件 {file_path}: {e}")

# 将提取的时间和文件名写入输出文件
try:
    with open(output_file, "w") as f:
        for entry in extracted_data:
            f.write(entry + "\n")
    print(f"提取完成！数据已保存到 {output_file}")
except Exception as e:
    print(f"无法写入文件 {output_file}: {e}")

