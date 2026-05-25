#pragma once
#include <thread>
#include <vector>
#include <deque>
#include <mutex>
#include <optional>
#include <functional>
#include <atomic>
#include <condition_variable>
#include <memory>

using Task = std::function<void()>;

inline thread_local int current_worker_id = -1;

class WorkerThread {
private:
    std::deque<Task> local_tasks;
    mutable std::mutex mtx; // mutable allows locking in const methods like is_empty()
public:
    // Thread pulls from the bottom (LIFO) for optimal cache locality of local work
    std::optional<Task> pop_task() {
        std::lock_guard<std::mutex> lock(mtx);
        if (local_tasks.empty()) return std::nullopt;
        Task task = std::move(local_tasks.back());
        local_tasks.pop_back();
        return task;
    }

    // Other threads steal from the top (FIFO) to get larger chunks of parallel work
    std::optional<Task> steal_task() {
        std::lock_guard<std::mutex> lock(mtx);
        if (local_tasks.empty()) return std::nullopt;
        Task task = std::move(local_tasks.front());
        local_tasks.pop_front();
        return task;
    }
    
    // Submit task to the current thread
    void push_task(Task task) {
        std::lock_guard<std::mutex> lock(mtx);
        local_tasks.push_back(std::move(task));
    }
    
    bool is_empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return local_tasks.empty();
    }
};

class WorkStealingThreadPool {
private:
    std::vector<WorkerThread> workers;
    std::vector<std::thread> threads;
    std::atomic<size_t> next_worker{0};
    std::atomic<bool> shutdown{false};

public:
    WorkStealingThreadPool(size_t n_threads) : workers(n_threads), shutdown(false) {
        for (size_t i = 0; i < n_threads; ++i) {
            threads.emplace_back(
                [this, i]() {
                    this->worker_loop(i);
                }
            );
        }
    }

    // Destructor explicitly stops the workers and joins the threads
    ~WorkStealingThreadPool() {
        shutdown.store(true, std::memory_order_release);
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }
    
    // Submit a task to the pool in a load-balanced round-robin fashion
    void submit(Task task) {
        size_t id = next_worker.fetch_add(1, std::memory_order_relaxed) % workers.size();
        workers[id].push_task(std::move(task));
    }

    // Submit a task directly to a specific worker's queue (useful for testing imbalance)
    void submit_to(size_t worker_id, Task task) {
        if (worker_id < workers.size()) {
            workers[worker_id].push_task(std::move(task));
        }
    }

    void worker_loop(size_t worker_id) {
        current_worker_id = static_cast<int>(worker_id);
        while (!shutdown.load(std::memory_order_acquire)) {
            // 1. Pop a local task and execute immediately (LIFO)
            if (auto task = workers[worker_id].pop_task()) {
                (*task)();
                continue; // Run next local task directly to keep local cache warm
            }
            
            // 2. If out of work, attempt to steal from other workers (round-robin search)
            bool stole = false;
            for (size_t i = 0; i < workers.size(); ++i) {
                size_t target_id = (worker_id + 1 + i) % workers.size();
                if (target_id == worker_id) continue;
                
                if (auto task = workers[target_id].steal_task()) {
                    (*task)();
                    stole = true;
                    break; // Successfully stole and ran a task, loop back
                }
            }
            
            // 3. If no local work and no stealing opportunities, yield to avoid CPU spinning
            if (!stole) {
                std::this_thread::yield();
            }
        }
    }

    size_t size() const {
        return workers.size();
    }
};

class NoStealingThreadPool {
private:
    std::vector<WorkerThread> workers;
    std::vector<std::thread> threads;
    std::atomic<size_t> next_worker{0};
    std::atomic<bool> shutdown{false};

public:
    NoStealingThreadPool(size_t n_threads) : workers(n_threads), shutdown(false) {
        for (size_t i = 0; i < n_threads; ++i) {
            threads.emplace_back([this, i]() { this->worker_loop(i); });
        }
    }

    ~NoStealingThreadPool() {
        shutdown.store(true, std::memory_order_release);
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
    }

    void submit(Task task) {
        size_t id = next_worker.fetch_add(1, std::memory_order_relaxed) % workers.size();
        workers[id].push_task(std::move(task));
    }

    void submit_to(size_t worker_id, Task task) {
        if (worker_id < workers.size()) {
            workers[worker_id].push_task(std::move(task));
        }
    }

    void worker_loop(size_t worker_id) {
        current_worker_id = static_cast<int>(worker_id);
        while (!shutdown.load(std::memory_order_acquire)) {
            if (auto task = workers[worker_id].pop_task()) {
                (*task)();
                continue;
            }
            std::this_thread::yield();
        }
    }

    size_t size() const {
        return workers.size();
    }
};

class NaiveThreadPool {
private:
    std::deque<Task> queue;
    std::mutex mtx;
    std::condition_variable cv;
    std::vector<std::thread> threads;
    std::atomic<bool> shutdown{false};

public:
    NaiveThreadPool(size_t n_threads) : shutdown(false) {
        for (size_t i = 0; i < n_threads; ++i) {
            threads.emplace_back([this, i]() { this->worker_loop(i); });
        }
    }

    ~NaiveThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            shutdown.store(true, std::memory_order_release);
        }
        cv.notify_all();
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
    }

    void submit(Task task) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push_back(std::move(task));
        }
        cv.notify_one();
    }

    void submit_to(size_t /*worker_id*/, Task task) {
        submit(std::move(task));
    }

    void worker_loop(size_t worker_id) {
        current_worker_id = static_cast<int>(worker_id);
        while (true) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this]() {
                    return shutdown.load(std::memory_order_acquire) || !queue.empty();
                });
                if (shutdown.load(std::memory_order_acquire) && queue.empty()) {
                    return;
                }
                task = std::move(queue.front());
                queue.pop_front();
            }
            task();
        }
    }

    size_t size() const {
        return threads.size();
    }
};

class ChaseLevQueue {
private:
    struct CircularArray {
        size_t capacity;
        std::vector<std::atomic<Task*>> tasks;
        CircularArray(size_t cap) : capacity(cap), tasks(cap) {
            for (size_t i = 0; i < cap; ++i) {
                tasks[i].store(nullptr, std::memory_order_relaxed);
            }
        }
    };

    std::atomic<int64_t> top{0};
    std::atomic<int64_t> bottom{0};
    std::unique_ptr<CircularArray> array;

public:
    ChaseLevQueue(size_t cap = 2048) : array(std::make_unique<CircularArray>(cap)) {}

    ~ChaseLevQueue() {
        CircularArray* a = array.get();
        if (a) {
            int64_t b = bottom.load(std::memory_order_relaxed);
            int64_t t = top.load(std::memory_order_relaxed);
            for (int64_t i = t; i < b; ++i) {
                if (Task* ptr = a->tasks[i % a->capacity].load(std::memory_order_relaxed)) {
                    delete ptr;
                }
            }
        }
    }
    
    // Copy construction and assignment are disabled for lock-free queues
    ChaseLevQueue(const ChaseLevQueue&) = delete;
    ChaseLevQueue& operator=(const ChaseLevQueue&) = delete;
    
    // Allow move semantics for vector storage
    ChaseLevQueue(ChaseLevQueue&& other) noexcept 
        : top(other.top.load(std::memory_order_relaxed)),
          bottom(other.bottom.load(std::memory_order_relaxed)),
          array(std::move(other.array)) {}

    // Push is ONLY called by the owner thread (LIFO push)
    void push(Task* task_ptr) {
        int64_t b = bottom.load(std::memory_order_relaxed);
        int64_t t = top.load(std::memory_order_acquire);
        CircularArray* a = array.get();
        
        if (b - t >= static_cast<int64_t>(a->capacity)) {
            // Buffer overflow - fallback to immediate execution to keep system robust
            (*task_ptr)();
            delete task_ptr;
            return;
        }
        
        a->tasks[b % a->capacity].store(task_ptr, std::memory_order_release);
        bottom.store(b + 1, std::memory_order_release);
    }

    // Pop is ONLY called by the owner thread (LIFO pop)
    Task* pop() {
        int64_t b = bottom.load(std::memory_order_relaxed) - 1;
        bottom.store(b, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        
        int64_t t = top.load(std::memory_order_relaxed);
        Task* result = nullptr;
        
        if (t <= b) {
            CircularArray* a = array.get();
            result = a->tasks[b % a->capacity].load(std::memory_order_relaxed);
            a->tasks[b % a->capacity].store(nullptr, std::memory_order_relaxed);
            
            if (t == b) {
                // Exactly 1 task left in the queue. Synchronize with potential concurrent stealers.
                if (!top.compare_exchange_strong(t, t + 1, 
                                                 std::memory_order_seq_cst, 
                                                 std::memory_order_relaxed)) {
                    // Lost the race to a stealer
                    result = nullptr;
                }
                bottom.store(b + 1, std::memory_order_relaxed);
            }
        } else {
            // Queue was already empty
            bottom.store(b + 1, std::memory_order_relaxed);
        }
        return result;
    }

    // Steal is called by other (stealer) threads (FIFO steal)
    Task* steal() {
        while (true) {
            int64_t t = top.load(std::memory_order_acquire);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            int64_t b = bottom.load(std::memory_order_acquire);
            
            if (t >= b) {
                return nullptr; // Queue is empty
            }
            
            CircularArray* a = array.get();
            Task* task_ptr = a->tasks[t % a->capacity].load(std::memory_order_acquire);
            
            if (task_ptr == nullptr) {
                std::this_thread::yield();
                continue;
            }
            
            if (top.compare_exchange_strong(t, t + 1, 
                                             std::memory_order_seq_cst, 
                                             std::memory_order_relaxed)) {
                a->tasks[t % a->capacity].store(nullptr, std::memory_order_relaxed);
                return task_ptr; // Steal succeeded!
            }
            // CAS failed (lost the race to another stealer). Loop and retry.
        }
    }

    bool is_empty() const {
        return top.load(std::memory_order_relaxed) >= bottom.load(std::memory_order_relaxed);
    }
};

class LockFreeMPSCQueue {
private:
    struct Node {
        std::atomic<Node*> next{nullptr};
        Task* task{nullptr};
        Node(Task* t) : task(t) {}
    };

    std::atomic<Node*> head;
    Node* tail; // Accessed only by consumer (owner thread)

public:
    LockFreeMPSCQueue() {
        Node* stub = new Node(nullptr);
        head.store(stub, std::memory_order_relaxed);
        tail = stub;
    }

    ~LockFreeMPSCQueue() {
        Node* curr = tail;
        while (curr) {
            Node* next = curr->next.load(std::memory_order_relaxed);
            if (curr->task) delete curr->task;
            delete curr;
            curr = next;
        }
    }

    // Disable copy
    LockFreeMPSCQueue(const LockFreeMPSCQueue&) = delete;
    LockFreeMPSCQueue& operator=(const LockFreeMPSCQueue&) = delete;

    // Enable move constructor for vector storage
    LockFreeMPSCQueue(LockFreeMPSCQueue&& other) noexcept 
        : head(other.head.load(std::memory_order_relaxed)),
          tail(other.tail) {
        other.head.store(nullptr, std::memory_order_relaxed);
        other.tail = nullptr;
    }

    // Lock-free, multi-producer safe enqueue using atomic exchange
    void enqueue(Task* task_ptr) {
        Node* node = new Node(task_ptr);
        Node* prev = head.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    // Lock-free, single-consumer pop
    Task* dequeue() {
        Node* t = tail;
        if (t == nullptr) return nullptr;
        Node* next_node = t->next.load(std::memory_order_acquire);
        if (next_node != nullptr) {
            tail = next_node;
            Task* task = next_node->task;
            next_node->task = nullptr; // Clear to prevent double free in destructor
            delete t; // Clean up old stub node
            return task;
        }
        return nullptr;
    }
};

struct LockFreeWorker {
    ChaseLevQueue local_queue;
    LockFreeMPSCQueue external_queue;

    LockFreeWorker() : local_queue(2048) {}

    // Disable copy
    LockFreeWorker(const LockFreeWorker&) = delete;
    LockFreeWorker& operator=(const LockFreeWorker&) = delete;

    // Enable move constructor for vector initialization
    LockFreeWorker(LockFreeWorker&& other) noexcept
        : local_queue(std::move(other.local_queue)),
          external_queue(std::move(other.external_queue)) {}
};

class LockFreeThreadPool {
private:
    std::vector<LockFreeWorker> workers;
    std::vector<std::thread> threads;
    std::atomic<size_t> next_worker{0};
    std::atomic<bool> shutdown{false};

public:
    LockFreeThreadPool(size_t n_threads) : workers(n_threads), shutdown(false) {
        for (size_t i = 0; i < n_threads; ++i) {
            threads.emplace_back([this, i]() { this->worker_loop(i); });
        }
    }

    ~LockFreeThreadPool() {
        shutdown.store(true, std::memory_order_release);
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
        // Clean up any remaining tasks in external queues
        for (auto& w : workers) {
            while (Task* ptr = w.external_queue.dequeue()) {
                delete ptr;
            }
        }
    }

    void submit(Task task) {
        size_t id = next_worker.fetch_add(1, std::memory_order_relaxed) % workers.size();
        submit_to(id, std::move(task));
    }

    void submit_to(size_t worker_id, Task task) {
        if (worker_id < workers.size()) {
            workers[worker_id].external_queue.enqueue(new Task(std::move(task)));
        }
    }

    void worker_loop(size_t worker_id) {
        current_worker_id = static_cast<int>(worker_id);
        auto& self = workers[worker_id];
        
        while (!shutdown.load(std::memory_order_acquire)) {
            // 1. Drain the external lock-free MPSC queue and push to local Chase-Lev queue (as the single owner)
            while (Task* task_ptr = self.external_queue.dequeue()) {
                self.local_queue.push(task_ptr);
            }

            // 2. Pop a local task (LIFO pop, fully lock-free)
            if (Task* task_ptr = self.local_queue.pop()) {
                (*task_ptr)();
                delete task_ptr;
                continue;
            }
            
            // 3. Try to steal from other workers (FIFO steal, fully lock-free)
            bool stole = false;
            for (size_t i = 0; i < workers.size(); ++i) {
                size_t target_id = (worker_id + 1 + i) % workers.size();
                if (target_id == worker_id) continue;
                
                if (Task* task_ptr = workers[target_id].local_queue.steal()) {
                    (*task_ptr)();
                    delete task_ptr;
                    stole = true;
                    break;
                }
            }
            
            // 4. Yield briefly to prevent 100% spin if completely out of work
            if (!stole) {
                std::this_thread::yield();
            }
        }
    }

    size_t size() const {
        return workers.size();
    }
};