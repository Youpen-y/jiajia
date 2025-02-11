// jacobi_heat.c -- jacobi iteration for heat conduction

#include <jia.h>
#include <stdio.h>
#include <unistd.h>

#define M 1024
#define N 1024
#define TIMES 5000

float *grid;
float scratch[M][N];
float boundary_value = 100;

void initialize_grid() {
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            if (i == 0 || i == M - 1 || j == 0 || j == N - 1) {
                grid[i * N + j] = boundary_value;
            } else {
                grid[i * N + j] = 0.0f;
            }
        }
    }
}

int main(int argc, char **argv) {
    jia_init(argc, argv);
    sleep(10);
    
    grid = (float *)jia_alloc(M * N * sizeof(float));
    if (jiapid == 0) {
        initialize_grid();
    }

    jia_barrier();

    int length = M / jiahosts;
    int begin = length * jiapid;
    int end = length * (jiapid + 1);

    for (int time = 0; time < TIMES; time++) {
        // Jacobi iteration
        for (int i = begin; i < end; i++) {
            for (int j = 0; j < N; j++) {
                double top = (i > 0) ? grid[(i - 1) * N + j] : boundary_value;
                double bottom = (i < M - 1) ? grid[(i + 1) * N + j] : boundary_value;
                double left = (j > 0) ? grid[i * N + j - 1] : boundary_value;
                double right = (j < N - 1) ? grid[i * N + j + 1] : boundary_value;

                // Calculate new value
                scratch[i][j] = (top + bottom + left + right) / 4.0;
            }
        }

        jia_barrier();

        // Update grid
        for (int i = begin; i < end; i++) {
            for (int j = 0; j < N; j++) {
                grid[i * N + j] = scratch[i][j];
            }
        }    

        jia_barrier();
    }

    if (jiapid == 0) {
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < N; j++) {
                printf("grid[%d][%d] = %f\n", i, j, grid[i * N + j]);
            }
        }
    }
    jia_exit();
}