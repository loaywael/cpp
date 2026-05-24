#include <atomic>
#include <vector>
#include <optional>
#include <new>
#include <queue>
#include <mutex>


// 1. naive queue, mutex approach
template <typename T>
class MutexQueue {
    std::queue<T> q;
    std::mutex mtx;
    size_t capacity;
public:
    explicit MutexQueue(size_t cap) : capacity(cap) {}

    bool push(const T& value) {
        std::lock_guard<std::mutex> lock(mtx);
        if (q.size() >= capacity) return false; // Queue is full
        q.push(value);
        return true;
    }

    std::optional<T> pop() {
        std::lock_guard<std::mutex> lock(mtx);
        if (q.empty()) return std::nullopt;     // Queue is empty
        T value = q.front();
        q.pop();
        return value;
    }
};


// 2. optimized ring buffer, seq_cst approach
template <typename T>
class SeqCstRingBuffer {
private:
    std::vector<T> buffer;
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
    size_t capacity;
    
public:
    explicit SeqCstRingBuffer(size_t cap)
        : capacity(cap), buffer(cap), head(0), tail(0) {}

    bool push(const T& value) {
        // Using seq_cst (Sequential Consistency) - The heavy "stop the world" barrier
        size_t current_head = head.load(std::memory_order_seq_cst);
        size_t next_head = (current_head + 1) % capacity;
        if (next_head == tail.load(std::memory_order_seq_cst)) {
            return false;
        }
        buffer[current_head] = value;
        head.store(next_head, std::memory_order_seq_cst);
        return true;
    }

    std::optional<T> pop() {
        // Using seq_cst
        size_t current_tail = tail.load(std::memory_order_seq_cst);
        if (current_tail == head.load(std::memory_order_seq_cst)) {
            return std::nullopt;
        }
        T value = buffer[current_tail];
        tail.store((current_tail + 1) % capacity, std::memory_order_seq_cst);
        return value;
    }
};


// 3. optimized ring buffer, release/acquire approach
template <typename T>
class SPSCRingBuffer {
private:
    std::vector<T> buffer;
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
    size_t capacity;
    
public:
    explicit SPSCRingBuffer(size_t cap)
        : capacity(cap),
        buffer(cap), 
        head(0),
        tail(0) {}

    bool push(const T& value) {
        size_t current_head = head.load(std::memory_order_relaxed);
        size_t next_head = (current_head + 1) % capacity;
        if (next_head == tail.load(std::memory_order_acquire)) {
            return false;
        }
        buffer[current_head] = value;
        head.store(next_head, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() {
        size_t current_tail = tail.load(std::memory_order_relaxed);
        if (current_tail == head.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        T value = buffer[current_tail];
        tail.store((current_tail + 1) % capacity, std::memory_order_release);
        return value;
    }
};