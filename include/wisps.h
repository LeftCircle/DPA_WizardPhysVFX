#pragma once

#include <random>

#include "Vector.h"
#include "Color.h"
#include "fractal_sum_noise.h"
#include "noise_fields.h"
#include "openvdb_helpers.h"

namespace lux{

struct CoordinateSys{
    Vector t = {1, 0, 0};
    Vector n = {0, 1, 0};
    Vector b = {0, 0, 1};
};


template <typename RNGFunc>
class Wisp{
public:
    Wisp(
        RNGFunc rng_func,
        size_t n_dots,
        float dens,
        float clump,
        float pscale,
        FSVals fspn1,
        FSVals fspn2,
        Vector displacement,
        Vector location,
        CoordinateSys axis,
        int id,
        Color col = Color(1, 1, 1, 1),
        float min = -2,
        float max = 2
    ) : _rng(rng_func), _n_dots(n_dots), _dens(dens), _pscale(pscale), _min(min), _max(max), _fspn1(fspn1),
        _fspn2(fspn2), _clump(clump),
        _displacement(displacement), _location(location),
        _axis(axis), _id(id), _col(col) {}
    
    template <typename T>
    void stamp_wisp_dots(GridPtr<T> grid){
        auto accessor = grid->getAccessor();
        const auto transform = grid->transform();
        std::cout << "wisping " << _n_dots << " dots " << std::endl;
        const float hw = (_max - _min) * 0.5;
        for (int i = 0; i < _n_dots; i++){
            // get random position in unit cube
            Vector p0 = Vector(_rng(_min, _max), _rng(_min, _max), _rng(_min, _max));
            p0 = p0 == Vector(0, 0, 0) ? Vector(_rng(_min, _max), _rng(_min, _max), 1) : p0;
            Vector proj = p0 / p0.magnitude() * hw;
            float r = std::powf(std::abs(fspn<float, Vector>(_fspn1, p0)), _clump);
            auto p3 = r * proj;
            // apply guid particle location and pscale
            auto p4 = _location + _pscale * p3[0] * _axis.t + p3[1] * _axis.n + p3[2] * _axis.b;
            // now displacement vec -> this should make x/y/z have similar ish values
            auto d = Vector(fspn<float>(_fspn2, p3), fspn<float, Vector>(_fspn2, p3 + _displacement), fspn<float>(_fspn2, p3 - _displacement));
            auto p5 = p4 + d;
            // Now bake the value into the grid
            trilinear_deposit(accessor, transform, {p5[0], p5[1], p5[2]}, _dens);
        }
    }

    // Getters and setters
    size_t get_n_dots() const { return _n_dots; }
    float get_dens() const { return _dens; }
    float get_clump() const { return _clump; }
    int get_id() const { return _id; }
    float get_pscale() const { return _pscale; }
    const FSVals& get_fspn1() const { return _fspn1; }
    const FSVals& get_fspn2() const { return _fspn2; }
    const Vector& get_displacement() const { return _displacement; }
    const Vector& get_location() const { return _location; }
    const CoordinateSys& get_axis() const { return _axis; }
    const Color& get_color() const { return _col; }

    void set_n_dots(size_t n_dots) { _n_dots = n_dots; }
    void set_pscale(float pscale) { _pscale = pscale; }
    void set_dens(float dens) { _dens = dens; }
    void set_fspn1(FSVals fspn1) { _fspn1 = fspn1; }
    void set_fspn2(FSVals fspn2) { _fspn2 = fspn2; }
    void set_clump(float clump) { _clump = clump; }
    void set_displacement(Vector displacement) { _displacement = displacement; }
    void set_location(Vector location) { _location = location; }
    void set_axis(CoordinateSys axis) { _axis = axis; }
    void set_id(int id) { _id = id; }
    void set_color(Color col) { _col = col; }

private:
    size_t _n_dots;
    float _dens;
    float _clump;
    float _pscale;
    float _min;
    float _max;
    FSVals _fspn1;
    FSVals _fspn2;
    Vector _displacement;
    Vector _location;
    CoordinateSys _axis;
    int _id;
    Color _col = Color(1, 1, 1, 1);
    RNGFunc _rng;

};


auto wisp_uniform(
    size_t n_dots,
    float dens,
    float clump,
    float pscale,
    FSVals fspn1,
    FSVals fspn2,
    Vector displacement,
    Vector location,
    CoordinateSys axis,
    int id,
    Color col = Color(1, 1, 1, 1),
    float min = -1,
    float max = 1
){
    auto rng = [
        engine = std::mt19937{static_cast<std::mt19937::result_type>(id)},
        distribution = std::uniform_real_distribution<float>{}
    ](float min, float max) mutable {
        using Params = decltype(distribution)::param_type;
        return distribution(engine, Params{min, max});
    };
    return Wisp<decltype(rng)>(
        rng,
        n_dots,
        dens,
        clump,
        pscale,
        std::move(fspn1),
        std::move(fspn2),
        std::move(displacement),
        std::move(location),
        std::move(axis),
        id,
        col,
        min,
        max
    );
};






} // end namespace lux





