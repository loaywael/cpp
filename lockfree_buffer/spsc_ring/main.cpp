#include <iostream>
#include <thread>
#include <chrono>
#include <string>

#include "utils.hpp"


const int NUM_ITEMS = 5'000'000; // 5 million items to pass through the queue
const size_t CAPACITY = 1024;


// benchmark producer
template <typename Buffer>
void benchmark_producer(Buffer& buffer) {
    for (int i = 0; i < NUM_ITEMS; ++i) {
        // Spin-wait: Brutally hammer the memory bus until push succeeds
        while (!buffer.push(i)) {}
    }
}

// benchmark consumer
template <typename Buffer>
void benchmark_consumer(Buffer& buffer) {
    for (int i = 0; i < NUM_ITEMS; ++i) {
        std::optional<int> val;
        // Spin-wait: Brutally hammer the memory bus until pop succeeds
        while (!(val = buffer.pop())) {}
    }
}

// benchmark runner
template <typename Buffer>
void run_benchmark(const std::string& name) {
    Buffer buffer(CAPACITY);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::thread prod(benchmark_producer<Buffer>, std::ref(buffer));
    std::thread cons(benchmark_consumer<Buffer>, std::ref(buffer));
    
    prod.join();
    cons.join();
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    
    std::cout << name << ": " << elapsed.count() << " ms\n";
}

// main function
int main() {
    std::cout << "Starting benchmarks with " << NUM_ITEMS << " items...\n";
    std::cout << "Queue capacity: " << CAPACITY << " elements\n";
    std::cout << "--------------------------------------------------\n";
    
    run_benchmark<MutexQueue<int>>("1. Naive Mutex Queue       ");
    run_benchmark<SeqCstRingBuffer<int>>("2. Seq_cst Ring Buffer     ");
    run_benchmark<SPSCRingBuffer<int>>("3. Acquire/Release (utils) ");
    
    std::cout << "--------------------------------------------------\n";
    std::cout << "Done.\n";
    
    return 0;
}