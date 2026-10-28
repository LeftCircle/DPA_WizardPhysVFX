#include "ray_marcher_cuda.h"
#include <nanovdb/math/SampleFromVoxels.h>

namespace lux{


// -----------------------------------------------------------------------------------------
//  Helper functions
// -----------------------------------------------------------------------------------------

using v3f = nanovdb::math::Vec3f;
using RngState = curandStatePhilox4_32_10_t;

struct CudaRay {
    float3 origin;
    float3 direction;

    __device__ __forceinline__ float3 at(float distance) const {
        return make_float3(
            origin.x + distance * direction.x,
            origin.y + distance * direction.y,
            origin.z + distance * direction.z
        );
    }
};

/*
Used with 
auto densityAcc = volume.density->getAccessor();
auto densitySampler = nanovdb::math::createSampler<1>(densityAcc);
<1> does trilinear interp
*/
template <typename GridT, typename SamplerT>
__device__ typename GridT::ValueType sampleTrilinearWorld(
    const GridT* grid,
    SamplerT& sampler,
    float3 world)
{
    // Keep fractional index coordinates for the interpolation weights.
    nanovdb::Vec3f ijkF = grid->worldToIndexF(nanovdb::Vec3f(world.x, world.y, world.z));

    // Floor returns the lower voxel coordinate and changes ijkF
    // to its fractional offset within that voxel.
    nanovdb::Coord ijk =
        nanovdb::math::Floor<nanovdb::Coord>(ijkF);

    typename GridT::ValueType values[2][2][2];
    sampler.stencil(ijk, values); // Fetch the 8 voxels
    return sampler.sample(ijkF, values); // interpolate them based on fractional part
}

__global__ void setup_rng_kernel(
    RngState* states,
    int pixel_count,
    unsigned long long seed)
{
    const int id = blockIdx.x * blockDim.x + threadIdx.x;
    if (id < pixel_count) {
        curand_init(seed, id, 0, &states[id]);
    }
}

cudaError_t create_rng_states(
    RngState** states,
    int pixel_count,
    unsigned long long seed,
    cudaStream_t stream)
{
    if (!states || pixel_count <= 0) return cudaErrorInvalidValue;
    *states = nullptr;

    auto err = cudaMalloc(
        reinterpret_cast<void**>(states),
        static_cast<size_t>(pixel_count) * sizeof(RngState));
    if (err != cudaSuccess) return err;

    constexpr int threads = 256;
    const int blocks = (pixel_count + threads - 1) / threads;

    setup_rng_kernel<<<blocks, threads, 0, stream>>>(
        *states, pixel_count, seed);

    err = cudaGetLastError();
    if (err != cudaSuccess) {
        cudaFree(*states);
        *states = nullptr;
    }
    return err;
}

cudaError_t destroy_rng_states(RngState* states)
{
    return states ? cudaFree(states) : cudaSuccess;
}

// -----------------------------------------------------------------------------------------
//  Shadow and render
// -----------------------------------------------------------------------------------------

__device__ float sampleShadowMap(
    const GPUVolume& volume,
    int index,
    float3 world
) {
    const auto* grid = volume.shadow_maps[index];

    if (!grid) {
        return 1.0f;
    }

    auto accessor = grid->getAccessor();
    auto sampler = nanovdb::math::createSampler<1>(accessor);

    return expf(sampleTrilinearWorld(grid, sampler, world));
}

__global__ void raymarch_kernel(
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
    RngState* rng_states
){
    const int pixel = blockIdx.x * blockDim.x + threadIdx.x;
    const int pixel_count = width * height;
    if (pixel >= pixel_count) return;

    RngState rng = rng_states[pixel];


    if (!render_data.density) {
        output[pixel] = make_float4(0.f, 0.f, 0.f, 0.f);
        return;
    }

    const int x = pixel % width;
    const int y = pixel / width;

    // Same pixel-to-ray mapping as RayMarcher::get_ray.
    const float u = (2.f * x / width - 1.f) * cam.horizontal_tan_fov;
    const float v = (2.f * y / height - 1.f) * cam.vertical_tan_fov;

    float3 dir = make_float3(
        u * cam.right.x + v * cam.up.x + cam.view.x,
        u * cam.right.y + v * cam.up.y + cam.view.y,
        u * cam.right.z + v * cam.up.z + cam.view.z
    );

    const float inv_len = rsqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    dir.x *= inv_len;
    dir.y *= inv_len;
    dir.z *= inv_len;

    
    float3 radiance = make_float3(0.f, 0.f, 0.f);
    float transmittance = 1.0f;
    
    const float voxel_step = static_cast<float>(render_data.density->voxelSize()[0]);
    const float step = min_step;
    
    if (step <= 0.f) {
        output[pixel] = make_float4(0.f, 0.f, 0.f, 0.f);
        return;
    }
    
    auto dens_acc = render_data.density->getAccessor();
    auto dens_sampler = nanovdb::math::createSampler<1>(dens_acc);
    auto color_acc = render_data.color->getAccessor();
    auto col_sampler = nanovdb::math::createSampler<1>(color_acc);
    // only randomize the first step
    const float rand_unif = curand_uniform(&rng);
    const float randomStep = min_step + rand_unif * (max_step - min_step);
    const float start = near_dist + randomStep;
    const float step_diff = max_step - min_step;
    //for (float t = start; t < far_dist && transmittance > 1e-3f; t += step)
    float t = start;
    while (t < far_dist && transmittance > 1e-4f)
    {
        const float3 world = make_float3(
            cam.eye.x + t * dir.x,
            cam.eye.y + t * dir.y,
            cam.eye.z + t * dir.z
        );

        const float density = sampleTrilinearWorld(render_data.density, dens_sampler, world);

        if (density < 0.f) {
            auto c = sampleTrilinearWorld(render_data.color, col_sampler, world);
            float3 color = make_float3(c[0], c[1], c[2]);

            float3 clights = make_float3(0, 0, 0);
            for (int i = 0; i < render_data.shadow_map_count; i++){
                float dsmt = sampleShadowMap(render_data, i, world);
                auto light_c = render_data.light_colors[i];
                clights.x += dsmt * light_c.x;
                clights.y += dsmt * light_c.y;
                clights.z += dsmt * light_c.z;
            }

            const float nextT = expf(step * kappa * density);
            const float alpha = 1.f - nextT;
            radiance.x += transmittance * alpha * color.x * clights.x;
            radiance.y += transmittance * alpha * color.y * clights.y;
            radiance.z += transmittance * alpha * color.z * clights.z;
            transmittance *= nextT;
        }
        t += min_step + curand_uniform(&rng) * step_diff;
    }
    rng_states[pixel] = rng;

    output[pixel] = make_float4(
        radiance.x, radiance.y, radiance.z, 1.f - transmittance);
}


__global__ void buildShadowMapKernel(
    const nanovdb::NanoGrid<float>* densityGrid,
    float* output,
    int3 dims,
    float3 worldOrigin,
    float voxelSize,
    float3 lightPosition,
    float shadowStep,
    float kappa)
{
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    const int count = dims.x * dims.y * dims.z;
    if (i >= count) return;

    const int x = i % dims.x;
    const int y = (i / dims.x) % dims.y;
    const int z = i / (dims.x * dims.y);

    const float3 start = make_float3(
        worldOrigin.x + x * voxelSize,
        worldOrigin.y + y * voxelSize,
        worldOrigin.z + z * voxelSize);

    const float3 toLight = make_float3(
        lightPosition.x - start.x,
        lightPosition.y - start.y,
        lightPosition.z - start.z);

    const float distance = sqrtf(
        toLight.x * toLight.x +
        toLight.y * toLight.y +
        toLight.z * toLight.z);

    if (distance <= 0.f) {
        output[i] = 1.f;
        return;
    }

    const float3 dir = make_float3(
        toLight.x / distance,
        toLight.y / distance,
        toLight.z / distance);

    auto acc = densityGrid->getAccessor();
    auto sampler = nanovdb::math::createSampler<1>(acc);

    float T = 0.f;

    for (float s = 0.f; s < distance;) {
        const float ds = fminf(shadowStep, distance - s);
        const float3 p = make_float3(
            start.x + (s + 0.5f * ds) * dir.x,
            start.y + (s + 0.5f * ds) * dir.y,
            start.z + (s + 0.5f * ds) * dir.z);

        const float rho = sampleTrilinearWorld(densityGrid, sampler, p);

        T += kappa * fminf(rho, 0.f) * ds;
        s += ds;
    }

    output[i] = T;
}


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
){
    if (width <= 0 || height <= 0) return cudaErrorInvalidValue;

    constexpr int threads = 128;
    const int pixel_count = width * height;
    const int blocks = (pixel_count + threads - 1) / threads;

    raymarch_kernel<<<blocks, threads, 0, stream>>>(
        render_data, output, width, height, cam,
        near_dist, far_dist, min_step, max_step, kappa, rng_states);

    return cudaGetLastError();
}

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
){
    constexpr int threads = 128;

    auto dim = dims.dim();
    auto int3dim = make_int3(dim[0], dim[1], dim[2]);
    const int count = (dim[0]) * (dim[1]) * (dim[2]);
    const int blocks = (count + threads - 1) / threads;
    
    const std::size_t shadowBytes = static_cast<std::size_t>(count * sizeof(float));
    float* shadowBuffer = nullptr;
    buildShadowMapKernel<<<blocks, threads, 0, stream>>>(
        device_density_grid,
        out,
        int3dim,
        make_float3(world_origin[0], world_origin[1], world_origin[2]),
        voxel_size,
        make_float3(light_position[0], light_position[1], light_position[2]),
        shadow_step,
        kappa
    );
    return cudaGetLastError();
}

// -------------------------------------------------------------
// A cuda demo
// -------------------------------------------------------------
// __global__ void thresholdKernel(
//     const nanovdb::NanoGrid<float>* grid,
//     float* output,
//     nanovdb::Coord first,
//     int count,
//     float threshold)
// {
//     const int i = blockIdx.x * blockDim.x + threadIdx.x;
//     if (i >= count) return;

//     // This example samples a line in NanoVDB index coordinates.
//     const nanovdb::Coord ijk(first.x() + i, first.y(), first.z());

//     auto accessor = grid->getAccessor();
//     const float density = accessor.getValue(ijk);

//     output[i] = density >= threshold ? density : 0.0f;
// }

// cudaError_t launchThreshold(
//     const nanovdb::NanoGrid<float>* deviceGrid,
//     float* deviceOutput,
//     nanovdb::Coord first,
//     int count,
//     float threshold,
//     cudaStream_t stream)
// {
//     constexpr int threads = 128;
//     const int blocks = (count + threads - 1) / threads;

//     thresholdKernel<<<blocks, threads, 0, stream>>>(
//         deviceGrid, deviceOutput, first, count, threshold);

//     return cudaGetLastError();
// }

} // end namespace lux
