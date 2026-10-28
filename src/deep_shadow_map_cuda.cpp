#include "deep_shadow_map_cuda.h"




namespace lux{

auto _get_shadow_map_as_dense_vector(
    const nanovdb::NanoGrid<float>* density_nano,
    const coordbbox& sm_bounds,
    const Vector& light_pos,
    float vs,
    float ray_step_size,
    float kappa,
    cudaStream_t stream
){
    const auto lo = sm_bounds.getStart();
    const auto hi = sm_bounds.getEnd(); // inclusive

    const int nx = hi[0] - lo[0] + 1;
    const int ny = hi[1] - lo[1] + 1;
    const int nz = hi[2] - lo[2] + 1;
    const float3 origin = make_float3(lo[0] * vs, lo[1] * vs, lo[2] * vs);
    const int count = nx * ny * nz;
    const std::size_t shadowBytes = static_cast<std::size_t>(count * sizeof(float));
    float* shadowBuffer = nullptr;
    CUDA_CHECK(cudaMalloc(&shadowBuffer, shadowBytes));
    printf("Starting cuda shadow map\n");
    CUDA_CHECK( launch_cuda_shadowmap(
        density_nano,
        shadowBuffer,
        sm_bounds,
        {origin.x, origin.y, origin.z},
        vs,
        light_pos,
        ray_step_size,
        kappa,
        stream
    ));
    CUDA_CHECK(cudaStreamSynchronize(stream));
    printf("Shadow map completed. Copying\n");
    
    std::vector<float> host_shadow(count);
    CUDA_CHECK(cudaMemcpy(
        host_shadow.data(),
        shadowBuffer,
        shadowBytes,
        cudaMemcpyDeviceToHost
    ));
    CUDA_CHECK(cudaFree(shadowBuffer));
    return host_shadow;
}

GridPtr<float> _stamp_dense_vector_to_grid(
    const std::vector<float>& shadow_vec,
    const coordbbox& bounds,
    float sm_vs
){
    auto lo = bounds.getStart();
    auto dims = bounds.dim();
    int nx = dims.x();
    int ny = dims.y();
    auto value_func = [lo, sm_vs, nx, ny, &shadow_vec](const Vector& p) -> float {
        const int x = static_cast<int>(std::round(p.x() / sm_vs)) - lo.x();
        const int y = static_cast<int>(std::round(p.y() / sm_vs)) - lo.y();
        const int z = static_cast<int>(std::round(p.z() / sm_vs)) - lo.z();

        const std::size_t index = (std::size_t(z) * ny + y) * nx + x;
        return shadow_vec[index];
    };

    auto condition_func = [](float value) {
        return std::abs(value) > 1e-6f;
    };

   return stamp_grid_parallel<float>(
        sm_vs,
        0.0f,
        value_func,
        condition_func,
        bounds
    );
}

GridPtr<float> deep_shadow_map_cuda(
    const nanovdb::NanoGrid<float>* density_nano,
    const coordbbox& sm_bounds,
    const Vector& light_pos,
    float vs,
    float ray_step_size,
    float kappa,
    cudaStream_t stream
){
    auto shadow_vec = _get_shadow_map_as_dense_vector(
        density_nano,
        sm_bounds,
        light_pos,
        vs,
        ray_step_size,
        kappa,
        stream
    );
    return _stamp_dense_vector_to_grid(shadow_vec, sm_bounds, vs);

}

const nanovdb::NanoGrid<float>** create_shadow_maps_and_send_to_gpu(
    const nanovdb::NanoGrid<float>* density,
    const std::vector<PointLight>& lights,
    const RenderSettings& rs,
    cudaStream_t stream
){
    using ShadowHandle = nanovdb::GridHandle<nanovdb::CudaDeviceBuffer>;
    std::vector<ShadowHandle> shadow_handles;
    shadow_handles.reserve(lights.size());
    for (const auto& light : lights){
        auto grid = deep_shadow_map_cuda(
            density,
            rs.get_shadow_bounds(),
            light.position,
            rs.get_shadow_vs(),
            rs.shadow_step,
            light.kappa,
            stream
        );
        shadow_handles.push_back(pack_grid_to_nano(grid, stream));
    }

    std::vector<const nanovdb::NanoGrid<float>*> host_shadow_ptrs;
    host_shadow_ptrs.reserve(shadow_handles.size());

    for (auto& handle : shadow_handles) {
        host_shadow_ptrs.push_back(handle.deviceGrid<float>());
    }

    return send_array_to_gpu<const nanovdb::NanoGrid<float>*>(
            host_shadow_ptrs.data(),
            host_shadow_ptrs.size()
    );
}


} // end namespace lux

