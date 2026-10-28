#include "render_settings.h"


namespace lux{

RenderSettings::RenderSettings(){
    set_density_vs(_density_vs); set_color_vs(_color_vs); set_shadow_vs(_sm_vs);
}

RenderSettings::RenderSettings(
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
) : model_halfsize(model_hw), width(_width), height(_height), near_dist(_near_dist), far_dist(_far_dist),
    min_step(_min_step), max_step(_max_step), kappa(_kappa), shadow_kappa(_shadow_kappa), shadow_step(_shadow_step)
{
        set_density_vs(density_vs); set_color_vs(color_vs); set_shadow_vs(sm_vs);
}


void RenderSettings::set_density_vs(float vs){
    _density_vs = vs;
    const auto hw = model_halfsize;
    _density_bounds = world_space_to_bounds({-hw, -hw, -hw}, {hw, hw, hw}, _density_vs);
}

void RenderSettings::set_color_vs(float vs){
    _color_vs = vs;
    const auto hw = model_halfsize;
    _color_bounds = world_space_to_bounds({-hw, -hw, -hw}, {hw, hw, hw}, _color_vs);
}

void RenderSettings::set_shadow_vs(float vs){
    _sm_vs = vs;
    const auto hw = model_halfsize;
    _shadow_bounds = world_space_to_bounds({-hw, -hw, -hw}, {hw, hw, hw}, _sm_vs);
}


} // end namespace lux