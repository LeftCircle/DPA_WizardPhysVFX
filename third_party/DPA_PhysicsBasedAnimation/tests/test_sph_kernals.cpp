#include "catch_helpers.h"

#include "sph_kernel.h"
#include "Vector.h"

using namespace pba;

TEST_CASE("Cubic Spline returns values"){
    CubicSplineKernel3 kernel(1.0);
    double val = kernel(0.5);
    Vector grad = kernel.gradient(0.5, Vector(1.0, 0.0, 0.0));
    REQUIRE(val > 0.0);
    REQUIRE(grad.X() < 0.0);
}



