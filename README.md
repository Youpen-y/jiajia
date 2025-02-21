### Next generation JIAJIA (SDSM) --- M-JIAJIA
M-JIAJIA is a software distributed shared memory(SDSM) system built upon JIAJIA SDSM. It inherits most of the features of JIAJIA, such as lock-based cache coherence protocol(which is an implementation of scope consistency model) to maintain the data consistency of the different copies spread on the different hosts.

"M" of M-JIAJIA means multithread and dual communication stack(udp and RDMA)

As a static library, it provide usable interfaces to programmer so that they can use it divide their tasks to run on multiple machines. [Interfaces](#jiajia30-interfaces)

> This project is implemented and maintained by [@Youpen-y](https://github.com/Youpen-y) and [@segzix](https://github.com/segzix).

### Dependency
- `libibverbs`
- `librdmacm`

On ubuntu
```bash
sudo apt install libibverbs-dev
sudo apt install librdmacm-dev
```

### Target
- **Easy to use** `Apps/` has the application template to mock.

- **Customizable** Supports dynamic adjustment of system configurations in `.jiaconf` file.

- **Portable** No need to install too much dependency, such as MPI or UCX.

### M-JIAJIA Interfaces

#### Init and Exit -- sytem initialization and end termination
```c
/**
 * @brief Initialize M-JIAJIA
 * copy the application to hosts specified in .jiahosts file; 
 * initializes internal data structures of JIAJIA3.0
*/
void jia_init(int argc, char **argv);

/**
 * @brief Exit M-JIAJIA
 */
void jia_exit();
```
#### Alloc -- alloc shared memory

```c
/**
 * @brief jia_alloc -- allocates all totalsize bytes shared memory on starthost, 
 * but the starthost depends on the times that the function have been called.
 * 
 * @param totalsize sum of size that allocated for shared memory
 * @return start address of the allocated memory
 */
unsigned long  jia_alloc(int totalsize);

/**
 * @brief jia_alloc_random -- choose a host randomly, and allocate shared
 * memory on it
 * 
 * @param totalsize sum of size that allocated for shared memory
 * @return start address of the allocated memory
 */
unsigned long jia_alloc_random(int totalsize);

/**
 * @brief jia_alloc_array -- allocate array[i] bytes shared memory on host i 
 * 
 * @param totalsize sum of size that allocated for shared memory
 * @param array blocksize array that will allocated on every host
 * @param n size of array, range: (0, jiahosts)
 * @return start address of the allocated memory
 */
unsigned long jia_alloc_array(int totalsize, int *array, int n);

/**
 * @brief jia_alloc2 -- alloc totalsize shared memory on cluster with a 
 * circular allocation strategy (per blocksize)
 * 
 * @param totalsize sum of size that allocated for shared memory
 * @param blocksize size(page aligned) that allocated by every host every time
 * @return start address of the allocated memory 
 */
unsigned long  jia_alloc2(int totalsize, int blocksize);

/**
 * @brief jia_alloc2p -- alloc size(page aligned) on
 * host(proc is jiapid), two parameters alloc
 *
 * @param totalsize sum of requested shared memory
 * @param starthost host that will allocate shared memory
 * @return start address of the allocated memory
 */
unsigned long  jia_alloc2p(int totalsize, int starthost);

/**
 * @brief jia_alloc3 -- allocates totalsize bytes cyclically across all hosts, each
 * time block bytes (3 parameters alloc function), start from starthost.
 *
 * @param totalsize sum of space that allocated across all hosts (page aligned)
 * @param blocksize size(page aligned) that allocated by every host every time
 * @param starthost specifies the host from which the allocation starts
 * @return start address of the allocated memory
 *
 * jia_alloc3 will allocate blocksize(page aligned) every time from starthost to
 * other hosts until the sum of allocated sizes equal totalsize(page aligned)
 */
unsigned long jia_alloc3(int totalsize,int blocksize, int starthost);

/**
 * @brief jia_alloc4 -- alloc totalsize bytes shared memory with blocks array
 *
 * @param totalsize sum of space that allocated across all hosts (page aligned)
 * @param blocks    blocksize array, blocks[i] specify how many bytes 
 *                  will be allocated on every host in loop i.
 * @param n 		length of blocks
 * @param starthost specifies the host from which the allocation starts
 * @return start address of the allocated memory
 */
unsigned long jia_alloc4(int totalsize, int *blocks, int n, int starthost);
```



#### Sync -- lock and barrier

```c
/**
 * jia_lock -- acquire the lock indentified by lockid
 * @lockid: the lock id, range: [0:63]
 */

void jia_lock(int lockid);
```

```c
/**
 * jia_unlock -- release the lock indentified by lockid
 * @lockid: the lock id, range: [0,63]
 */
void jia_unlock(int lockid);
```

```c
/**
 * jia_barrier -- perform a global barrier
 */
void jia_barrier();
```
Note: `jia_barrier()` cannot be called inside a critical section enclosed by `jia_lock()` and `jia_unlock()`

#### Others
```c
/**
 * @brief jia_clock() - calculate the elapsed time since program started
 *
 * @return double: time(us) elapsed since program started
 */
double          jia_clock();
```

### Usage
1. **ssh configuration**
Refer to this tutorials [SSH Essentials: Working with SSH Servers, Clients, and Keys](https://www.digitalocean.com/community/tutorials/ssh-essentials-working-with-ssh-servers-clients-and-keys#allowing-root-access-for-specific-commands)
This step aims to help password-free operation.

2. **create your apps**
    - Use JIA api in your program with `#include <jia.h>`.
    - Two common macro `jiahosts`(the num of machines in .jiahosts) and `jiapid`(the process id on current machine) can be used directly.

Example:
```c
#include <jia.h>
#include <stdio.h>

int main(int argc, char **argv) {
    jia_init(argc, argv); // init system
    if (jiapid == 0) {
        printf("Hello from master, Number of processes is: %d\n", jiahosts);
    }
    int *arr = (int *)jia_alloc(200000*sizeof(int));
    if (jiapid == 0){
        for(int i = 0; i < 200000; ++i) {
            arr[i] = 0;
        }
    }

    jia_lock(0);    // critical section
    for(int i = 0; i < 200000; i++) {
        arr[i]++;
    }
    jia_unlock(0);

    jia_barrier();  // memory barrier

    if (jiapid == 0) {
        for(int i = 0; i < 200; i++) {
            printf("arr[%d] = %d\n", i, arr[i]);
        }
    }

    jia_exit(); // exit system
}
```

3. **Modify .jiahost file**
Add machines that you want to add to the system.

Example:
```
# comment line
192.168.103.1 username password
192.168.103.2 username password
# ...
```

4. **Optional**: Makefile (Refer to other app structure)


### Test
`run.sh` is a test script that used to test all cases.

```
bash run.sh
```