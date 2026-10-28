#include <catch2/catch_test_macros.hpp>
#include <memory>

#include "volume.h"
#include "Vector.h"
#include "implicit_fields.h"
#include "field_operations.h"
#include "field_interface.h"

using namespace lux;
using vspf = VolumeSPtr<float>;

TEST_CASE("Testing templated Field operations"){

    
    // Create two templated ConstField<float>
    float a_val = 7.0;
    float b_val = 5.0;
    std::shared_ptr<Volume<float>> a = std::make_shared<ConstantField<float>>(a_val);
    std::shared_ptr<Volume<float>> b = std::make_shared<ConstantField<float>>(b_val);

    //VolumeSPtr<float> c = add(a, b);
    VolumeSPtr<float> d = a + b;
    //REQUIRE(c->eval(Vector(3, 3, 3)) == 12);
    REQUIRE(d->eval(Vector(3, 3, 3)) == 12);

    VolumeSPtr<float> sub = a - b;
    REQUIRE(sub->eval(Vector()) == (a_val - b_val));
    
    // switching to plane to actuall test gradients
    Vector point(1, 1, 1);
    Vector normal(1, 0, 0);
    Vector point2(10, -10, 15);
    Vector normal2(-3, 2, 7);
    VolumeSPtr<float> plane1 = make_plane(point, normal);
    VolumeSPtr<float> plane2 = make_plane(point2, normal2);

    Vector eval_point(7, 7, -7);
    float p1_eval = normal * (eval_point - point);
    float p2_eval = normal2 * (eval_point - point2);
    vspf negate = -plane1;
    REQUIRE(negate->eval(eval_point) == -p1_eval);
    REQUIRE(negate->grad(eval_point) == -normal);

    vspf divide = plane1 / a;
    REQUIRE(divide->eval(eval_point) == p1_eval / a_val);
    REQUIRE(divide->grad(eval_point) == normal / a_val);

    vspf divide_consts = a / b;
    REQUIRE(divide_consts->grad(eval_point) == Vector());


    vspf exp_f = exp(plane1);
    REQUIRE(exp_f->eval(eval_point) == std::exp(p1_eval));
    REQUIRE(exp_f->grad(eval_point) == std::exp(p1_eval) * normal);

    vspf logf = log(plane1);
    REQUIRE(logf->eval(eval_point) == std::log(p1_eval));
    REQUIRE(logf->grad(eval_point) == normal / p1_eval);

    vspf sinf = sin(plane1);
    REQUIRE(sinf->eval(eval_point) == std::sin(p1_eval));
    REQUIRE(sinf->grad(eval_point) == std::cos(p1_eval) * normal);

    vspf cosf = cos(plane1);
    REQUIRE(cosf->eval(eval_point) == std::cos(p1_eval));
    REQUIRE(cosf->grad(eval_point) == -std::sin(p1_eval) * normal);

    float power_to = 3.0;
    vspf pow_field = make_constant<float>(power_to);
    vspf powf = pow(plane1, pow_field);
    REQUIRE(powf->eval(eval_point) == std::pow(p1_eval, power_to));
    REQUIRE(powf->grad(eval_point) == power_to * std::pow(p1_eval, power_to - 1.0) * normal);

    float scale_val = 4.5;
    vspf scale_field = make_constant<float>(scale_val);
    vspf scalef = scale(plane1, scale_field);
    float expected_eval = normal * ((eval_point / scale_val) - point);
    REQUIRE(scalef->eval(eval_point) == expected_eval);
    REQUIRE(scalef->grad(eval_point) == normal / scale_val);

    Vector translation_val = Vector(-4, 3, 100);
    auto trans_field = make_constant<Vector>(translation_val);
    vspf translate_f = translate(plane1, trans_field);
    expected_eval = normal * ((eval_point - translation_val) - point);
    REQUIRE(translate_f->eval(eval_point) == expected_eval);
    REQUIRE(translate_f->grad(eval_point) == normal);


}


TEST_CASE("Test implicit plane"){
    // A plane has a point that is on the surface, and a normal direction
    // Positive above the field and negative below
    Vector point(1, 1, 1);
    Vector normal(1, 0, 0);
    VolumeSPtr<float> plane = make_plane(point, normal);

    REQUIRE(plane->eval(Vector(2, -1, 10)) == 1.0);
    REQUIRE(plane->eval(Vector(0, 0, 0))== -1.0);
    REQUIRE(plane->grad(Vector(1, 2, 3)) == normal);

    float const_val = 3.5;
    vspf const_field = make_constant<float>(const_val);

    vspf mult = const_field * plane;
    //grad(fg) = f *grad(g) + g * grad(f);
    Vector expected = const_val * normal;
    REQUIRE(mult->grad(Vector(1, 1, 1)) == expected);

}

TEST_CASE("Test Implicit Functions"){
    // Make a sphere and confirm that a point inside is negative and
    // outside is positive inside
    Vector inside_eval, outside_eval;
    float expected_val;
    Vector center(3, 4, 5);
    float radius = 5;
    inside_eval = Vector(2, 3, 4);
    outside_eval = center + radius * 2 * Vector(1, 2, 3);
    vspf sphere = isf_sphere(center, radius);
    REQUIRE(sphere->eval(inside_eval) < 0);
    REQUIRE(sphere->eval(outside_eval) > 0);
    

    // float r_major = 3.0;
    // float r_minor = 2.0;
    // Vector nhat(0, 1, 0);
    // inside_eval = center + (r_major + r_minor) / 2.0 * Vector(1, 0, 0);
    // outside_eval = center;
    // vspf torus = isf_torus(center, r_major, r_major, nhat);
    // REQUIRE(torus->eval(inside_eval) < 0);
    // REQUIRE(torus->eval(outside_eval) > 0);
}