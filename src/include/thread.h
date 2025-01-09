#if !defined(THREAD_H)
#define THREAD_H
#include "global.h"
#include "msg.h"
#include <pthread.h>
#include <stdbool.h>

/**
 * @brief client_thread - client thread that move msg from msgbuffer to outqueue
 * 
 * @param args outqueue
 * @return void* 
 */
void *client_thread(void *args);

/**
 * @brief server_thread - server thread that handles msg in inqueue
 * 
 * @param args inqueue
 * @return void* 
 */
void *server_thread(void *args);

/**
 * @brief listen_thread - listen thread that listening on recv_fds

 * 
 * @param args NULL
 * @return void* 
 */
void *listen_thread(void *args);

extern pthread_t client_tid;
extern pthread_t server_tid[Maxhosts];
extern pthread_t listen_tid;

// inqueue[i] args that will delivere to server thread[i]
// multiple inqueues, to store remote host's msg
typedef struct inqueue_arg {
    void *addr; // inqueue[i]'s addr
    int id;      // id is the inqueue corresponding to the remote host
} inqueue_arg_t;

#endif // THREAD_H