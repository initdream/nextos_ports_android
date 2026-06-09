#ifndef PTHREAD_BRIDGE_H
#define PTHREAD_BRIDGE_H
int b_mutex_init(void *, void *); int b_mutex_destroy(void *);
int b_mutex_lock(void *); int b_mutex_unlock(void *); int b_mutex_trylock(void *);
int b_cond_init(void *, void *); int b_cond_destroy(void *);
int b_cond_wait(void *, void *); int b_cond_timedwait(void *, void *, void *);
int b_cond_signal(void *); int b_cond_broadcast(void *);
int b_mutexattr_init(void *); int b_mutexattr_destroy(void *); int b_mutexattr_settype(void *, int);
int b_once(void *, void (*)(void));
int b_rwlock_rdlock(void *); int b_rwlock_wrlock(void *); int b_rwlock_unlock(void *);
#endif
