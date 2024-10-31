#if !defined(THREAD_H)
#define THREAD_H

#include "msg.h"
#include <pthread.h>
#include <stdbool.h>

/**
 * @brief client_thread - client thread that move msg from msgbuffer to outqueue

 * 
 * @return void* 
 */
void *client_thread(void *);



/**
 * @brief outsend - send msg to destination host
 *
 * @param msg msg to send
 * @return int
 */
int outsend(jia_msg_t *msg);


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
 * @brief msg_handle - handle msg
 * 
 * @param msg 
 * @note msg_handle called by server_thread
 */
void msg_handle(jia_msg_t *msg);

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
 * @param trigger_mode trigger mode, 1 for edge trigger, 0 for level trigger
 */
void addfd(int epollfd, int fd, int trigger_mode);

extern pthread_t client_tid;
extern pthread_t server_tid;
extern pthread_t listen_tid;

#endif // THREAD_H