#pragma once


#include <string>
#include <iostream>
#include <memory>

#include <openvdb/openvdb.h>
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/VolumeToMesh.h>

#include "field_interface.h"
#include "ray_marcher.h"
#include "image_data.h"
#include "command_line_parser.h"

#include "cuda_helpers.h"
#include "openvdb_helpers.h"
#include "humanoid.h"
#include "render_settings.h"

#include "string_funcs.h"


#include "obj_reader.h"

namespace lux{

#define RADTODEG(radians) ((radians) * 180.0f / 3.14159265358979)
#define DEGTORAD(degrees) ((degrees * 3.14159265358979 / 180.0))

using vecsptr = std::shared_ptr<const std::vector<vspf>>;

struct RenderInfo{
    RenderInfo(
        vspf dens,
        vspf masked_dens,
        vspc col,
        float sm_k,
        float k
    ) : density(dens), masked_density(masked_dens), color(col), sm_kappa(sm_k), kappa(k) {}
    vspf density;
    vspf masked_density;
    vspc color;
    float sm_kappa;
    float kappa;
};

void render_bunny(const std::string& path, const std::string& name);
RenderInfo bunny_info();

void render_bust(const std::string& path, const std::string& name);
RenderInfo bust_info();

void render_bunny_in_ajax(const std::string& path, const std::string& name);
RenderInfo bunnybust_info();

void render_humanoid_with_bunnybust(const std::string& path, const std::string& name);
void write_out_humanoid_bunnybust(const std::string& path, const std::string& name);

void render_turnable(
    const std::string& path,
    const std::string& name, 
    const vspf& density,
    const vspf& masked_density,
    const vspc& color,
    float sm_kappa,
    float kappa,
    float t = 0
);

void cpu_render_frame(
    const RenderSettings& rs,
    const GridPtr<float> dens_grid,
    const GridPtr<Color> col_grid,
    const std::vector<GridPtr<float>>& shadow_grids,
    const std::vector<PointLight>& lights,
    const Camera& cam,
    const std::string& filename,
    const std::string& label = ""
);


} // end namespace lux
