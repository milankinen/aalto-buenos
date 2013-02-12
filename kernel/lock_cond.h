/*
 * created by Jaakko
 */

#ifndef LOCK_COND_H
#define LOCK_COND_H

#ifdef CHANGED_1


#define LOCK_NO_THREAD -1
#define LOCK_RESERVED_THREAD -2

/* data structures for lock_t and cond_t */

typedef struct {
    spinlock_t slock;
    TID_t locked_id;
    uint32_t nested_locking_count;
    uint32_t waiting_thread_count;
    int8_t is_used;
} lock_t;

typedef struct {
    spinlock_t slock;
    uint32_t waiting_thread_count;
    int8_t is_used;
} cond_t;


void lock_table_init(void);

lock_t *lock_create(void);
void lock_destroy(lock_t *lock);
void lock_acquire(lock_t *lock);
void lock_release(lock_t *lock);


void cond_table_init(void);

cond_t *condition_create(void);
void condition_destroy(cond_t *cond);
void condition_wait(cond_t *cond, lock_t *condition_lock);
void condition_signal(cond_t *cond, lock_t *condition_lock);
void condition_broadcast(cond_t *cond, lock_t *condition_lock);

#endif /* CHANGED_1 */

#endif /* LOCK_COND_H */
