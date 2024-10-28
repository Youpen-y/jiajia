#if !defined(THREAD_H)
#define THREAD_H

#include <pthread.h>

/**
 * @brief client_thread - client thread that move msg from msgbuffer to outqueue

 * 
 * @return void* 
 */
void *client_thread(void *);

// /**
//  * @brief move_msg_to_outqueue - move msg from msgbuffer to outqueue
//  * 
//  * @return int 
//  */
// int move_msg_to_outqueue();


/**
 * @brief server_thread - server thread that handles msg in inqueue
 * 
 * @param args inqueue
 * @return void* 
 */
void *server_thread(void *args);


extern pthread_t client_tid;
extern pthread_t server_tid;

#endif // THREAD_H