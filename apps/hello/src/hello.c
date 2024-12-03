#include <jia.h>
#include <stdio.h>

int main(int argc, char **argv) {
    jia_init(argc, argv);
    if (jiapid == 0) {
        printf("Hello from master, Number of processes is: %d\n", jiahosts);
    }
    int *arr = (int *)jia_alloc(2000*sizeof(int));
    if (jiapid == 0){
        for(int i = 0; i < 2000; ++i) {
            arr[i] = 0;
        }
    }

    jia_lock(0);
    for(int i = 0; i < 2000; i++) {
        arr[i]++;
    }
    jia_unlock(0);

    jia_barrier();

    if (jiapid == 0) {
        for(int i = 0; i < 2000; i++) {
            printf("arr[%d] = %d\n", i, arr[i]);
        }
    }

    jia_exit();
}