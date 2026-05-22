#include <iostream>
#include <chrono>
#include "utils.hpp"



int main() {
    constexpr size_t N = 10000000; // 10 million points
    std::cout << "Starting comparison with " << N << " points...\n";
    
    // ==========================================
    // ❌ Naive Implementation (unique_ptr vector)
    // ==========================================
    std::cout << "\nGenerating naive point cloud..." << std::endl;
    PointCloud cloud = generate_pc(N);
    
    std::cout << "Calculating center (Naive)..." << std::endl;
    auto start_naive = std::chrono::high_resolution_clock::now();
    Point3D center_naive = naiive_findMassCenter(cloud);
    auto end_naive = std::chrono::high_resolution_clock::now();
    
    std::cout << "--- Naive Approach Results ---" << std::endl;
    std::cout << "Center: (" << center_naive.x << ", " << center_naive.y << ", " << center_naive.z << ")" << std::endl;
    std::cout << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end_naive - start_naive).count() << " ms" << std::endl;
    
    // Clear naive memory to free up RAM
    cloud.clear();
    
    // ==========================================
    // ⚡ Aligned Contiguous Implementation (AoS)
    // ==========================================
    std::cout << "\nGenerating aligned point cloud (AoS)" << std::endl;
    PointCloudAoS cloudAoS = generate_pc_aos(N);

    std::cout << "Calculating center (AoS)" << std::endl;
    auto start_aos = std::chrono::high_resolution_clock::now();
    Point3D center_aos = aos_findMassCenter(cloudAoS);
    auto end_aos = std::chrono::high_resolution_clock::now();

    std::cout << "--- AoS Approach Results ---" << std::endl;
    std::cout << "Center: " << center_aos.x << ", " << center_aos.y << ", " << center_aos.z << std::endl;
    std::cout << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end_aos - start_aos).count() << "ms" << std::endl;

    cloudAoS.clear();

    // ==========================================
    // ⚡ Aligned Contiguous Implementation (AoS)
    // ==========================================
    std::cout << "\nGenerating contiguous aligned point cloud..." << std::endl;
    PointCLoud16 cloud16 = generate_pc16(N);
    
    std::cout << "Calculating center (Aligned)..." << std::endl;
    auto start_aligned = std::chrono::high_resolution_clock::now();
    Point3D16 center_aligned = alignas_findMassCenter(cloud16);
    auto end_aligned = std::chrono::high_resolution_clock::now();
    
    std::cout << "--- Contiguous Aligned Results ---" << std::endl;
    std::cout << "Center: (" << center_aligned.x << ", " << center_aligned.y << ", " << center_aligned.z << ")" << std::endl;
    std::cout << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end_aligned - start_aligned).count() << " ms" << std::endl;

    cloud16.clear();

    // ==========================================
    // ⚡ SoA Implementation
    // ==========================================
    std::cout << "\nGenerating SoA point cloud..." << std::endl;
    PointCloudSoA cloudSoA = generate_pc_soa(N);
    
    std::cout << "Calculating center (SoA)..." << std::endl;
    auto start_soa = std::chrono::high_resolution_clock::now();
    Point3D center_soa = soa_findMassCenter(cloudSoA);
    auto end_soa = std::chrono::high_resolution_clock::now();
    
    std::cout << "--- SoA Approach Results ---" << std::endl;
    std::cout << "Center: (" << center_soa.x << ", " << center_soa.y << ", " << center_soa.z << ")" << std::endl;
    std::cout << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end_soa - start_soa).count() << " ms" << std::endl;
}