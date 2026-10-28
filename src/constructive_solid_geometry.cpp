#include "constructive_solid_geometry.h"


using namespace lux;

const UnionField::volumeDataType UnionField::eval(const Vector& p) const {
    return std::min(this->_a->eval(p), std::get<0>(this->_values)->eval(p));
}

const UnionField::volumeGradType UnionField::grad(const Vector& p) const {
    if (this->_a->eval(p) > std::get<0>(this->_values)->eval(p)){
        return this->_a->grad(p);
    } else{
        return std::get<0>(this->_values)->grad(p);
    }
}

const IntersectionField::volumeDataType IntersectionField::eval(const Vector& p) const {
    return std::max(this->_a->eval(p), std::get<0>(this->_values)->eval(p));
}

const IntersectionField::volumeGradType IntersectionField::grad(const Vector& p) const { 
    if (this->_a->eval(p) < std::get<0>(this->_values)->eval(p)){
        return this->_a->grad(p);
    } else{
        return std::get<0>(this->_values)->grad(p);
    }
}

const CutoutField::volumeDataType CutoutField::eval(const Vector& p) const {
    return std::max(this->_a->eval(p), -std::get<0>(this->_values)->eval(p));
}

const CutoutField::volumeGradType CutoutField::grad(const Vector& p) const { 
    if (this->_a->eval(p) > -std::get<0>(this->_values)->eval(p)){
        return this->_a->grad(p);
    } else{
        return std::get<0>(this->_values)->grad(p);
    }
}

const MaskField::volumeDataType MaskField::eval(const Vector& p) const {
    return this->_a->eval(p) < 0 ? 1.0 : 0.0;
}

const ClampField::volumeDataType ClampField::eval(const Vector& p) const {
    auto eval = this->_a->eval(p);
    auto min = std::get<0>(this->_values)->eval(p);
    auto max = std::get<1>(this->_values)->eval(p);
    if (eval < min){
        return min;
    } else if (eval > max){
        return max;
    } else{
        return eval;
    }
}

const BlinnBlendField::volumeDataType BlinnBlendField::eval(const Vector& p) const {
    float eval = 0.0;
    for (const VolumeSPtr<float>& field : *_fields){
        eval += std::expf(field->eval(p) * _blend_factor);
    }
    return eval - _shape_broadness;
}