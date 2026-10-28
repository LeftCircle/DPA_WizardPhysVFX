#pragma once


#include "level_set_renders.h"
#include "ray_marcher_cuda.h"
#include "render_settings.h"
#include "deep_shadow_map_cuda.h"
#include "lights.h"
#include "cuda_helpers.h"

namespace lux{


void cuda_render_bunny(const std::string& path, const std::string& name);
void cuda_render_tests(const std::string& filename);
void profile_cuda();
void cuda_render_frame(
    const RenderSettings& rs,
    const GridPtr<float> dens_grid,
    const GridPtr<Color> col_grid,
    const std::vector<PointLight>& lights,
    const Camera& cam,
    const std::string& filename,
    const std::string label = ""
);

void cuda_render_frame_with_dsm(
    const RenderSettings& rs,
    const GridPtr<float> dens_grid,
    const GridPtr<Color> col_grid,
    const std::vector<GridPtr<float>>& shadow_grids,
    const std::vector<PointLight>& lights,
    const Camera& cam,
    const std::string& filename,
    const std::string label = ""
);

void render_wedges(
    std::string path,
    std::string name
);

void render_pyro_wedges(
    std::string path,
    std::string name
);

void render_wisp_wedges(
    std::string path,
    std::string name
);



} // end namespace lux
