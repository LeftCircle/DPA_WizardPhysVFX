#pragma once

#include <tuple>

#include "openvdb_helpers.h"

namespace lux{


class RenderSettings{
public:
    RenderSettings();;
    RenderSettings(
        float model_hw,
        int _width,
        int _height,
        float _near_dist,
        float _far_dist,
        float _min_step,
        float _max_step,
        float _kappa,
        float _shadow_kappa,
        float _shadow_step,
        float density_vs,
        float color_vs,
        float sm_vs 
    );

    void set_density_vs(float vs);
    void set_color_vs(float vs);
    void set_shadow_vs(float vs);

    float get_density_vs() const { return _density_vs; }
    float get_color_vs() const { return _color_vs; }
    float get_shadow_vs() const { return _sm_vs; }


    const coordbbox& get_density_bounds() const { return _density_bounds; }
    const coordbbox& get_color_bounds() const { return _color_bounds; }
    const coordbbox& get_shadow_bounds() const { return _shadow_bounds; }

public:
    float model_halfsize = 4.0;
    int width = 1920;
    int height = 1080;
    float near_dist = 6;
    float far_dist = 15;
    float min_step = 0.01;
    float max_step = min_step * 5.0;
    float kappa = 3.0;
    float shadow_kappa = 3.0;
    float shadow_step = 0.01;
    float cam_dist = 10.0;

private:
    float _density_vs = 0.025;
    float _color_vs = 0.025;
    float _sm_vs = 0.025;
    coordbbox _density_bounds;
    coordbbox _color_bounds;
    coordbbox _shadow_bounds;

};

// returns the coordbbox for density, color, and shadow map as 0, 1, and 2
std::tuple<coordbbox, coordbbox, coordbbox> get_default_bounds(const RenderSettings& rs);




}
