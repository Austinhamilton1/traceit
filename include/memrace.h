#pragma once

#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>

/* Keeps track of each thread's global time */
typedef struct {
    std::mutex mMutex;
    std::unordered_map<std::thread::id, size_t> vc;
} vector_clock_t;

/* Each memory location has access to one of these */
struct shadow_mem {
    std::mutex mMutex;
    std::thread::id TID;
    bool was_write;
    vector_clock_t last_access;
    size_t race_count;
};

/* Helper function to determine if two accesses are concurrent or not */
bool is_concurrent(vector_clock_t& access_1, vector_clock_t& access_2);

/* Determine if a new access is a race on a memory */
bool is_race(struct shadow_mem& old_state, vector_clock_t& new_clock, bool is_write);

/* Helper function to get the current thread's vector clock */
vector_clock_t& get_vector_clock(std::thread::id);

/* Helper function to get/set last acccess state of memory */
struct shadow_mem& get_shadow_mem(void *mem);

/*
 * Read a memory location.
 * Arguments:
 *     T *value - Read from this location.
 * Returns:
 *     T - The value in the memory location.
 */
template<typename T> T read(T *value) {
    // Get the shadow memory associated with the memory location
    void *mem = static_cast<void *>(value);
    struct shadow_mem& mem_state = get_shadow_mem(mem);

    // Grab the current thread clock and lock the structures down
    vector_clock_t& clock = get_vector_clock(std::this_thread::get_id());
    mem_state.mMutex.lock();
    clock.mMutex.lock();

    // Update vector clock
    clock.vc[std::this_thread::get_id()]++;

    // If there is a race, update the race counter
    if(is_race(mem_state, clock, false))
        mem_state.race_count++;
    
    mem_state.TID = std::this_thread::get_id();
    mem_state.was_write = false;
    mem_state.last_access.vc = clock.vc;

    // Unlock the data structures
    T v = *value;
    clock.mMutex.unlock();
    mem_state.mMutex.unlock();

    return v;
}

/*
 * Write to a memory location.
 * Arguments:
 *     T *dest - Write to this memory location.
 *     T value - Write this value to the memory.
 */
template<typename T> void write(T *dest, T value) {
    // Get the shadow memory associated with the memory location
    void *mem = static_cast<void *>(dest);
    struct shadow_mem& mem_state = get_shadow_mem(mem);

    // Grab the current thread clock and lock the structures down
    vector_clock_t& clock = get_vector_clock(std::this_thread::get_id());
    mem_state.mMutex.lock();
    clock.mMutex.lock();

    // Update vector clock
    clock.vc[std::this_thread::get_id()]++;

    // If there is a race, update the race counter
    if(is_race(mem_state, clock, true))
        mem_state.race_count++;

    mem_state.TID = std::this_thread::get_id();
    mem_state.was_write = true;
    mem_state.last_access.vc = clock.vc;

    // Unlock the data structures
    *dest = value;
    clock.mMutex.unlock();
    mem_state.mMutex.unlock();
}