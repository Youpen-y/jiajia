#if !defined(THREAD_H)
#define THREAD_H

#include <pthread.h>

void *client_thread(void *);
void *server_thread(void *);


extern pthread_t client_tid;
extern pthread_t server_tid;

#endif // THREAD_H