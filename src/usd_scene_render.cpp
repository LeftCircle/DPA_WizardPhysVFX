#include "usd_scene_render.h"

#include <cmath>
#include <atomic>
#include <filesystem>
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
#include "progressive_sampling.h"
#include "sparse_grid_interpolation.h"

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

// Each pass is completely written before it replaces the watched preview.
void write_preview_atomic(const ImageData& image, const std::string& filename) {
    namespace fs = std::filesystem;
    static std::atomic<unsigned long long> serial{0};
    const fs::path destination(filename);
    const auto stamp = Clock::now().time_since_epoch().count();
    const auto temp = destination.parent_path() / ("." + destination.stem().string() +
        ".physvfx_" + std::to_string(stamp) + "_" + std::to_string(serial++) + destination.extension().string());
    try {
        write_image(image, temp.string());
        fs::rename(temp, destination);
    } catch (...) {
        std::error_code ignored;
        fs::remove(temp, ignored);
        throw;
    }
}

}

void render_usd_scene(const vspf& density, const vspc& color, const Camera& camera,
    const std::vector<PointLight>& lights, const RenderSettings& rs,
    const Vector& lo, const Vector& hi, const std::string& filename,
    const std::vector<ShadowRegion>& authored_regions,
    const std::vector<std::vector<GridPtr<float>>>& cached_shadow_grids,
    const std::string& preview_filename)
{
    const auto positive = [](float v) { return std::isfinite(v) && v > 0.0f; };
    if (!preview_filename.empty() && std::filesystem::absolute(preview_filename).lexically_normal() ==
                                     std::filesystem::absolute(filename).lexically_normal())
        throw std::invalid_argument("Preview and final image paths must differ");
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

    if (!cached_shadow_grids.empty()) {
        if (cached_shadow_grids.size() != regions.size())
            throw std::invalid_argument("Shadow cache region count mismatch");
        for (const auto& grids : cached_shadow_grids)
            if (grids.size() != lights.size())
                throw std::invalid_argument("Shadow cache light count mismatch");
    }

    RayMarcher marcher;
    marcher.set_snear(rs.near_dist);
    marcher.set_sfar(rs.far_dist);
    marcher.set_min_ds(rs.min_step);
    marcher.set_max_ds(rs.max_step);
    marcher.set_exticntion_coefficient(rs.kappa);
    marcher.set_Tmin(0.001f);
    auto start = Clock::now();
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
            auto grid = cached_shadow_grids.empty() ? nullptr : cached_shadow_grids[j][i];
            if (grid) {
                std::cout << "[PhysVFX] " << label << ", region " << (j+1) << ": using disk cache" << std::endl;
            } else {
                grid = make_dsm_grid_in_regions(density, rs.get_shadow_vs(), bounds, light, rs.shadow_step, light.kappa, regions);
            }
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
    if (preview_filename.empty()) {
        marcher.ray_march_image(camera, image, density, color);
    } else {
        // Keep the sparse ray-marched image separate from the interpolated
        // preview. Later passes must never treat interpolation as a real sample.
        ImageData preview(rs.width, rs.height, 4);
        const auto xs = progressive_axis_order(rs.width);
        const auto ys = progressive_axis_order(rs.height);
        marcher.ray_march_image_progressive(camera, image, density, color,
            [&](const ImageData& pass_image, int pass) {
                if (pass == 10) {
                    write_preview_atomic(pass_image, preview_filename);
                } else {
                    const int nx = progressive_axis_count(rs.width, pass);
                    const int ny = progressive_axis_count(rs.height, pass);
                    image_processing::interpolate_sparse_grid(pass_image, preview,
                        std::vector<int>(xs.begin(), xs.begin() + nx),
                        std::vector<int>(ys.begin(), ys.begin() + ny));
                    write_preview_atomic(preview, preview_filename);
                }
                std::cout << "[PhysVFX] Preview " << (pass * 10) << "%: " << preview_filename << std::endl;
            });
    }
    report_elapsed("Ray march image", start);
    std::cout << "[PhysVFX] Write image: starting" << std::endl;
    start = Clock::now();
    write_image(image, filename);
    report_elapsed("Write image", start);
}

}
