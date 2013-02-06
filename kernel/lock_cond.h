/*
 * created by Jaakko
 */

#ifndef LOCK_COND_H
#define LOCK_COND_H

#ifdef CHANGED_1

typedef struct lock_s lock_t;

void lock_init(void);

lock_t *lock_create(void);
void lock_destroy(lock_t *lock);
void lock_acquire(lock_t *lock);
void lock_release(lock_t *lock);

typedef struct cond_s cond_t;

void cond_init(void);

cond_t *condition_create(void);
void condition_destroy(cond_t *cond);
void condition_wait(cond_t *cond, lock_t *condition_lock);
void condition_signal(cond_t *cond, lock_t *condition_lock);
void condition_broadcast(cond_t *cond, lock_t *condition_lock);

#endif /* CHANGED_1 */

#endif /* LOCK_COND_H */
