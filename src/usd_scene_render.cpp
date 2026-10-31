#include "usd_scene_render.h"

#include <cmath>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "deep_shadow_map.h"
#include "image_data.h"
#include "openvdb_helpers.h"
#include "ray_marcher.h"

namespace lux {
namespace {

using Clock = std::chrono::steady_clock;

void report_elapsed(const std::string& label, Clock::time_point start) {
    const double seconds = std::chrono::duration<double>(Clock::now() - start).count();
    std::ostringstream message;
    message << "[PhysVFX] " << label << ": completed in "
            << std::fixed << std::setprecision(2) << seconds << "s";
    std::cout << message.str() << std::endl;
}

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
    const Vector& lo, const Vector& hi, const std::string& filename,
    const std::vector<ShadowRegion>& authored_regions)
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
    const auto regions = authored_regions.empty() ? std::vector<ShadowRegion>{{lo, hi}} : authored_regions;
    for (const auto& region : regions) {
        if (!finite_vector(region.first) || !finite_vector(region.second) ||
            region.first.X() >= region.second.X() || region.first.Y() >= region.second.Y() ||
            region.first.Z() >= region.second.Z()) throw std::invalid_argument("Invalid shadow region");
    }

    RayMarcher marcher;
    marcher.set_snear(rs.near_dist);
    marcher.set_sfar(rs.far_dist);
    marcher.set_min_ds(rs.min_step);
    marcher.set_max_ds(rs.max_step);
    marcher.set_exticntion_coefficient(rs.kappa);
    marcher.set_Tmin(0.001f);
    std::cout << "[PhysVFX] Build render bounds: starting" << std::endl;
    auto start = Clock::now();
    marcher.add_levelset(make_bounds_levelset(lo, hi, rs.get_density_vs()));
    report_elapsed("Build render bounds", start);
    for (size_t i = 0; i < lights.size(); ++i) {
        const auto& light = lights[i];
        const auto label = "Shadow map " + std::to_string(i + 1) + "/" + std::to_string(lights.size());
        std::cout << "[PhysVFX] " << label << ": starting" << std::endl;
        start = Clock::now();
        std::vector<vspf> maps;
        for (size_t j = 0; j < regions.size(); ++j) {
            std::cout << "[PhysVFX] " << label << ", region " << (j+1) << "/" << regions.size() << ": starting" << std::endl;
            auto region_start = Clock::now();
            auto bounds = world_space_to_bounds(regions[j].first, regions[j].second, rs.get_shadow_vs());
            auto grid = make_dsm_grid_in_regions(density, rs.get_shadow_vs(), bounds, light, rs.shadow_step, light.kappa, regions);
            maps.push_back(make_deep_shadow_map(grid));
            report_elapsed(label + ", region " + std::to_string(j+1), region_start);
        }
        auto shadow = funcfield<float>([maps, regions](const Vector& p) {
            for (size_t j = 0; j < regions.size(); ++j) {
                const auto& a = regions[j].first; const auto& b = regions[j].second;
                if (p.X() >= a.X() && p.X() <= b.X() && p.Y() >= a.Y() && p.Y() <= b.Y() && p.Z() >= a.Z() && p.Z() <= b.Z())
                    return maps[j]->eval(p);
            }
            return 1.0f;
        });
        marcher.add_shadow_map(shadow, light.color);
        report_elapsed(label, start);
    }

    ImageData image(rs.width, rs.height, 4);
    std::cout << "[PhysVFX] Ray march image: starting" << std::endl;
    start = Clock::now();
    marcher.ray_march_image(camera, image, density, color);
    report_elapsed("Ray march image", start);
    std::cout << "[PhysVFX] Write image: starting" << std::endl;
    start = Clock::now();
    write_image(image, filename);
    report_elapsed("Write image", start);
}

}
