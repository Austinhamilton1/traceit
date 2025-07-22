#include "memrace.h"
#include <algorithm>

static thread_local vector_clock_t vector_clock;                // Clock of the current thread
static std::unordered_map<void *, shadow_mem> history;   // Mapping of shadow memory

/*
 * Determine if two accesses are concurrent or not.
 * Arguments:
 *     vector_clock_t& access_1 - First access.
 *     vector_clock_t& access_2 - Second access.
 * Returns:
 *     bool- true if concurrent, false otherwise.
 */
bool is_concurrent(vector_clock_t& access_1, vector_clock_t& access_2) {
    // The happened before operator determines if all values in one thread's vector clock
    // are less than or equal to those in the second thread's vector clock.
    auto happened_before = [](vector_clock_t& event1, vector_clock_t& event2) {
        return std::all_of(
            event1.vc.begin(), event1.vc.end(),
            [&event2] (const std::pair<std::thread::id, size_t>& t1) {
                return t1.second <= event2.vc[t1.first];
            }
        );
    };

    // If one thread happened before the other, then this is not concurrent, otherwise it is
    if(happened_before(access_1, access_2) || happened_before(access_2, access_1)) return false;
    return true;
}

/*
 * Determine if a new access is a potential data race.
 * Arguments:
 *     shadow_mem& old_access - The shadow memory associated with the access.
 *     vector_clock_t& new_clock - The current thread's clock.
 *     bool is_write - Is the new access a write?
 * Returns:
 *     bool - true if race, false otherwise.
 */
bool is_race(shadow_mem& old_access, vector_clock_t& new_clock, bool is_write) {
    // Condition 1 - Two accesses must be different threads
    if(old_access.TID == std::this_thread::get_id()) return false;

    // Condition 2 - At least one access must be write
    if(!old_access.was_write && !is_write) return false;

    // Condition 3 - Accesses must be concurrent
    if(!is_concurrent(old_access.last_access, new_clock)) return false;

    // All conditions met, most likely a race
    return true;
}

/*
 * Get the vector clock associated with the current thread.
 * Returns:
 *     vector_clock_t& - A reference to the current thread's vector clock.
 */
vector_clock_t& get_vector_clock() {
    return vector_clock;
}

/*
 * Get the shadow mem associated with a memory location.
 * Arguments:
 *     void *mem - The memory to look up.
 * Returns:
 *     shadow_mem& - The associated shadow memory.
 */
shadow_mem& get_shadow_mem(void *mem) {
    return history[mem];
}

/*
 * Initialize a memory in the history.
 * Arguments:
 *     void *mem - The memory to initialize.
 */
void mem_init(void *mem) {
     auto [it, inserted] = history.emplace(std::piecewise_construct,
                                          std::forward_as_tuple(mem),
                                          std::forward_as_tuple());
    if (inserted) {
        it->second.TID = std::this_thread::get_id();
        it->second.was_write = false;
        it->second.race_count = 0;
    }
}