#pragma once

#include <vector>
#include <memory>
#include <numeric>
#include <new>       // Required for placement new
#include <random>
#include <cstdlib>
#include <fstream>
#include <execution>


class Point3D {
public:
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    Point3D() = default;
    Point3D(float x, float y, float z) : x(x), y(y), z(z) {}
    virtual ~Point3D() = default;
};

struct alignas(16) Point3D16 {
    float x, y, z;
};

using PointCloud = std::vector<std::unique_ptr<Point3D>>;
using PointCloudAoS = std::vector<Point3D>;
using PointCLoud16 = std::vector<Point3D16>;

struct PointCloudSoA {
    std::vector<float> x;
    std::vector<float> y;
    std::vector<float> z;
    PointCloudSoA() = default;
    PointCloudSoA(size_t n) : x(n), y(n), z(n) {}
    size_t size() const { return x.size(); }
};

inline PointCloud generate_pc(size_t n) {
    PointCloud cloud;
    cloud.reserve(n);
    for(size_t i = 0; i < n; ++i) {
        float x = static_cast<float>(rand()) / RAND_MAX;
        float y = static_cast<float>(rand()) / RAND_MAX;
        float z = static_cast<float>(rand()) / RAND_MAX;
        cloud.push_back(std::make_unique<Point3D>(x, y, z));
    }
    return cloud;
}

inline PointCloudAoS generate_pc_aos(size_t n) {
    PointCloudAoS cloud;
    cloud.reserve(n);
    for (size_t i=0; i<n; i++) {
        float x = static_cast<float>(rand()) / RAND_MAX;
        float y = static_cast<float>(rand()) / RAND_MAX;
        float z = static_cast<float>(rand()) / RAND_MAX;
        cloud.push_back(Point3D(x, y, z));
    }
    return cloud;
}

inline PointCLoud16 generate_pc16(size_t n) {
    PointCLoud16 cloud;
    cloud.reserve(n);
    for(size_t i=0; i<n; ++i) {
        float x = static_cast<float>(rand()) / RAND_MAX;
        float y = static_cast<float>(rand()) / RAND_MAX;
        float z = static_cast<float>(rand()) / RAND_MAX;
        cloud.push_back(Point3D16{x, y, z});
    }
    return cloud;
}

inline PointCloudSoA generate_pc_soa(size_t n) {
    PointCloudSoA cloud(n);
    for (size_t i = 0; i < n; i++) {
        float x = static_cast<float>(rand()) / RAND_MAX;
        float y = static_cast<float>(rand()) / RAND_MAX;
        float z = static_cast<float>(rand()) / RAND_MAX;
        cloud.x[i] = x;
        cloud.y[i] = y;
        cloud.z[i] = z;
    }
    return cloud;
}

// ==========================================
// 🧮 Processing Algorithms (Mass Centers)
// ==========================================

inline Point3D naiive_findMassCenter(const PointCloud& cloud) {
    float sumX = 0, sumY = 0, sumZ = 0;
    float size = static_cast<float>(cloud.size());
    for (const auto& pt_arr : cloud) {
        sumX += pt_arr->x;
        sumY += pt_arr->y;
        sumZ += pt_arr->z;
    }
    return {sumX/size, sumY/size, sumZ/size};
}

inline Point3D aos_findMassCenter(const PointCloudAoS& cloud) {
    float sumX=0, sumY=0, sumZ=0;
    float size = static_cast<float>(cloud.size());
    for (const auto& pt : cloud) {
        sumX += pt.x;
        sumY += pt.y;
        sumZ += pt.z;
    }
    return {sumX/size, sumY/size, sumZ/size};
}

inline Point3D16 alignas_findMassCenter(const PointCLoud16& cloud) {
    float sumX=0, sumY=0, sumZ=0;
    float size = static_cast<float>(cloud.size());
    for (const auto& pt : cloud) {
        sumX += pt.x;
        sumY += pt.y;
        sumZ += pt.z;
    }
    return {sumX/size, sumY/size, sumZ/size};
}

inline Point3D soa_findMassCenter(const PointCloudSoA& cloud) {
    float sumX=0, sumY=0, sumZ=0;
    float size = static_cast<float>(cloud.x.size());
    for (size_t i=0; i<size; i++) {
        sumX += cloud.x[i];
        sumY += cloud.y[i];
        sumZ += cloud.z[i];
    }
    return {sumX/size, sumY/size, sumZ/size};
}

inline Point3D simd_findMassCenterr(const PointCloudSoA& cloud) {
    float n_points = static_cast<float>(cloud.size());
    float sumX=0.0f, sumY=0.0f, sumZ=0.0f;
    sumX = std::reduce(
        cloud.x.begin(),
        cloud.x.end(),
        0.0f
    );
    sumY = std::reduce(
        cloud.y.begin(),
        cloud.y.end(),
        0.0f
    );
    sumZ = std::reduce(
        cloud.z.begin(),
        cloud.z.end(),
        0.0f
    );
    return {sumX/n_points, sumY/n_points, sumZ/n_points};
}

inline Point3D simd_findMassCenterr_par(const PointCloudSoA& cloud) {
    float n_points = static_cast<float>(cloud.size());
    float sumX=0.0f, sumY=0.0f, sumZ=0.0f;
    sumX = std::reduce(
        std::execution::par_unseq,
        cloud.x.begin(),
        cloud.x.end(),
        0.0f
    );
    sumY = std::reduce(
        std::execution::par_unseq,
        cloud.y.begin(),
        cloud.y.end(),
        0.0f
    );
    sumZ = std::reduce(
        std::execution::par_unseq,
        cloud.z.begin(),
        cloud.z.end(),
        0.0f
    );
    return {sumX/n_points, sumY/n_points, sumZ/n_points};
}

inline std::vector<Point3D> load_pc_file(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Error opening file: " + file_path);
    }
    // read file header
    uint32_t n_points;
    file.read(reinterpret_cast<char*>(&n_points), sizeof(n_points));
    // read all points into the heap storage
    std::vector<Point3D> cloud(n_points);
    file.read(
        reinterpret_cast<char*>(cloud.data()),
        n_points * sizeof(Point3D)
    );
    return cloud;
}

class Arena {
private:
    std::unique_ptr<std::byte[]> buffer;
    size_t size = 0;
    size_t offset = 0;
public:
    explicit Arena(size_t n) : size(n), buffer(std::make_unique<std::byte[]>(n)) {}
    void* allocate(size_t n, size_t alignment=alignof(std::max_align_t)) {
        void* current_ptr = buffer.get() + offset;
        size_t remaining_space = size - offset;
        void* aligned_ptr = std::align(alignment, n, current_ptr, remaining_space);
        if (!aligned_ptr) {
            throw std::bad_alloc();
        }
        offset = static_cast<std::byte*>(aligned_ptr) + n - buffer.get();
        return aligned_ptr;
    }
    void reset() noexcept {
        offset = 0;
    }

    template <typename T, typename... Args>
    T* construct(Args&&... args) {
        void* memory = allocate(sizeof(T), alignof(T));
        return ::new (memory) T(std::forward<Args>(args)...);
    }
};