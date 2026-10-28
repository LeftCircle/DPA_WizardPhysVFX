#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include <OpenImageIO/imageio.h>

#include "field_interface.h"
#include "usd_scene_render.h"

using namespace lux;

TEST_CASE("general USD renderer writes red and green volumes") {
    const auto left = isf_sphere(Vector(-1, 0, 0), 0.65f);
    const auto right = isf_sphere(Vector(1, 0, 0), 0.65f);
    const auto density = -mask(union_fields(left, right));
    const auto color = funcfield<Color>([left, right](const Vector& p) {
        if (left->eval(p) < 0) return Color(1, 0, 0, 0);
        if (right->eval(p) < 0) return Color(0, 1, 0, 0);
        return Color(0, 0, 0, 0);
    });

    RenderSettings rs;
    rs.width = 64;
    rs.height = 32;
    rs.near_dist = 0.1f;
    rs.far_dist = 12.0f;
    rs.min_step = 0.05f;
    rs.max_step = 0.05f;
    rs.kappa = 3.0f;
    rs.shadow_step = 0.2f;
    rs.set_density_vs(0.2f);
    rs.set_color_vs(0.2f);
    rs.set_shadow_vs(0.3f);
    Camera camera;
    camera.setEyeViewUp(Vector(0, 0, 6), Vector(0, 0, -1), Vector(0, 1, 0));
    camera.setFov(45);
    camera.setAspectRatio(2);
    std::vector<PointLight> lights = {
        PointLight(Color(1, 1, 1, 0), Vector(0, 3, 4), 0),
        PointLight(Color(0.5, 0.5, 0.5, 0), Vector(-3, 2, 2), 0)
    };

    const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto output = std::filesystem::temp_directory_path() /
        ("pbvfx_general_scene_" + std::to_string(stamp) + ".exr");
    const auto one_light_output = std::filesystem::temp_directory_path() /
        ("pbvfx_general_scene_one_light_" + std::to_string(stamp) + ".exr");
    struct RemoveFile {
        std::filesystem::path path;
        ~RemoveFile() { std::error_code ignored; std::filesystem::remove(path, ignored); }
    } remove_file{output}, remove_one_light{one_light_output};

    render_usd_scene(density, color, camera, lights, rs,
        Vector(-2, -1.5, -1.5), Vector(2, 1.5, 1.5), output.string());
    auto image = OIIO::ImageInput::open(output.string());
    REQUIRE(image);
    REQUIRE(image->spec().width == rs.width);
    REQUIRE(image->spec().height == rs.height);
    REQUIRE(image->spec().nchannels == 4);
    std::vector<float> pixels(static_cast<size_t>(rs.width) * rs.height * 4);
    REQUIRE(image->read_image(OIIO::TypeDesc::FLOAT, pixels.data()));
    image->close();
    size_t red_pixels = 0, green_pixels = 0;
    double two_light_sum = 0;
    for (size_t i = 0; i < pixels.size(); i += 4) {
        two_light_sum += pixels[i] + pixels[i + 1] + pixels[i + 2];
        if (pixels[i] > 1e-4f && pixels[i] > 2 * pixels[i + 1]) ++red_pixels;
        if (pixels[i + 1] > 1e-4f && pixels[i + 1] > 2 * pixels[i]) ++green_pixels;
    }
    REQUIRE(red_pixels > 0);
    REQUIRE(green_pixels > 0);

    render_usd_scene(density, color, camera, {lights[0]}, rs,
        Vector(-2, -1.5, -1.5), Vector(2, 1.5, 1.5), one_light_output.string());
    auto one_light_image = OIIO::ImageInput::open(one_light_output.string());
    REQUIRE(one_light_image);
    REQUIRE(one_light_image->read_image(OIIO::TypeDesc::FLOAT, pixels.data()));
    one_light_image->close();
    double one_light_sum = 0;
    for (size_t i = 0; i < pixels.size(); i += 4)
        one_light_sum += pixels[i] + pixels[i + 1] + pixels[i + 2];
    REQUIRE(one_light_sum > 0);
    REQUIRE(two_light_sum > 1.3 * one_light_sum);
}
