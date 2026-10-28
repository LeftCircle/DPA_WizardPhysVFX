#pragma once

#include "Color.h"


namespace lux{

struct PointLight{
    PointLight(Color _color, Vector _pos, float _kappa = 1.0) : 
        color(_color), position(_pos), kappa(_kappa) {}
    Color color;
    Vector position;
    float kappa;
};




} // end namespcae lux