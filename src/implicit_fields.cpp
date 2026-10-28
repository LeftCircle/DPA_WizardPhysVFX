#include "implicit_fields.h"


using namespace lux;


const TorusField::volumeDataType TorusField::eval(const Vector& p) const {
    auto v = p - _center;
    float term1 = (v).magnitude_squared() + _r_major_sq - _r_minor_sq;
    return term1 * term1 - (4.0 * _r_major_sq * (v - (v*_n_hat) * _n_hat).magnitude_squared());
}


const ConeField::volumeDataType ConeField::eval(const Vector& p) const {
    ConeField::volumeDataType return_val = {};
    auto diff_dot_n = (p - _x_0) * _n_hat;
    if (p == _x_0){
        return_val = {};
    } else if (diff_dot_n > _height){
        return_val = diff_dot_n - _height; 
    } else if (diff_dot_n < 0){
        return_val = -diff_dot_n;
    } else {
        return_val = -_theta + std::acosf(diff_dot_n / (p - _x_0).magnitude());
    }
    return return_val;
}


const BoxField::volumeDataType BoxField::eval(const Vector& p) const {
    auto v = p - _center;
    return std::powf(v.X(), _rounding_exponent) + std::powf(v.Y(), _rounding_exponent) + 
            std::powf(v.Z(), _rounding_exponent) - _r_to_rounding_exp;
}

const IcosahedronField::volumeDataType IcosahedronField::eval(const Vector& p) const {
    auto v = p - _center;
    if (v.magnitude() > _pi_times_1p8){
        return _pi_times_1p8;
    } else {
        auto tx = v.X() * _T;
        auto ty = v.Y() * _T;
        auto tz = v.Z() * _T;
        return 2.0 - std::cosf(v.X() + ty) - std::cosf(v.X() - ty) - 
                std::cosf(v.Y() + tz) - std::cosf(v.Y() - tz) - 
                std::cosf(v.Z() + tx) - std::cosf(v.Z() - tx);
    }
}

const SteinerPatchField::volumeDataType SteinerPatchField::eval(const Vector& p) const {
    auto v = p - _center;
    auto xx = v.X() * v.X();
    auto yy = v.Y() * v.Y();
    auto zz = v.Z() * v.Z();
    return xx*yy + xx * zz + yy * zz - v.X() * v.Y() * v.Z();
}

const EllipseField::volumeDataType EllipseField::eval(const Vector& p ) const{
    auto v = p - _center;
    auto Z = v * _normal;
    auto xperp = v - Z * _normal;
    return Z * Z / _r_major_sq + xperp.magnitude_squared() / _r_minor_sq - 1.0;
}

const CylinderField::volumeDataType CylinderField::eval(const Vector& p) const {
    auto v = p - _center;
    float infinite_cyl = (v - (v * _normal) * _normal).magnitude() - _radius;
    // now we have to cutout with the planes
    //std::max(this->_a->eval(p), -std::get<0>(this->_values)->eval(p))
    float cutout_p1 = std::max(infinite_cyl, -_plane1.eval(p));
    float cutout_p2 = std::max(cutout_p1, -_plane2.eval(p));
    return cutout_p2;
}