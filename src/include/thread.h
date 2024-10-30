#include <stdbool.h>
#if !defined(THREAD_H)
#define THREAD_H

#include <pthread.h>

/**
 * @brief client_thread - client thread that move msg from msgbuffer to outqueue

 * 
 * @return void* 
 */
void *client_thread(void *);





/**
 * @brief client_listen - thread that listens on ack_fds and waits for ack
 * 
 * @return void* 
 */
void *client_listen(void *);






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


/**
 * @brief listen_thread - listen thread that listening on recv_fds

 * 
 * @param args 
 * @return void* 
 */
void *listen_thread(void *args);


/**
 * @brief addfd - add fd to epollfd instance

 * 
 * @param epollfd epollfd instance
 * @param fd fd to add
 * @param one_shot EPOLLONESHOT or not
 * @param trigger_mode trigger mode, 1 for edge trigger, 0 for level trigger
 */
void addfd(int epollfd, int fd, bool one_shot, int trigger_mode);

extern pthread_t client_tid;
extern pthread_t server_tid;

#endif // THREAD_H