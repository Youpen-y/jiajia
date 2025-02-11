// jacobi_equation.c -- jacobi iteration for solving linear equation {A ⋅ x = b}
#include <jia.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define M 4     // 方程组的维度（矩阵 A 的行数）
#define N 4     // 方程组的维度（矩阵 A 的列数）
#define TIMES  25// 迭代次数
#define EPSILON 1e-6 // 迭代停止的精度要求(未使用)

// 假设 A 为 M x N 矩阵，b 为长度 M 的向量，x 为未知数向量
double A[M][N] = {
    {10, 2, 1, 1},
    {1, 12, 0, 1},
    {2, 1, 15, 3},
    {0, 1, 2, 20}
};  // 系数矩阵
double b[M] = {15, 14, 21, 23};     // 常数向量

// #define M 2          // 方程组的维度（矩阵 A 的行数）
// #define N 2          // 方程组的维度（矩阵 A 的列数）
// #define TIMES 25     // 迭代次数
// #define EPSILON 1e-6 // 迭代停止的精度要求

// // A 为 M x N 矩阵，b 为长度 M 的向量，x 为未知数向量
// double A[M][N] = {
//     {2, 1},
//     {5, 7}
// };  // 系数矩阵
// double b[M] = {11, 13}; // 常数向量

double **x; // x[i]: 指向机器 i 上分配的共享内存

double x_new[M]; // 用于存储下一个迭代的解向量

// Jacobi 迭代法求解 Ax = b
void jacobi() {
    int length = M / jiahosts;
    int begin = length * jiapid;
    int end = length * (jiapid + 1);

    for (int iter = 0; iter < TIMES; iter++) {
        // 进行一轮迭代
        for (int i = begin; i < end; i++) {
            float sum = b[i]; // 初始化为 b[i]
            for (int j = 0; j < N; j++) {
                if (i != j) {
                    sum -= A[i][j] * (*(x[j])); // 排除对角线元素
                }
            }
            x_new[i] = sum / A[i][i]; // 更新 x[i]
        }

        // 更新当前解为新的解
        for (int i = begin; i < end; i++) {
            *(x[i]) = x_new[i];
        }

        jia_barrier();
    }
}

// 打印前 n 个解
void print_solution(int n) {
    for (int i = 0; i < n; i++) {
        printf("*(x[%d]) = %f\n", i, *x[i]);
    }
}

int main(int argc, char **argv) {
    // 初始 JIAJIA 环境
    jia_init(argc, argv);

    int length = M / jiahosts;  // 注意，这里cond: length >= 1 
    int begin = length * jiapid;
    int end = length * (jiapid + 1);

    x = (double **)malloc(sizeof(double *) * M); // 机器私有变量，每个 double 分配到一个页上

    jia_barrier();

    /*
     * 在机器 i 上分配所用到的共享变量
     */
    for (int i = 0; i < M; i++) {    // 在各机器上分配的共享内存
        x[i] = (double *)jia_alloc(sizeof(double));
    }

    jia_barrier();

    // 此处由 master 赋初值，也可由主机各赋初值
    if (jiapid == 0) {
        for (int i = 0; i < M; i++) {
                *(x[i]) = 0.0;
        }
    }

    // 同步初始赋值
    jia_barrier();

    // 核心算法
    jacobi();

    // 同步结果
    jia_barrier();

    // 打印结果
    print_solution(M);

    free(x);
    // 退出系统
    jia_exit();
    return 0;
}
