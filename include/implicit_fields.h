#pragma once

#include "Vector.h"
#include "volume.h"
#include "cmath"
#include "constructive_solid_geometry.h"

namespace lux{


template<typename T>
class ConstantField : public Volume<T> 
{
public:
    using typename Volume<T>::volumeDataType;
    using typename Volume<T>::volumeGradType;

    ConstantField( const T v ) : _value(std::move(v)) {}
    ~ConstantField() = default;
    
    const volumeDataType eval( const Vector& P ) const override { return _value; }
    const volumeGradType grad( const Vector& P ) const override { return {}; }
    virtual std::string typelabel() { return "Constant"; }
    
private:
    
    volumeDataType _value;
    volumeGradType _gradvalue;
};

class SphereField : public Volume<float>{
public:
    using typename Volume<float>::volumeDataType;
    using typename Volume<float>::volumeGradType;

    SphereField(const Vector& center, const float radius) : _center(center), _radius(radius) {};
    ~SphereField() = default;
    
    const volumeDataType eval( const Vector& p ) const override { return (p - _center).magnitude() - _radius; }
    //const volumeGradType grad( const Vector& p ) const override { return {}; }
    virtual std::string typelabel() { return "Sphere"; }

private:
    Vector _center;
    float _radius;

};

class TorusField : public Volume<float>{
public:
    using volumeDataType = typename Volume<float>::volumeDataType;
    using volumeGradType = typename Volume<float>::volumeGradType;

    TorusField(const Vector& center, float r_major, float r_minor, const Vector& n_hat) : 
            _center(center), _r_major_sq(r_major * r_major), _r_minor_sq(r_minor * r_minor), _n_hat(n_hat.unitvector()) {};
    ~TorusField() = default;
    
    const volumeDataType eval( const Vector& p ) const override;
    //const volumeGradType grad( const Vector& p ) const override { return {}; }
    virtual std::string typelabel() { return "Torus"; }

private:
    Vector _center;
    Vector _n_hat;
    float _r_major_sq;
    float _r_minor_sq;
};


class ConeField : public Volume<float>{
public:
    using volumeDataType = typename Volume<float>::volumeDataType;
    using volumeGradType = typename Volume<float>::volumeGradType;

    ConeField(const Vector& x_0, const Vector& n_hat, float height, float theta) : 
            _x_0(x_0), _n_hat(n_hat), _height(height), _theta(theta) {};
    ~ConeField() = default;
    
    const volumeDataType eval( const Vector& p ) const override;
    //const volumeGradType grad( const Vector& p ) const override { return {}; }
    virtual std::string typelabel() { return "Cone"; }

private:
    Vector _x_0;
    Vector _n_hat;
    float _height;
    float _theta;
};

class BoxField : public Volume<float>{
public:
    using volumeDataType = typename Volume<float>::volumeDataType;
    using volumeGradType = typename Volume<float>::volumeGradType;

    BoxField(const Vector& center, float radius, int rounding_exponent) : 
            _center(center), _radius(radius), _rounding_exponent( 2 * rounding_exponent),
            _r_to_rounding_exp(std::powf(radius, 2 * rounding_exponent)) {};
    ~BoxField() = default;
    
    const volumeDataType eval( const Vector& p ) const override;
    //const volumeGradType grad( const Vector& p ) const override { return {}; }
    virtual std::string typelabel() { return "Box"; }

private:
    Vector _center;
    float _radius;
    float _r_to_rounding_exp;
    int _rounding_exponent;
};

class IcosahedronField : public Volume<float>{
public:
    using volumeDataType = typename Volume<float>::volumeDataType;
    using volumeGradType = typename Volume<float>::volumeGradType;

    IcosahedronField(const Vector& center) : 
            _center(center) {};
    ~IcosahedronField() = default;
    
    const volumeDataType eval( const Vector& p ) const override;
    //const volumeGradType grad( const Vector& p ) const override { return {}; }
    virtual std::string typelabel() { return "Icosahedron"; }

private:
    Vector _center;
    const float _T = 1.61803399;
    const float _pi_times_1p8 = 3.14159265358979 * 1.8; 
};

class SteinerPatchField : public Volume<float>{
public:
    using volumeDataType = typename Volume<float>::volumeDataType;
    using volumeGradType = typename Volume<float>::volumeGradType;

    SteinerPatchField(const Vector& center) : 
            _center(center) {};
    ~SteinerPatchField() = default;
    
    const volumeDataType eval( const Vector& p ) const override;
    //const volumeGradType grad( const Vector& p ) const override { return {}; }
    virtual std::string typelabel() { return "SteinerPatch"; }

private:
    Vector _center;
};

class EllipseField : public Volume<float>{
public:
    using volumeDataType = typename Volume<float>::volumeDataType;
    using volumeGradType = typename Volume<float>::volumeGradType;

    EllipseField(const Vector& center, const Vector& normal, float r_major, float r_minor) : 
            _center(center), _normal(normal), _r_major_sq(r_major * r_major), _r_minor_sq(r_minor * r_minor)  {};
    ~EllipseField() = default;
    
    const volumeDataType eval( const Vector& p ) const override;
    //const volumeGradType grad( const Vector& p ) const override { return {}; }
    virtual std::string typelabel() { return "Ellipse"; }

private:
    Vector _center;
    Vector _normal;
    float _r_major_sq;
    float _r_minor_sq;
};

class PlaneField : public Volume<float>{
public:
	using typename Volume<float>::volumeDataType;
	using typename Volume<float>::volumeGradType;
    PlaneField() = default;
	PlaneField(const Vector& point, const Vector& normal) : _point(point), _normal(normal.unitvector()) {}
	PlaneField(Vector&& point, Vector&& normal) :
		_point(std::move(point)), _normal(std::move(normal)) {}

	const volumeDataType eval( const Vector& P ) const override { return _normal * (P - _point); }
    const volumeGradType grad( const Vector& P ) const override { return _normal; }
    virtual std::string typelabel() { return "Plane"; }

private:
	Vector _point;
	Vector _normal;
};

class CylinderField : public Volume<float>{
public:
    using volumeDataType = typename Volume<float>::volumeDataType;
    using volumeGradType = typename Volume<float>::volumeGradType;

    CylinderField(const Vector& center, const Vector& normal, float radius, float height) : 
            _center(center), _normal(normal), _radius(radius) {
                float halfh = height * 0.5;
                _plane1 = PlaneField(center + halfh * normal, -normal);
                _plane2 = PlaneField(center - halfh * normal, normal);
            };
    ~CylinderField() = default;
    
    const volumeDataType eval( const Vector& p ) const override;
    //const volumeGradType grad( const Vector& p ) const override { return {}; }
    virtual std::string typelabel() { return "Cylinder"; }

private:
    Vector _center;
    Vector _normal;
    float _radius;
    PlaneField _plane1;
    PlaneField _plane2;
};


} // end namespace lux