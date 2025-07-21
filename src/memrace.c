#include "memrace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Merge two vector clocks into one vector clock.
 * Arguments:
 *     uint64_t *a - The vector clock to merge the two into.
 *     uint64_t *b - Merge this into a.
 */
static inline void merge(uint64_t *a, uint64_t *b) {
    for(int i = 0; i < MAX_CLOCK_SIZE; i++) {
        if(a[i] < b[i])
            a[i] = b[i];
    }
}

/*
 * Determines if there is a race on a read operation.
 * Arguments:
 *     struct mem_state *X - State associated with the variable being accessed.
 *     struct thread_state *t - State associated with the reader thread.
 */
void mem_read(struct mem_state *x, struct thread_state *t) {
    // Lock down mem state to ensure synchronization
    pthread_mutex_lock(&x->lock);
    
    if(x->R == t->epoch) { // Same epoch (same thread has read twice, no race possible)
        pthread_mutex_unlock(&x->lock);
        return;
    }

    uint8_t tid;

    // Write-read race? (ensure read happened after last write)
    tid = TID(x->W);
    if(GET_CLOCK(x->W) > t->C[tid]) {
        printf("write-read detected.\n");
    }

    // Update read state
    if(x->R == READ_SHARED) {
        /* Apply rule [FT Read Shared] when R_x is a vector clock and the read happened after last write */
        /* Update the component of the vector clock associated with current thread to last epoch */
        x->Rvc[t->tid] = GET_CLOCK(t->epoch); // Shared
    } else {
        tid = TID(x->R);
        if(GET_CLOCK(x->R) <= t->C[tid]) { // Exclusive
            /* Apply rule [FT Read Exclusive] when R_x is an epoch and the last read happened before the current epoch */
            /* Update the read to the latest epoch */
            x->R = t->epoch;
        } else { // Share
            /* Apply rule [FT Read Share] when there hasn't been an epoch (two shared reads) */
            /* Create a new vector clock */
            memset(x->Rvc, 0, sizeof(x->Rvc));
            
            /* Store the epochs of both reads in the variables vector clock */
            x->Rvc[tid] = x->R;
            x->Rvc[t->tid] = GET_CLOCK(t->epoch);
            x->R = READ_SHARED;
        }
    }

    // Mem state can now be used again
    pthread_mutex_unlock(&x->lock);
}

/*
 * Determines if there is a race on a write operation.
 * Arguments:
 *     struct mem_state *x - State associated with the variable being accessed.
 *     struct thread_state *t - State associated with the writer thread.
 */
void mem_write(struct mem_state *x, struct thread_state *t) {
    // Lock down mem state to ensure synchronization
    pthread_mutex_lock(&x->lock);

    if(x->W == t->epoch) { // Same epoch (Same thread has written twice, no race possible)
        pthread_mutex_unlock(&x->lock);
        return;
    }

    uint8_t tid;

    // Write-write race? (ensure write happened after last write)
    tid = TID(x->W);
    if(GET_CLOCK(x->W) > t->C[tid]) {
        printf("write-write detected.\n");
    }

    tid = TID(x->R);
    // Read-write race? (ensure read happened after last write)
    if(x->R != READ_SHARED) { // Exclusive
        if(GET_CLOCK(x->R) > t->C[tid]) {
            /* Apply rule [FT Write Exclusive] when R_x is an epoch and the last read happened after the current write */
            printf("read-write detected.\n");
        }
    } else { // Shared
        for(int i = 0; i < MAX_CLOCK_SIZE; i++) {
            if(x->Rvc[i] > t->C[i]) {
                /* Apply rule [FT Write Shared] when R_x is a vector clock and the last read happened after the current write */
                printf("read-write detected.\n");
                break;
            }
        }
        x->R = EMPTY_EPOCH; // Reset read (no shared reads currently being tracked).
    }

    // Update write state
    x->W = t->epoch;

    // Mem state can now be used again
    pthread_mutex_unlock(&x->lock);
}

/*
 * Called when a lock is acquired. Synchronizes thread states.
 * Arguments:
 *     struct lock_state *l - State associated with the lock being acquired.
 *     struct thread_state *t - State associated with the thread acquiring lock.
 */
void lock_acq(struct lock_state *l, struct thread_state *t) {
    /* On acquiring a lock apply rule [FT Acquire] */
    /* Merge the lock's vector clock into the thread's vector clock */
    merge(t->C, l->L);
}

/*
 * Called when a lock is released. Synchronizes thread states.
 * Arguments:
 *     struct lock_state *l - State associated with the lock being released.
 *     struct thread_state *t - State associated with the thread releasing lock.
 */
void lock_rel(struct lock_state *l, struct thread_state *t) {
    /* On releasing a lock apply rule [FT Release] */
    /* Copy the thread's vector clock into the lock's vector clock */
    for(int i = 0; i < MAX_CLOCK_SIZE; i++) {
        l->L[i] = t->C[i];
    }

    /* Increment the thread clock's vector clock */
    t->C[t->tid]++;
    t->epoch = MAKE_EPOCH(t->tid, t->C[t->tid]);
}

/*
 * Called when a new thread is created.
 * Arguments:
 *     struct thread_state *t - The parent thread state.
 *     struct thread_state *u - The child thread state being created.
 */
void thread_fork(struct thread_state *t, struct thread_state *u) {
    /* On forking a new thread apply rule [FC Fork] */
    /* Merge the old thread's vector clock into the new thread's vector clock */
    merge(u->C, t->C);

    /* Increment the old thread's vector clock */
    t->C[t->tid]++;
    t->epoch = MAKE_EPOCH(t->tid, t->C[t->tid]);
}

/*
 * Called when a thread is joined.
 * Arguments:
 *     struct thread_state *t - The parent thread state.
 *     struct thread_state *u - The child thread state being created.
 */
void thread_join(struct thread_state *t, struct thread_state *u) {
    /* On joining a thread, apply rule [FC Join] */
    /* Merge teh new thread's vector clock into the old thread's vector clock */
    merge(t->C, u->C);

    /* Increment the new thread's vector clock */
    u->C[u->tid]++;
    u->epoch = MAKE_EPOCH(u->tid, u->C[u->tid]);
}

static pthread_mutex_t tid_lock = PTHREAD_MUTEX_INITIALIZER;
static uint8_t next_tid = 0;

/*
 * Initialize a thread state.
 * Arguments:
 *     struct thread_state *t - Initialize this thread state.
 */
void init_thread_state(struct thread_state *t) {
    pthread_mutex_lock(&tid_lock);
    t->tid = next_tid++;
    pthread_mutex_unlock(&tid_lock);
    memset(t->C, 0, sizeof(t->C));
    t->epoch = MAKE_EPOCH(t->tid, 1);
}

/*
 * Initialize a memory state.
 * Arguments:
 *     struct mem_state *x - Initialize this memory state.
 *     void *mem - The memory state points to this memory.
 */
void init_mem_state(struct mem_state *x, void *mem) {
    pthread_mutex_init(&x->lock, NULL);
    pthread_mutex_lock(&x->lock);
    x->R = EMPTY_EPOCH;
    x->W = EMPTY_EPOCH;
    memset(x->Rvc, 0, sizeof(x->Rvc));
    x->mem = mem;
    pthread_mutex_unlock(&x->lock);
}

/*
 * Initialize a lock state.
 * Arguments:
 *     struct lock_state *l - Initialize this lock state.
 *     pthread_mutex_t *lock - The lock points to this lock.
 */
void init_lock_state(struct lock_state *l, pthread_mutex_t *lock) {
    memset(l->L, 0, sizeof(l->L));
    l->mem = (void *)lock;
}