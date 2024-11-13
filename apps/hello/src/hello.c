#include <jia.h>
#include <stdio.h>

int main(int argc, char **argv) {
    jia_init(argc, argv);
    if (jiapid == 0) {
        printf("Hello from master, Number of processes is: %d\n", jiahosts);
    }
    jia_exit();
}