### Next generation JIAJIA (SDSM) --- JIAJIA3.0
JIAJIA is a software distributed shared memory system that written at the beginning of the century. What makes it unique is its lock-based consistency.

As a static library, it provide usable interfaces to programmer so that they can use it divide their tasks to run on multiple machines. [Interfaces](#jiajia30-interfaces)

> This project is implemented and maintained by [@Youpen-y](https://github.com/Youpen-y) and [@segzix](https://github.com/segzix).

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
 * @lockid: the lock id
 */

void jia_lock(int lockid);
```

```c
/**
 * jia_unlock -- release the lock indentified by lockid
 * @lockid: the lock id
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
