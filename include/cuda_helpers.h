#pragma once

#include <type_traits>

#include <nanovdb/tools/CreateNanoGrid.h>

#include <cuda_runtime.h>
#include <nanovdb/tools/CreateNanoGrid.h>
#include <nanovdb/cuda/DeviceBuffer.h>


namespace lux{

#define CUDA_CHECK(call)                                      \
    do {                                                      \
        cudaError_t err = (call);                             \
        if (err != cudaSuccess)                               \
            throw std::runtime_error(cudaGetErrorString(err));\
    } while (0)


struct NanoAndBuffer{
    nanovdb::GridHandle<nanovdb::HostBuffer> nano;
    void* buffer;
};

template <typename GridPtr>
auto pack_grid_to_nano(const GridPtr& grid, cudaStream_t stream) {
    using GridT = std::remove_cv_t<std::remove_reference_t<decltype(*grid)>>;
    using ValueT = typename GridT::ValueType;
    using NanoValueT = typename nanovdb::tools::MapToNano<ValueT>::type;

    auto handle = nanovdb::tools::createNanoGrid<
            GridT, NanoValueT, nanovdb::CudaDeviceBuffer
            >(*grid);

    handle.deviceUpload(stream, /*sync=*/false);
    return handle;
}

template <typename T>
T* send_array_to_gpu(const T* start, size_t elements) {
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable for a raw CUDA copy");
    if (elements == 0) return nullptr;
    const size_t bytes = elements * sizeof(T);
    T* device_data = nullptr;
    CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&device_data), bytes));
    CUDA_CHECK(cudaMemcpy(
        device_data,
        start,
        bytes,
        cudaMemcpyHostToDevice
    ));
    return device_data;
}

inline void log_cuda_memory(const char* where)
{
    std::size_t free_bytes = 0;
    std::size_t total_bytes = 0;

    const cudaError_t err = cudaMemGetInfo(&free_bytes, &total_bytes);
    if (err != cudaSuccess) {
        std::cerr << "[CUDA memory] " << where << ": "
                  << cudaGetErrorString(err) << '\n';
        return;
    }

    constexpr double bytes_per_mib = 1024.0 * 1024.0;
    const double free_mib = free_bytes / bytes_per_mib;
    const double total_mib = total_bytes / bytes_per_mib;

    std::cerr << "[CUDA memory] " << where
              << ": free " << free_mib << " MiB / "
              << total_mib << " MiB"
              << " (used " << total_mib - free_mib << " MiB)\n";
}

} // namespace lux