// 使用蒙特卡罗模拟生成高斯分布（正态分布）随机对
#include <jia.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>  // atoi
#include <strings.h> // bzero
#include <sys/time.h>
#include <unistd.h> // getopt

double aint(double x) {
    if (x > 0.0)
        return floor(x);
    else
        return ceil(x);
}

// 用于生成随机数和缩放计算的常数
unsigned int M = 0; /*#define M  28*/
#define MK 10
unsigned int MM;                           /*#define MM (M - MK)  */
unsigned int NN; /*#define NN (1 << MM) */ /* 2 ** MM */
unsigned int NK; /*#define NK (1 << MK) */ /* 2 ** MK */
#define NQ 10
#define A 1220703125.0
#define S 271828183.0

extern char *optarg;
int *shared;    // 存储每个主机计算的结果的数组

int main(int argc, char **argv) {
    double half23 = 1.0, half46 = 1.0, two23 = 1.0, two46 = 1.0;
    double a_to_n;
    double a1, a2, b1, b2;
    double t1, t2, t3, t4, t5;
    double seed, temp;
    double x1, x2;
    int extra, len;
    int count[NQ + 1] = {0};
    int i, j, k, c;
    int begin, end;

    struct timeval start, stop;

    while ((c = getopt(argc, argv, "m:")) != -1)
        switch (c) {
            case 'm':
                M = atoi(optarg);
                break;
        }

    if (!M)
        M = 28;

    MM = M - MK;
    NN = 1 << MM;
    NK = 1 << MK;

    jia_init(argc, argv);

    shared = (int *)jia_alloc((NQ + 1) * sizeof(int));
    jia_barrier();
    if (jiapid == 0) {
        bzero(shared, (NQ + 1) * sizeof(int));
    }
    jia_barrier();

    /*bzero(&Tmk_stat, sizeof(Tmk_stat));*/

    /* compute (1/2)^23, (1/2)^46, 2^23, 2^46 */
    for (i = 23; i > 0; i--)
        half23 *= 0.5, half46 *= 0.5 * 0.5, two23 *= 2.0, two46 *= 2.0 * 2.0;

    /* compute a_to_n */
    a_to_n = A;
    for (i = MK; i >= 0; i--) {
        a1 = aint(half23 * a_to_n);
        a2 = a_to_n - two23 * a1;
        b1 = aint(half23 * a_to_n);
        b2 = a_to_n - two23 * b1;
        t1 = a1 * b2 + a2 * b1;
        t2 = aint(half23 * t1);
        t3 = t1 - two23 * t2;
        t4 = two23 * t3 + a2 * b2;
        t5 = aint(half46 * t4);
        a_to_n = t4 - two46 * t5;
    }

    gettimeofday(&start, 0);
    len = NN / jiahosts;
    extra = NN % jiahosts;
    if (jiapid < extra) {
        begin = jiapid * (len + 1);
        end = begin + len + 1;
    } else {
        begin = jiapid * len + extra;
        end = begin + len;
    }

    for (j = begin; j < end; j++) {
        k = j;
        seed = S;
        temp = a_to_n;

        for (i = 100; i > 0; i--) {
            if (k & 1) {
                a1 = aint(half23 * temp);
                a2 = temp - two23 * a1;
                b1 = aint(half23 * seed);
                b2 = seed - two23 * b1;
                t1 = a1 * b2 + a2 * b1;
                t2 = aint(half23 * t1);
                t3 = t1 - two23 * t2;
                t4 = two23 * t3 + a2 * b2;
                t5 = aint(half46 * t4);
                seed = t4 - two46 * t5;
            }
            if ((k >>= 1) == 0)
                break;
            {
                a1 = aint(half23 * temp);
                a2 = temp - two23 * a1;
                b1 = aint(half23 * temp);
                b2 = temp - two23 * b1;
                t1 = a1 * b2 + a2 * b1;
                t2 = aint(half23 * t1);
                t3 = t1 - two23 * t2;
                t4 = two23 * t3 + a2 * b2;
                t5 = aint(half46 * t4);
                temp = t4 - two46 * t5;
            }
        }

        a1 = aint(half23 * A);
        a2 = A - two23 * a1;

        for (i = 0; i < NK; i++) {
            b1 = aint(half23 * seed);
            b2 = seed - two23 * b1;
            t1 = a1 * b2 + a2 * b1;
            t2 = aint(half23 * t1);
            t3 = t1 - two23 * t2;
            t4 = two23 * t3 + a2 * b2;
            t5 = aint(half46 * t4);
            seed = t4 - two46 * t5;
            x1 = 2.0 * half46 * seed - 1.0;

            b1 = aint(half23 * seed);
            b2 = seed - two23 * b1;
            t1 = a1 * b2 + a2 * b1;
            t2 = aint(half23 * t1);
            t3 = t1 - two23 * t2;
            t4 = two23 * t3 + a2 * b2;
            t5 = aint(half46 * t4);
            seed = t4 - two46 * t5;
            x2 = 2.0 * half46 * seed - 1.0;

            t1 = x1 * x1 + x2 * x2;
            if (t1 <= 1.0) {
                t2 = sqrt(-2.0 * log(t1) / t1);
                t3 = fabs(x1 * t2);
                t4 = fabs(x2 * t2);
                count[(int)floor(t3 > t4 ? t3 : t4)]++;
                count[NQ]++;
            }
        }
    }

    if (jiahosts > 1) {
        jia_lock(0);

        for (i = 0; i <= NQ; i++)
            shared[i] += count[i];

        jia_unlock(0);

        jia_barrier();
    } else
        shared = count;

    gettimeofday(&stop, 0);

    printf("Benchmark 1 Results:\n\nCPU Time = %.3f\nN = 2 ^ %d\nNo. Gaussian "
           "Pairs = %d\nCounts:\n",
           (stop.tv_sec + stop.tv_usec * 1e-6) -
               (start.tv_sec + start.tv_usec * 1e-6),
           M, shared[NQ]);

    if (jiapid == 0)
        for (i = 0; i < NQ; i++)
            printf("%3d\t%12d\n", i, shared[i]);

    jia_exit();
}
