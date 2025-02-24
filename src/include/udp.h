#include "global.h"
#if !defined(THREAD_H)
#define THREAD_H

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

/**
 * @brief initcomm -- initialize udp communication
 *
 */
void init_udp_comm();

/* communication manager */
typedef struct comm_manager {
    int         snd_fds;  // send file descriptor
    unsigned    snd_seq[Maxhosts];  // sequence number that used to acknowledge
    unsigned short snd_server_port; // snd server port is destination host's port

    int ack_fds;
    unsigned ack_seq[Maxhosts];
    unsigned short ack_port;

    int         rcv_fds[Maxhosts];  // read file descriptor
    unsigned    rcv_seq[Maxhosts];  // sequence number
    unsigned short rcv_ports[Maxhosts];

    msg_queue_t* outqueue;
    msg_queue_t* inqueue;
} comm_manager_t;

extern comm_manager_t comm_manager;

extern pthread_t client_tid;
extern pthread_t server_tid;
extern pthread_t listen_tid;

#endif // THREAD_H