#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include "utils.hpp"


struct BenchmarkResult {
    double naive_time = 0.0;
    double aos_time = 0.0;
    double aligned_time = 0.0;
    double soa_time = 0.0;
    double simd_time = 0.0;
    double simd_par_time = 0.0;
};

BenchmarkResult run_benchmark(size_t N) {
    BenchmarkResult res;
    std::cout << "\n==========================================" << std::endl;
    std::cout << "🚀 Running Benchmark with N = " << N << " points..." << std::endl;
    std::cout << "==========================================" << std::endl;

    // ❌ Naive Implementation (unique_ptr vector)
    {
        std::cout << "Generating naive point cloud..." << std::endl;
        PointCloud cloud = generate_pc(N);
        std::cout << "Calculating center (Naive)..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Point3D center = naiive_findMassCenter(cloud);
        auto end = std::chrono::high_resolution_clock::now();
        res.naive_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
        std::cout << "Naive Center: (" << center.x << ", " << center.y << ", " << center.z << ") - " << res.naive_time << " ms\n" << std::endl;
    }

    // ⚡ AoS (Unaligned)
    {
        std::cout << "Generating unaligned AoS point cloud..." << std::endl;
        PointCloudAoS cloudAoS = generate_pc_aos(N);
        std::cout << "Calculating center (AoS)..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Point3D center = aos_findMassCenter(cloudAoS);
        auto end = std::chrono::high_resolution_clock::now();
        res.aos_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
        std::cout << "AoS Center: (" << center.x << ", " << center.y << ", " << center.z << ") - " << res.aos_time << " ms\n" << std::endl;
    }

    // ⚡ Contiguous Aligned
    {
        std::cout << "Generating contiguous aligned point cloud..." << std::endl;
        PointCLoud16 cloud16 = generate_pc16(N);
        std::cout << "Calculating center (Aligned)..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Point3D16 center = alignas_findMassCenter(cloud16);
        auto end = std::chrono::high_resolution_clock::now();
        res.aligned_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
        std::cout << "Aligned Center: (" << center.x << ", " << center.y << ", " << center.z << ") - " << res.aligned_time << " ms\n" << std::endl;
    }

    // ⚡ SoA
    {
        std::cout << "Generating SoA point cloud..." << std::endl;
        PointCloudSoA cloudSoA = generate_pc_soa(N);
        
        std::cout << "Calculating center (SoA)..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Point3D center = soa_findMassCenter(cloudSoA);
        auto end = std::chrono::high_resolution_clock::now();
        res.soa_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
        std::cout << "SoA Center: (" << center.x << ", " << center.y << ", " << center.z << ") - " << res.soa_time << " ms\n" << std::endl;

        // ⚡ SIMD
        std::cout << "Calculating center (SIMD)..." << std::endl;
        auto start_simd = std::chrono::high_resolution_clock::now();
        Point3D center_simd = simd_findMassCenterr(cloudSoA);
        auto end_simd = std::chrono::high_resolution_clock::now();
        res.simd_time = std::chrono::duration_cast<std::chrono::microseconds>(end_simd - start_simd).count() / 1000.0;
        std::cout << "SIMD Center: (" << center_simd.x << ", " << center_simd.y << ", " << center_simd.z << ") - " << res.simd_time << " ms\n" << std::endl;

        // ⚡ SIMD Parallel
        std::cout << "Calculating center (SIMD Parallel)..." << std::endl;
        auto start_simd_par = std::chrono::high_resolution_clock::now();
        Point3D center_simd_par = simd_findMassCenterr_par(cloudSoA);
        auto end_simd_par = std::chrono::high_resolution_clock::now();
        res.simd_par_time = std::chrono::duration_cast<std::chrono::microseconds>(end_simd_par - start_simd_par).count() / 1000.0;
        std::cout << "SIMD Parallel Center: (" << center_simd_par.x << ", " << center_simd_par.y << ", " << center_simd_par.z << ") - " << res.simd_par_time << " ms\n" << std::endl;
    }

    return res;
}

void print_summary_table(const BenchmarkResult& r1, const BenchmarkResult& r10, const BenchmarkResult& r100) {
    std::cout << "\n========================================================================\n";
    std::cout << "                       📊 BENCHMARK SUMMARY TABLE                       \n";
    std::cout << "========================================================================\n";
    std::cout << std::left 
              << std::setw(25) << "Implementation" 
              << std::right 
              << std::setw(15) << "1M Points" 
              << std::setw(15) << "10M Points" 
              << std::setw(15) << "100M Points" << "\n";
    std::cout << "------------------------------------------------------------------------\n";
    
    auto print_row = [](const std::string& name, double t1, double t10, double t100) {
        std::cout << std::left << std::setw(25) << name << std::right
                  << std::fixed << std::setprecision(2)
                  << std::setw(12) << t1 << " ms"
                  << std::setw(12) << t10 << " ms"
                  << std::setw(12) << t100 << " ms\n";
    };

    print_row("Naive (unique_ptr)", r1.naive_time, r10.naive_time, r100.naive_time);
    print_row("AoS (Unaligned 24B)", r1.aos_time, r10.aos_time, r100.aos_time);
    print_row("Aligned AoS (16B)", r1.aligned_time, r10.aligned_time, r100.aligned_time);
    print_row("SoA (12B)", r1.soa_time, r10.soa_time, r100.soa_time);
    print_row("SIMD (std::reduce)", r1.simd_time, r10.simd_time, r100.simd_time);
    print_row("SIMD Parallel", r1.simd_par_time, r10.simd_par_time, r100.simd_par_time);
    std::cout << "========================================================================\n";
}

int main() {
    auto r1 = run_benchmark(1000000);
    auto r10 = run_benchmark(10000000);
    auto r100 = run_benchmark(100000000);

    print_summary_table(r1, r10, r100);
    return 0;
}