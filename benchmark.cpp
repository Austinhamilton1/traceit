#include <thread>
#include <vector>
#include <iostream>

#include "memrace.h"

#define UPDATE_COUNT 10000

int main(int argc, char *argv[]) {
    const int thread_count = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    size_t counter = 0;
    mem_init(&counter);

    for(int i = 0; i < thread_count; i++) {
        threads.emplace_back([&counter]() {
            for(int j = 0; j < UPDATE_COUNT; j++) {
                size_t val = read<size_t>(&counter);
                write<size_t>(&counter, val + 1);
            }
        });
    }

    for(int i = 0; i < thread_count; i++) {
        threads[i].join();
    }

    size_t race_count = get_shadow_mem(&counter).race_count;

    std::cout << "Expected value of counter: " << thread_count * UPDATE_COUNT << std::endl;
    std::cout << "Actual value of counter: " << counter << std::endl;
    std::cout << "Number of potential races on counter: " << race_count << std::endl;

    return 0;
}