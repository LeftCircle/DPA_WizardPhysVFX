#include "usd_scene_render.h"

#include <cmath>
#include <stdexcept>
#include <vector>

#include "deep_shadow_map.h"
#include "image_data.h"
#include "openvdb_helpers.h"
#include "ray_marcher.h"

namespace lux {
namespace {

bool finite_vector(const Vector& v) {
    return std::isfinite(v.X()) && std::isfinite(v.Y()) && std::isfinite(v.Z());
}

// The existing marcher expects a level set for acceleration. A conservative
// box encloses all fields; density is still evaluated from the actual scene.
openvdb::FloatGrid::Ptr make_bounds_levelset(const Vector& lo, const Vector& hi, float voxel_size) {
    const std::vector<openvdb::Vec3s> points = {
        {float(lo.X()), float(lo.Y()), float(lo.Z())}, {float(hi.X()), float(lo.Y()), float(lo.Z())},
        {float(hi.X()), float(hi.Y()), float(lo.Z())}, {float(lo.X()), float(hi.Y()), float(lo.Z())},
        {float(lo.X()), float(lo.Y()), float(hi.Z())}, {float(hi.X()), float(lo.Y()), float(hi.Z())},
        {float(hi.X()), float(hi.Y()), float(hi.Z())}, {float(lo.X()), float(hi.Y()), float(hi.Z())}
    };
    const std::vector<openvdb::Vec4I> faces = {
        {0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
        {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}
    };
    return mesh_to_levelset(voxel_size, points, {}, faces, 3.0f);
}

void write_image(const ImageData& image, const std::string& filename) {
    auto output = OIIO::ImageOutput::create(filename);
    if (!output) throw std::runtime_error("Could not create image output: " + filename);
    OIIO::ImageSpec spec(image.get_width(), image.get_height(), 4, OIIO::TypeDesc::FLOAT);
    if (!output->open(filename, spec))
        throw std::runtime_error("Could not open image output: " + filename + ": " + output->geterror());
    if (!output->write_image(OIIO::TypeDesc::FLOAT, &image.get_pixel_value(0, 0, 0)))
        throw std::runtime_error("Could not write image: " + filename + ": " + output->geterror());
    if (!output->close()) throw std::runtime_error("Could not close image: " + filename);
}

}

void render_usd_scene(const vspf& density, const vspc& color, const Camera& camera,
    const std::vector<PointLight>& lights, const RenderSettings& rs,
    const Vector& lo, const Vector& hi, const std::string& filename)
{
    const auto positive = [](float v) { return std::isfinite(v) && v > 0.0f; };
    if (!density || !color || filename.empty() || rs.width <= 0 || rs.height <= 0 ||
        !finite_vector(lo) || !finite_vector(hi) || lo.X() >= hi.X() || lo.Y() >= hi.Y() || lo.Z() >= hi.Z() ||
        !positive(rs.get_density_vs()) || !positive(rs.get_color_vs()) || !positive(rs.get_shadow_vs()) ||
        !positive(rs.min_step) || !positive(rs.max_step) || rs.max_step < rs.min_step ||
        !positive(rs.shadow_step) || !positive(rs.kappa) ||
        !std::isfinite(rs.near_dist) || rs.near_dist < 0 || !std::isfinite(rs.far_dist) || rs.far_dist <= rs.near_dist) {
        throw std::invalid_argument("Invalid scene fields, bounds, or render settings");
    }
    for (const auto& light : lights) {
        if (!finite_vector(light.position) || !std::isfinite(light.kappa) || light.kappa < 0 ||
            !std::isfinite(light.color.red()) || !std::isfinite(light.color.green()) || !std::isfinite(light.color.blue()) ||
            light.color.red() < 0 || light.color.green() < 0 || light.color.blue() < 0) {
            throw std::invalid_argument("Invalid point light");
        }
    }

    openvdb::initialize();
    const auto density_bounds = world_space_to_bounds(lo, hi, rs.get_density_vs());
    const auto color_bounds = world_space_to_bounds(lo, hi, rs.get_color_vs());
    const auto shadow_bounds = world_space_to_bounds(lo, hi, rs.get_shadow_vs());
    auto density_grid = stamp_isf_to_grid(rs.get_density_vs(), 0.0f, density, density_bounds);
    auto color_grid = stamp_color_to_grid(color, color_bounds, rs.get_color_vs());
    auto density_field = make_grid_field<float>(density_grid);
    auto color_field = make_grid_field<Color>(color_grid);

    RayMarcher marcher;
    marcher.set_snear(rs.near_dist);
    marcher.set_sfar(rs.far_dist);
    marcher.set_min_ds(rs.min_step);
    marcher.set_max_ds(rs.max_step);
    marcher.set_exticntion_coefficient(rs.kappa);
    marcher.set_Tmin(0.001f);
    marcher.add_levelset(make_bounds_levelset(lo, hi, rs.get_density_vs()));
    for (const auto& light : lights) {
        auto shadow = make_deep_shadow_map_parallel(density_field, rs.get_shadow_vs(),
            shadow_bounds, light, rs.shadow_step, light.kappa);
        marcher.add_shadow_map(shadow, light.color);
    }

    ImageData image(rs.width, rs.height, 4);
    marcher.ray_march_image(camera, image, density_field, color_field);
    write_image(image, filename);
}

}
