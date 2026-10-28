#include "usd_sphere_render.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <openvdb/tools/LevelSetSphere.h>

#include "Camera.h"
#include "deep_shadow_map.h"
#include "image_data.h"
#include "lights.h"
#include "openvdb_helpers.h"
#include "ray_marcher.h"

namespace lux {

namespace {
// The legacy world_space_to_bounds helper centers its result on the origin.
// Preserve the actual world-space location for translated USD primitives.
coordbbox sphere_bounds(const Vector& lower, const Vector& upper, float voxel_size) {
    return coordbbox(
        openvdb::Coord(static_cast<int>(std::floor(lower.X() / voxel_size)),
                       static_cast<int>(std::floor(lower.Y() / voxel_size)),
                       static_cast<int>(std::floor(lower.Z() / voxel_size))),
        openvdb::Coord(static_cast<int>(std::ceil(upper.X() / voxel_size)),
                       static_cast<int>(std::ceil(upper.Y() / voxel_size)),
                       static_cast<int>(std::ceil(upper.Z() / voxel_size))));
}
}

void render_usd_sphere(const vspf& sphere, const Vector& center, float radius,
    const Vector& eye, const Vector& view, const Vector& up, float horizontal_fov,
    const Vector& light_position, const std::array<float, 3>& light_color,
    float light_shadow_kappa, const RenderSettings& rs, const std::string& filename)
{
    const auto positive = [](float v) { return std::isfinite(v) && v > 0.0f; };
    if (!sphere || !positive(radius) || rs.width <= 0 || rs.height <= 0 ||
        !positive(rs.get_density_vs()) || !positive(rs.get_color_vs()) ||
        !positive(rs.get_shadow_vs()) || !positive(rs.min_step) ||
        !positive(rs.max_step) || rs.max_step < rs.min_step ||
        !positive(rs.shadow_step) || !positive(rs.kappa) ||
        !std::isfinite(rs.near_dist) || rs.near_dist < 0 ||
        !std::isfinite(rs.far_dist) || rs.far_dist <= rs.near_dist ||
        !std::isfinite(light_shadow_kappa) || light_shadow_kappa < 0 ||
        !positive(horizontal_fov) || horizontal_fov >= 180 || filename.empty()) {
        throw std::invalid_argument("Invalid sphere, camera, or render settings");
    }

    openvdb::initialize();
    const auto padding = 2.0f * std::max({rs.get_density_vs(), rs.get_color_vs(), rs.get_shadow_vs()});
    const float extent = radius + padding;
    const Vector lower = center - Vector(extent, extent, extent);
    const Vector upper = center + Vector(extent, extent, extent);

    // RayMarcher and deep shadow maps use negative values for density.
    const auto density = -mask(sphere);
    const auto density_bounds = sphere_bounds(lower, upper, rs.get_density_vs());
    auto density_grid = stamp_isf_to_grid(rs.get_density_vs(), 0.0f, density, density_bounds);
    const auto color_bounds = sphere_bounds(lower, upper, rs.get_color_vs());
    auto color_grid = stamp_color_to_grid(make_constant(Color(1, 1, 1, 0)), color_bounds, rs.get_color_vs());
    auto density_field = make_grid_field<float>(density_grid);
    auto color_field = make_grid_field<Color>(color_grid);

    PointLight light(Color(light_color[0], light_color[1], light_color[2], 0),
                     light_position, light_shadow_kappa);
    const auto shadow_bounds = sphere_bounds(lower, upper, rs.get_shadow_vs());
    auto shadow = make_deep_shadow_map_parallel(density_field, rs.get_shadow_vs(),
        shadow_bounds, light, rs.shadow_step, light.kappa);

    // The sphere's narrow-band level set is used only for ray acceleration.
    auto levelset = openvdb::tools::createLevelSetSphere<openvdb::FloatGrid>(
        radius, openvdb::Vec3f(center.X(), center.Y(), center.Z()), rs.get_density_vs());
    Camera cam;
    cam.setEyeViewUp(eye, view, up);
    cam.setFov(horizontal_fov);
    cam.setAspectRatio(static_cast<double>(rs.width) / rs.height);

    RayMarcher marcher;
    marcher.set_snear(rs.near_dist);
    marcher.set_sfar(rs.far_dist);
    marcher.set_min_ds(rs.min_step);
    marcher.set_max_ds(rs.max_step);
    marcher.set_exticntion_coefficient(rs.kappa);
    marcher.set_Tmin(0.001f);
    marcher.add_levelset(levelset);
    marcher.add_shadow_map(shadow, light.color);

    ImageData image(rs.width, rs.height, 4);
    marcher.ray_march_image(cam, image, density_field, color_field);
    // ImageData::oiio_write_to only prints errors. Propagate them to the CLI.
    auto output = OIIO::ImageOutput::create(filename);
    if (!output) throw std::runtime_error("Could not create image output: " + filename);
    OIIO::ImageSpec spec(rs.width, rs.height, 4, OIIO::TypeDesc::FLOAT);
    if (!output->open(filename, spec)) {
        throw std::runtime_error("Could not open image output: " + filename + ": " + output->geterror());
    }
    if (!output->write_image(OIIO::TypeDesc::FLOAT, &image.get_pixel_value(0, 0, 0))) {
        throw std::runtime_error("Could not write image: " + filename + ": " + output->geterror());
    }
    if (!output->close()) throw std::runtime_error("Could not close image: " + filename);
}

}
