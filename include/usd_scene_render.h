#pragma once

#include <string>
#include <vector>
#include "Camera.h"
#include "field_interface.h"
#include "lights.h"
#include "render_settings.h"
#include "deep_shadow_map.h"

namespace lux {

// density must be negative inside the volume; bounds are in world units.
void render_usd_scene(const vspf& density, const vspc& color, const Camera& camera,
    const std::vector<PointLight>& lights, const RenderSettings& settings,
    const Vector& bounds_min, const Vector& bounds_max, const std::string& filename,
    const std::vector<ShadowRegion>& shadow_regions = {});

}
