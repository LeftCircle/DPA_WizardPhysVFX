#pragma once


//#include <nanovdb/tools/CreateNanoGrid.h> // converter from OpenVDB to NanoVDB (includes NanoVDB.h and GridManager.h)
#include <cstddef>
#include <cuda_runtime.h>
#include <curand_kernel.h>
#include <nanovdb/NanoVDB.h>
#include <nanovdb/cuda/Buffer.h>
#include <nanovdb/cuda/HandleStorage.h>

#include "Camera.h"
#include "openvdb_helpers.h"
//#include "ray_marcher.h"

namespace lux{

using RngState = curandStatePhilox4_32_10_t;

inline float3 to_float3(const Vector& v) noexcept {
    return make_float3(v.x(), v.y(), v.z());
}

struct GPUVolume{
    const nanovdb::NanoGrid<float>* density;
    const nanovdb::NanoGrid<nanovdb::Vec3f>* color;
    const nanovdb::NanoGrid<float>** shadow_maps;
    const float4* light_colors; // includes kappa
    int shadow_map_count = 0;
};

struct GPUCamera {
    explicit GPUCamera(const Camera& cam)
        : eye(to_float3(cam.eye())),
          right(to_float3(cam.right())),
          up(to_float3(cam.up())),
          view(to_float3(cam.view())),
          horizontal_tan_fov(cam.get_htanfov()),
          vertical_tan_fov(cam.get_vtanfov()) {}
    float3 eye;
    float3 right;
    float3 up;
    float3 view;
    float horizontal_tan_fov;
    float vertical_tan_fov;
};

cudaError_t launch_cuda_raymarch(
    GPUVolume render_data,
    float4* output,
    int width,
    int height,
    GPUCamera cam,
    float near_dist,
    float far_dist,
    float min_step,
    float max_step,
    float kappa,
    RngState* rng_states,
    cudaStream_t stream
);

cudaError_t launch_cuda_shadowmap(
    const nanovdb::NanoGrid<float>* device_density_grid,
    float* out,
    coordbbox dims,
    Vector world_origin,
    float voxel_size,
    Vector light_position,
    float shadow_step,
    float kappa,
    cudaStream_t stream
); 

cudaError_t create_rng_states(
    RngState** states,
    int pixel_count,
    unsigned long long seed,
    cudaStream_t stream
);

cudaError_t destroy_rng_states(RngState* states);

// -------------------------------------------------------------
// A cuda demo
// -------------------------------------------------------------

// cudaError_t launchThreshold(
//     const nanovdb::NanoGrid<float>* deviceGrid,
//     float* deviceOutput,
//     nanovdb::Coord first,
//     int count,
//     float threshold,
//     cudaStream_t stream);


} // end namespace lux
