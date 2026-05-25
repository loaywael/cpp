#include <iostream>
#include <chrono>
#include <atomic>
#include <vector>
#include <numeric>
#include <iomanip>
#include <thread>
#include "utils.hpp"

// Active-spin delay to simulate actual CPU computation (avysync yield)
void active_spin_work(int ms) {
    auto start = std::chrono::high_resolution_clock::now();
    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start).count();
        if (elapsed >= ms) break;
    }
}

// Print detailed telemetry table of task shares per core
void print_telemetry_table(const std::string& test_name, 
                           const std::vector<size_t>& counts, 
                           double elapsed_ms, 
                           size_t total_tasks) {
    std::cout << "\n========================================================================\n";
    std::cout << "📊 " << test_name << " TELEMETRY\n";
    std::cout << "========================================================================\n";
    std::cout << std::left 
              << std::setw(15) << "Worker ID" 
              << std::right 
              << std::setw(18) << "Tasks Executed" 
              << std::setw(18) << "Share (%)" 
              << std::setw(20) << "Status" << "\n";
    std::cout << "------------------------------------------------------------------------\n";
    
    for (size_t i = 0; i < counts.size(); ++i) {
        double share = (static_cast<double>(counts[i]) / total_tasks) * 100.0;
        std::string status = "Idle";
        if (counts[i] > 0) {
            if (test_name.find("No-Stealing") != std::string::npos) {
                status = (i == 0) ? "Single Target Choked" : "Idle Starved";
            } else if (test_name.find("Naive") != std::string::npos) {
                status = "Shared-Queue Puller";
            } else {
                status = (i == 0 && test_name.find("Imbalanced") != std::string::npos) 
                         ? "Producer Target" 
                         : "Stealer Core";
            }
            if (test_name.find("Balanced") != std::string::npos) {
                status = "Active Worker";
            }
        }
        std::cout << std::left << "  Worker " << std::setw(7) << i << std::right
                  << std::setw(18) << counts[i]
                  << std::fixed << std::setprecision(2)
                  << std::setw(17) << share << "%"
                  << std::setw(20) << status << "\n";
    }
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << "⏱️  Total Processing Time: " << std::fixed << std::setprecision(2) << elapsed_ms << " ms\n";
    std::cout << "⚡ Throughput: " << std::fixed << std::setprecision(2) 
              << (total_tasks / (elapsed_ms / 1000.0)) << " tasks/sec\n";
    std::cout << "========================================================================\n";
}

// Templated benchmark logic to promote SINE, KISS, DRY, and YAGNI
template <typename PoolType>
void run_test(const std::string& name, 
              size_t n_threads, 
              size_t num_tasks, 
              int task_cost_ms, 
              bool submit_to_worker_zero) {
    std::cout << "\n🧪 Running Test: " << name << "...\n";
    if (submit_to_worker_zero) {
        std::cout << "👉 Action: Submitting all " << num_tasks << " tasks to Worker 0 only.\n";
    } else {
        std::cout << "👉 Action: Submitting all " << num_tasks << " tasks round-robin.\n";
    }

    std::vector<std::atomic<size_t>> task_counts(n_threads);
    for (size_t i = 0; i < n_threads; ++i) task_counts[i].store(0);
    std::atomic<size_t> completed{0};

    auto start = std::chrono::high_resolution_clock::now();

    {
        PoolType pool(n_threads);

        // Submit the tasks to the pool
        for (size_t i = 0; i < num_tasks; ++i) {
            auto task = [&task_counts, &completed, task_cost_ms]() {
                active_spin_work(task_cost_ms);
                if (current_worker_id >= 0 && static_cast<size_t>(current_worker_id) < task_counts.size()) {
                    task_counts[current_worker_id].fetch_add(1, std::memory_order_relaxed);
                }
                completed.fetch_add(1, std::memory_order_release);
            };

            if (submit_to_worker_zero) {
                pool.submit_to(0, std::move(task));
            } else {
                pool.submit(std::move(task));
            }
        }

        // Active wait loop until all tasks are executed
        while (completed.load(std::memory_order_acquire) < num_tasks) {
            std::this_thread::yield();
        }
    } // Pool is destroyed here: stops and joins all threads.

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;

    std::vector<size_t> final_counts(n_threads);
    for (size_t i = 0; i < n_threads; ++i) {
        final_counts[i] = task_counts[i].load();
    }

    print_telemetry_table(name, final_counts, elapsed, num_tasks);
}

int main() {
    const size_t n_threads = std::thread::hardware_concurrency();
    std::cout << "🚀 CPU Hardware Threads Available: " << n_threads << "\n";
    std::cout << "💡 Starting Work-Stealing vs. Naive vs. No-Stealing Benchmarks...\n";

    const size_t num_tasks = 200;
    const int task_cost_ms = 10; // 10ms compute per task

    // =========================================================================
    // 🔥 SCENARIO 1: Highly Imbalanced Workload (All tasks sent to Worker 0)
    // =========================================================================
    std::cout << "\n========================================================================\n";
    std::cout << "🔥 SCENARIO 1: Highly Imbalanced Workload (All tasks sent to Worker 0)\n";
    std::cout << "========================================================================\n";

    run_test<WorkStealingThreadPool>("Work-Stealing Pool (Imbalanced)", n_threads, num_tasks, task_cost_ms, true);
    run_test<LockFreeThreadPool>("Lock-Free Chase-Lev Pool (Imbalanced)", n_threads, num_tasks, task_cost_ms, true);
    run_test<NoStealingThreadPool>("No-Stealing Pool (Imbalanced Choke)", n_threads, num_tasks, task_cost_ms, true);
    run_test<NaiveThreadPool>("Naive Shared-Queue Pool (Imbalanced)", n_threads, num_tasks, task_cost_ms, true);

    // =========================================================================
    // 🌀 SCENARIO 2: Balanced Workload (Tasks submitted Round-Robin)
    // =========================================================================
    std::cout << "\n========================================================================\n";
    std::cout << "🌀 SCENARIO 2: Balanced Workload (Tasks submitted Round-Robin)\n";
    std::cout << "========================================================================\n";

    run_test<WorkStealingThreadPool>("Work-Stealing Pool (Balanced RR)", n_threads, num_tasks, task_cost_ms, false);
    run_test<LockFreeThreadPool>("Lock-Free Chase-Lev Pool (Balanced RR)", n_threads, num_tasks, task_cost_ms, false);
    run_test<NoStealingThreadPool>("No-Stealing Pool (Balanced RR)", n_threads, num_tasks, task_cost_ms, false);
    run_test<NaiveThreadPool>("Naive Shared-Queue Pool (Balanced RR)", n_threads, num_tasks, task_cost_ms, false);

    std::cout << "\n🏁 Comparative profiling completed successfully!\n";
    return 0;
}
