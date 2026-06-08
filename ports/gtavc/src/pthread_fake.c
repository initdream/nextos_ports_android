/* pthread_fake.c — bridge pthread bionic(armhf) -> glibc, p/ o so-loader.
 *
 * Problema de ABI (ARM 32-bit): o bionic guarda mutex/cond/sem em 4 bytes,
 * mas a glibc usa 24/48/16 bytes. Se a glibc escrever no slot de 4 bytes do
 * jogo, corrompe memória. Solução: o slot de 4 bytes do jogo guarda um
 * PONTEIRO p/ um objeto glibc que a gente aloca (lazy-init cobre os
 * PTHREAD_*_INITIALIZER estáticos, que vêm zerados).
 *
 * pthread_t / pthread_key_t / pthread_once_t são 4 bytes em ambos no armhf,
 * então pthread_create/key/once mapeiam ~direto (attr ignorado).
 *
 * Baseado na lógica do gtactw_vita (que usa primitivas Vita); aqui = glibc.
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ---------- mutex ---------- */
static pthread_mutex_t *mtx_get(void **slot) {
  if (!*slot) {
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_t *m = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(m, &a);
    pthread_mutexattr_destroy(&a);
    *slot = m;
  }
  return (pthread_mutex_t *)*slot;
}
int pthread_mutex_init_fake(void **slot, const void *attr) {
  (void)attr; *slot = NULL; mtx_get(slot); return 0;
}
int pthread_mutex_destroy_fake(void **slot) {
  if (*slot) { pthread_mutex_destroy((pthread_mutex_t *)*slot); free(*slot); *slot = NULL; }
  return 0;
}
int pthread_mutex_lock_fake(void **slot) { return pthread_mutex_lock(mtx_get(slot)); }
int pthread_mutex_unlock_fake(void **slot) {
  if (!*slot) return 0; return pthread_mutex_unlock((pthread_mutex_t *)*slot);
}
int pthread_mutex_trylock_fake(void **slot) { return pthread_mutex_trylock(mtx_get(slot)); }

/* ---------- cond ---------- */
static pthread_cond_t *cond_get(void **slot) {
  if (!*slot) {
    pthread_cond_t *c = malloc(sizeof(pthread_cond_t));
    pthread_cond_init(c, NULL);
    *slot = c;
  }
  return (pthread_cond_t *)*slot;
}
int pthread_cond_init_fake(void **slot, const void *attr) {
  (void)attr; *slot = NULL; cond_get(slot); return 0;
}
int pthread_cond_destroy_fake(void **slot) {
  if (*slot) { pthread_cond_destroy((pthread_cond_t *)*slot); free(*slot); *slot = NULL; }
  return 0;
}
int pthread_cond_signal_fake(void **slot) { return pthread_cond_signal(cond_get(slot)); }
int pthread_cond_broadcast_fake(void **slot) { return pthread_cond_broadcast(cond_get(slot)); }
int pthread_cond_wait_fake(void **cslot, void **mslot) {
  return pthread_cond_wait(cond_get(cslot), mtx_get(mslot));
}
int pthread_cond_timedwait_fake(void **cslot, void **mslot, const struct timespec *ts) {
  return pthread_cond_timedwait(cond_get(cslot), mtx_get(mslot), ts);
}

/* ---------- sem ---------- */
static sem_t *sem_get(void **slot) {
  if (!*slot) { sem_t *s = malloc(sizeof(sem_t)); sem_init(s, 0, 0); *slot = s; }
  return (sem_t *)*slot;
}
int sem_init_fake(void **slot, int pshared, unsigned value) {
  (void)pshared; sem_t *s = malloc(sizeof(sem_t)); sem_init(s, 0, value); *slot = s; return 0;
}
int sem_destroy_fake(void **slot) {
  if (*slot) { sem_destroy((sem_t *)*slot); free(*slot); *slot = NULL; } return 0;
}
int sem_post_fake(void **slot) { return sem_post(sem_get(slot)); }
int sem_wait_fake(void **slot) { return sem_wait(sem_get(slot)); }
int sem_trywait_fake(void **slot) { return sem_trywait(sem_get(slot)); }

/* ---------- create / attr (mapeiam ~direto; attr bionic ignorado) ---------- */
int pthread_create_fake(pthread_t *t, const void *attr, void *(*start)(void *), void *arg) {
  (void)attr;
  return pthread_create(t, NULL, start, arg);
}
int pthread_attr_init_fake(void *a) { (void)a; return 0; }
int pthread_attr_destroy_fake(void *a) { (void)a; return 0; }
int pthread_attr_setdetachstate_fake(void *a, int s) { (void)a; (void)s; return 0; }
int pthread_attr_setstacksize_fake(void *a, size_t s) { (void)a; (void)s; return 0; }
int pthread_attr_setschedparam_fake(void *a, const void *p) { (void)a; (void)p; return 0; }
int pthread_setschedparam_fake(pthread_t t, int p, const void *s) { (void)t; (void)p; (void)s; return 0; }
int pthread_setname_np_fake(pthread_t t, const char *n) { (void)t; (void)n; return 0; }

/* bionic: pthread_cond_timeout_np(cond, mutex, ms) — espera relativa em ms */
#include <time.h>
int pthread_cond_timeout_np_fake(void **cslot, void **mslot, unsigned ms) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += ms / 1000;
  ts.tv_nsec += (long)(ms % 1000) * 1000000L;
  if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
  return pthread_cond_timedwait(cond_get(cslot), mtx_get(mslot), &ts);
}
