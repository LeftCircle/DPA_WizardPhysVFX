#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <stdexcept>

#include "wisps.h"

namespace lux{

TEST_CASE("Test wisp guide particle interp"){
    auto wisp_guide_a = WispGuideParticle(
        1,
        1.0,
        2.0,
        3.0,
        FSVals(0.5, 2, {1, 1, 1}, 1.0, 1.0),
        FSVals(0.5, 2, {1, 1, 1}, 1.0, 1.0),
        Vector(1, 2, 3),
        Vector(4, 5, 6),
        CoordinateSys({1, 0, 0}, {0, 1, 0}, {0, 0, 1}),
        1
    );
    auto wisp_guide_b = WispGuideParticle(
        3,
        4.0,
        5.0,
        6.0,
        FSVals(1.0, 3, {2, 2, 2}, 2.0, 2.0),
        FSVals(1.0, 3, {2, 2, 2}, 2.0, 2.0),
        Vector(7, 8, 9),
        Vector(10, 11, 12),
        CoordinateSys({0, 1, 0}, {1, 0, 0}, {0, 0, 1}),
        2
    );

    auto wisp_guide_interp = interpolate(wisp_guide_a, wisp_guide_b, 0.5);
    REQUIRE(wisp_guide_interp.n_dots == 1);
    REQUIRE(wisp_guide_interp.dens == Catch::Approx(2.5));
    REQUIRE(wisp_guide_interp.clump == Catch::Approx(3.5));
    REQUIRE(wisp_guide_interp.pscale == Catch::Approx(4.5));
    REQUIRE(wisp_guide_interp.fspn1.r == Catch::Approx(0.75));
    REQUIRE(wisp_guide_interp.fspn1.octaves == Catch::Approx(2.5));
    REQUIRE(wisp_guide_interp.fspn1.translation.x() == Catch::Approx(1.5));
    REQUIRE(wisp_guide_interp.fspn1.translation.y() == Catch::Approx(1.5));
    REQUIRE(wisp_guide_interp.fspn1.translation.z() == Catch::Approx(1.5));
    REQUIRE(wisp_guide_interp.fspn1.freq == Catch::Approx(1.5));
    REQUIRE(wisp_guide_interp.fspn1.fjump == Catch::Approx(1.5));
    REQUIRE(wisp_guide_interp.fspn2.r == Catch::Approx(0.75));
    REQUIRE(wisp_guide_interp.fspn2.octaves == Catch::Approx(2.5));
    REQUIRE(wisp_guide_interp.fspn2.translation.x() == Catch::Approx(1.5));
    REQUIRE(wisp_guide_interp.fspn2.translation.y() == Catch::Approx(1.5));
    REQUIRE(wisp_guide_interp.fspn2.translation.z() == Catch::Approx(1.5));
    REQUIRE(wisp_guide_interp.fspn2.freq == Catch::Approx(1.5));
    REQUIRE(wisp_guide_interp.fspn2.fjump == Catch::Approx(1.5));
    REQUIRE(wisp_guide_interp.displacement.x() == Catch::Approx(4.0));
    REQUIRE(wisp_guide_interp.displacement.y() == Catch::Approx(5.0));
    REQUIRE(wisp_guide_interp.displacement.z() == Catch::Approx(6.0));
    REQUIRE(wisp_guide_interp.location.x() == Catch::Approx(7.0));
    REQUIRE(wisp_guide_interp.location.y() == Catch::Approx(8.0));
    REQUIRE(wisp_guide_interp.location.z() == Catch::Approx(9.0));
    REQUIRE(wisp_guide_interp.axis.t.x() == Catch::Approx(0.7071067811865476));
    REQUIRE(wisp_guide_interp.axis.t.y() == Catch::Approx(0.7071067811865476));
    REQUIRE(wisp_guide_interp.axis.t.z() == Catch::Approx(0.0));
    REQUIRE(wisp_guide_interp.axis.n.x() == Catch::Approx(0.7071067811865476));
    REQUIRE(wisp_guide_interp.axis.n.y() == Catch::Approx(0.7071067811865476));
    REQUIRE(wisp_guide_interp.axis.n.z() == Catch::Approx(0.0));
    REQUIRE(wisp_guide_interp.axis.b.x() == Catch::Approx(0.0));
    REQUIRE(wisp_guide_interp.axis.b.y() == Catch::Approx(0.0));
    REQUIRE(wisp_guide_interp.axis.b.z() == Catch::Approx(1.0));
    REQUIRE(wisp_guide_interp.id == 1);
    auto endpoint = interpolate(wisp_guide_a, wisp_guide_b, 1.0f);
    REQUIRE(endpoint.axis.t.x() == Catch::Approx(0.0).margin(1e-6));
    REQUIRE(endpoint.axis.t.y() == Catch::Approx(1.0));

}

TEST_CASE("wisp spline stamps a deterministic field and rejects coincident guides") {
    const FSVals noise(0.5, 2, Vector(0,0,0), 1, 2);
    const CoordinateSys axis;
    WispGuideParticle a(1, -1, 1, 1, noise, noise, Vector(1,1,1), Vector(0,0,0), axis, 1);
    WispGuideParticle b(1, -1, 1, 1, noise, noise, Vector(1,1,1), Vector(0,4,0), axis, 2);
    auto middle = interpolate(a, b, 0.5f);
    REQUIRE(middle.axis.t.X() == Catch::Approx(1));
    REQUIRE(middle.axis.n.Y() == Catch::Approx(1));
    REQUIRE(middle.axis.b.Z() == Catch::Approx(1));
    WispSpline spline({a, b}, 500, 0.25f, 7);
    auto first = spline.stamp_wisp_spline_grid();
    auto second = spline.stamp_wisp_spline_grid();
    REQUIRE(first->activeVoxelCount() > 0);
    REQUIRE(first->activeVoxelCount() == second->activeVoxelCount());
    bool negative = false;
    for (auto it = first->cbeginValueOn(); it; ++it) {
        REQUIRE(*it == Catch::Approx(second->tree().getValue(it.getCoord())));
        if (*it < 0) negative = true;
    }
    REQUIRE(negative);
    REQUIRE(spline.stamp_wisp_spline());
    REQUIRE_THROWS_AS(WispSpline({a, a}, 5, 0.25f, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(WispSpline({a}, 5, 0.25f, 1), std::invalid_argument);
}


} // end namespace lux
