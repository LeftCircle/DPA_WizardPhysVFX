#include "implicit_fields.h"
#include <algorithm>
#include <cmath>


using namespace lux;


const TorusField::volumeDataType TorusField::eval(const Vector& p) const {
    auto v = p - _center;
    float term1 = (v).magnitude_squared() + _r_major_sq - _r_minor_sq;
    return term1 * term1 - (4.0 * _r_major_sq * (v - (v*_n_hat) * _n_hat).magnitude_squared());
}


namespace {
struct ConeDistance { double da, dr, dist, sign; bool side; };
ConeDistance cone_distance(double a, double r, double height, double theta) {
    const double radius = height * std::tan(theta);
    const double t = std::clamp((a * height + r * radius) / (height * height + radius * radius), 0.0, 1.0);
    const double sa = a - t * height, sr = r - t * radius;
    const double ca = a - height, cr = r - std::min(r, radius);
    const bool side = sa*sa + sr*sr <= ca*ca + cr*cr;
    const double da = side ? sa : ca, dr = side ? sr : cr;
    const bool inside = a >= 0 && a <= height && r <= a * std::tan(theta);
    return {da, dr, std::hypot(da, dr), inside ? -1.0 : 1.0, side};
}
}

const ConeField::volumeDataType ConeField::eval(const Vector& p) const {
    const Vector v = p - _x_0;
    const double a = v * _n_hat;
    const double r = (v - a * _n_hat).magnitude();
    const auto d = cone_distance(a, r, _height, _theta);
    return static_cast<float>(d.sign * d.dist);
}

const ConeField::volumeGradType ConeField::grad(const Vector& p) const {
    const Vector v = p - _x_0;
    const double a = v * _n_hat;
    const Vector radial = v - a * _n_hat;
    const double r = radial.magnitude();
    const auto d = cone_distance(a, r, _height, _theta);
    if (d.dist < 1e-12) {
        if (r < 1e-12) return d.side ? Vector(0, 0, 0) : _n_hat;
        return d.side ? -std::sin(_theta) * _n_hat + std::cos(_theta) * (radial / r) : _n_hat;
    }
    return (d.sign / d.dist) * (d.da * _n_hat + (r > 1e-12 ? (d.dr / r) * radial : Vector(0, 0, 0)));
}


const BoxField::volumeDataType BoxField::eval(const Vector& p) const {
    auto v = p - _center;
    return std::pow(v.X(), _rounding_exponent) + std::pow(v.Y(), _rounding_exponent) + 
            std::pow(v.Z(), _rounding_exponent) - _r_to_rounding_exp;
}

const IcosahedronField::volumeDataType IcosahedronField::eval(const Vector& p) const {
    auto v = p - _center;
    if (v.magnitude() > _pi_times_1p8){
        return _pi_times_1p8;
    } else {
        auto tx = v.X() * _T;
        auto ty = v.Y() * _T;
        auto tz = v.Z() * _T;
        return 2.0 - std::cos(v.X() + ty) - std::cos(v.X() - ty) - 
                std::cos(v.Y() + tz) - std::cos(v.Y() - tz) - 
                std::cos(v.Z() + tx) - std::cos(v.Z() - tx);
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
