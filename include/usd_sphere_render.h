#pragma once

#include <array>
#include <string>
#include "field_interface.h"
#include "render_settings.h"

namespace lux {

// Render one sphere field. The center and radius also define the acceleration
// grid and the finite stamping bounds; they must describe the supplied field.
void render_usd_sphere(const vspf& sphere, const Vector& center, float radius,
    const Vector& eye, const Vector& view, const Vector& up, float horizontal_fov,
    const Vector& light_position, const std::array<float, 3>& light_color,
    float light_shadow_kappa, const RenderSettings& settings,
    const std::string& filename);

}
