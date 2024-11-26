import re
import sys
from pathlib import Path

file1 = 'jiajia178.log'
file2 = 'jiajia196.log'
output_file = 'merge.log'

sort_lines = []
result_lines = []

def sort_logs():
    log_lines = []
    pattern = r'\[TIME: (\d+)\].*\(.*\.c:\d+:.*\)'

    # 每一条对应的log信息作为一个单位
    for filename in [file1, file2]:
        try:
            file_path = Path(filename)
            filename_str = str(file_path.name)  # 获取文件名字符串
            current_str = ''
            current_time = 0
            current_host = ''
            
            with open(file_path, 'r') as f:
                for line in f:
                    match = re.search(pattern, line)
                    if match:
                        if (current_str):
                            log_lines.append((current_time, current_str.strip(), current_host))
                        current_time = int(match.group(1))
                        current_str = line
                        # 存储为三元组：(时间戳, 日志行, 文件名)
                        if(filename_str == 'jiajia178.log'):
                            current_host = 'host 0'
                        if(filename_str == 'jiajia196.log'):
                            current_host = 'host 1'
                    else:
                        current_str = current_str + line
        except FileNotFoundError:
            print(f"文件不存在: {file_path}")
            continue
    if (current_str):
        log_lines.append((current_time, current_str.strip(), current_host))
    
    # 根据时间戳排序
    origin_lines = sorted(log_lines, key=lambda x: x[0])

    # 将中间()的部分清空
    clean_pattern = r'\(.*?\)'
    for line in origin_lines:
        cleaned_line = re.sub(clean_pattern, '', line[1])
        sort_lines.append((cleaned_line, line[2]))

    # sort1_lines = []
    # sort1_pattern = r'.*is send to.*'
    # for line in filter_lines:
    #     sort1_match = re.search(sort1_pattern, line[0])
    #     if sort1_match:
    #         sort1_lines.append(line)
    
    # 输出排序后的结果，包含文件名
    # 写入排序后的结果
    with open(output_file, 'w', encoding='utf-8') as out_f:
        out_f.write("\n排序后的日志内容:\n")
        out_f.write("-" * 80 + "\n")
        for timestamp, line, filename in origin_lines:
            out_f.write(f"[{filename}] {line}\n")

def filter_logs():
    filter1_pattern = r'.*is send to.*'
    for line in sort_lines:
        filter1_match = re.search(filter1_pattern, line[0])
        if filter1_match:
            result_lines.append(line)
    
    # 输出排序后的结果，包含文件名
    # 写入排序后的结果
    with open(output_file, 'w', encoding='utf-8') as out_f:
        out_f.write("\n排序后的日志内容:\n")
        out_f.write("-" * 80 + "\n")
        for line, filename in result_lines:
            out_f.write(f"[{filename}] {line}\n")

if __name__ == "__main__":
    sort_logs()
    filter_logs()