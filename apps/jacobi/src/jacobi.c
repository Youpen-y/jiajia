#include <jia.h>
#include <stdio.h>
#include <unistd.h>

#define M 1024
#define N 1024
#define TIMES 10

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
    
    if (jiapid == 0) {
        grid = (float *)jia_alloc(M * N * sizeof(float));
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
                if (i == 0) {
                    // top line
                    scratch[i][j] = (boundary_value + grid[(i + 1) * N + j] + 
                                     (j > 0 ? grid[i * N + j - 1] : boundary_value) + 
                                     (j < N - 1 ? grid[i * N + j + 1] : boundary_value)) / 4;
                } else if (i == M - 1) {
                    // bottom line
                    scratch[i][j] = (grid[(i - 1) * N + j] + boundary_value + 
                                     (j > 0 ? grid[i * N + j - 1] : boundary_value) + 
                                     (j < N - 1 ? grid[i * N + j + 1] : boundary_value)) / 4;
                } else {
                    // internal line
                    scratch[i][j] = (grid[(i - 1) * N + j] + grid[(i + 1) * N + j] + 
                                     (j > 0 ? grid[i * N + j - 1] : boundary_value) + 
                                     (j < N - 1 ? grid[i * N + j + 1] : boundary_value)) / 4;
                }
            }
        }

        jia_barrier();

        // update grid
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
}