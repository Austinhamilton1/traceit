#ifndef MEMRACE_H
#define MEMRACE_H

#include <stdint.h>
#include <pthread.h>

#define MAX_CLOCK_SIZE 64
#define TID(epoch) ((uint8_t)((epoch) >> 56))
#define GET_CLOCK(epoch) ((epoch) & 0x00FFFFFFFFFFFFFFUL)
#define MAKE_EPOCH(tid, c) ((((uint64_t)(tid)) << 56) | (c))
#define EMPTY_EPOCH MAKE_EPOCH(0, 1)
#define READ_SHARED 0

struct thread_state {
    uint8_t tid;                    // Thread ID
    uint64_t C[MAX_CLOCK_SIZE];     // Thread's local vector clock
    uint64_t epoch;                 // Cache the current epoch (same as C[tid])
};

struct mem_state {
    pthread_mutex_t lock;           // Used to allow concurrent access to variable metadata
    uint64_t W;                     // Write epoch
    uint64_t R;                     // Read epoch
    uint64_t Rvc[MAX_CLOCK_SIZE];   // Read vector clock (only used on a shared memory read)
    void *mem;                      // Memory location that the this state refers to
};

struct lock_state {
    uint64_t L[MAX_CLOCK_SIZE];     // Lock's local vector clock
    void *mem;                     // Memory location of the lock this state refers to
};

// Detect races on either a read or a write to a variable
void mem_read(struct mem_state *x, struct thread_state *t);
void mem_write(struct mem_state *x, struct thread_state *t);

// Update states on synchronizations
void lock_acq(struct lock_state *l, struct thread_state *t);
void lock_rel(struct lock_state *l, struct thread_state *t);

// Updates states on thread creation/joining
void thread_fork(struct thread_state *t, struct thread_state *u);
void thread_join(struct thread_state *t, struct thread_state *u);

// Initialize states for state types
void init_thread_state(struct thread_state *t);
void init_mem_state(struct mem_state *x, void *mem);
void init_lock_state(struct lock_state *l, pthread_mutex_t *lock);

#endif