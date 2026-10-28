#pragma once

#include <vector>

#include "cuda_helpers.h"
#include "ray_marcher_cuda.h"
#include "openvdb_helpers.h"
#include "lights.h"
#include "render_settings.h"

namespace lux{

auto _get_shadow_map_as_dense_vector(
    const nanovdb::NanoGrid<float>* density_nano,
    const coordbbox& sm_bounds,
    const Vector& light_pos,
    float vs,
    float ray_step_size,
    float kappa,
    cudaStream_t stream
);


GridPtr<float> _stamp_dense_vector_to_grid(
    const std::vector<float>& shadow_vec,
    const coordbbox& bounds,
    float sm_vs
);

GridPtr<float> deep_shadow_map_cuda(
    const nanovdb::NanoGrid<float>* density_nano,
    const coordbbox& sm_bounds,
    const Vector& light_pos,
    float vs,
    float ray_step_size,
    float kappa,
    cudaStream_t stream
);

const nanovdb::NanoGrid<float>** create_shadow_maps_and_send_to_gpu(
    const nanovdb::NanoGrid<float>* density,
    const std::vector<PointLight>& lights,
    const RenderSettings& rs,
    cudaStream_t stream
);


}