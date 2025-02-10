### Next generation JIAJIA (SDSM) --- JIAJIA3.0
JIAJIA is a software distributed shared memory system that written at the beginning of the century. What makes it unique is its lock-based consistency.

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
- Upgrade it so that it can be adapted to 64-bit machines
- Redesign some interfaces to increase usability (eg. replace signal-driven IO with event-driven IO epoll in linux system; jia_falloc alloc memory with control flag)
- Optimize cache design to support LRU
- Prefetch pages to reduce SIGSEGV triggers
- Memory pool support to provide finer-grained control on memory allocation(Reconsidering)
- Adopt RDMA technology to redesign the whole system desing (two directions)
  - RDMA support as a extra function that can be truned on or off. It means that there are two side-by-side network protocol stacks in the system, and will use RDMA first if possible.
  - Pure RDMA (pursue extreme performance)
- The last one need to consider is its availability (fault tolerance), consider checkpoint/restore(store it's memory content to persistent storage periodically)

### JIAJIA3.0 Interfaces
```c
/**
 * @brief Initialize JIAJIA3.0
 * copy the application to hosts specified in .jiahosts file; 
 * initializes internal data structures of JIAJIA3.0
*/
void jia_init(int argc, char **argv);

/**
 * @brief Exit JIAJIA3.0
 */
void jia_exit();
```
`jia_init()` should be called before any other JIAJIA3.0 functions. \
`jia_exit()` should be called at the end of your program.
```c
/**
 * @brief Allocate size bytes memory across the hosts
 */
unsigned long jia_alloc(size_t size);

```
`jia_alloc()` will be expanded to be more feature-rich and intelligent.

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

### Usage
1. **ssh config**
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