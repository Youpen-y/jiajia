#include "tools.h"
#include <jia.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    jia_init(argc, argv);

    int num_tests = 6;

    int sizes[] = {1, 1024, 4096, 40960, 409600, 4096000};


    for (int i = 0; i < num_tests; i++) {
        int data_size = sizes[i] / sizeof(int);
        int *data = (int *)malloc(sizes[i]);

        if (jiapid == 0) {
            for (int j = 0; j < data_size; j++) {
                data[j] = j + data_size;
            }
            double start_time = jia_current_time();
            jia_send((char *)data, data_size, 1, 0);
            double end_time = jia_current_time();
            printf("进程 %d 发送 %d 字节数据 用时: %f ms\n", jiapid, sizes[i], (end_time - start_time)/1000000);
        } else if (jiapid == 1) { // 进程 1 接收数据
            double start_time = jia_current_time();
            jia_recv((char *)data, data_size, 0, 0);
            double end_time = jia_current_time();
            printf("进程 %d 接收 %d 字节数据{%d} 用时: %f ms\n", jiapid, sizes[i], data[1], (end_time - start_time)/1000000);
        }
        free(data);
        jia_wait();  // 确保两端同步执行
    }
    jia_exit();
    return 0;
}

