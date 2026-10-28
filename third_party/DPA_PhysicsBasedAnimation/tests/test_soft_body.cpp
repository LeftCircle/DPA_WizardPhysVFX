#include "catch_helpers.h"

#include <memory>
#include <ranges>

#include "dynamical_state_data.h"
#include "soft_body_data.h"
#include "math.h"
#include "shapes.h"
#include "force_library.h"
#include "the_wheel.h"

using namespace pba;


TEST_CASE("test factorial"){
    REQUIRE(factorial<int>(10) == 3628800);
}

TEST_CASE("check number of soft edges in fully connected soft body"){
    // We need to confirm that we are not double counting connections in 
    // the soft body. So the first particle should be connected to all, the second
    // to all but the first, the third to all but the first two, ...

    auto soft_body = std::make_unique<SoftBody>();
    soft_body->add(10);
    soft_body->connect_all_particles_together();
    REQUIRE(soft_body->get_n_soft_edges() == 9 * 10 / 2);


}

TEST_CASE("test no net force in soft triangles"){
    // Create a dynamical state data with three particles
    // Create a soft triangle based on those particles
    // calculate the soft triangle force on those particles
    // ensure that the sum of the three forces is zero

    // auto dsd = std::make_shared<SoftBody>();
    // dsd->add(3);
    // dsd->set_position(0, Vector(0, 0, 0));
    // dsd->set_position(1, Vector(1, 0, 0));
    // dsd->set_position(2, Vector(0, 1, 0));

    // double area = Triangle::get_area(dsd->p(0), dsd->p(1), dsd->p(2));
    // SoftTriangle soft_tri(0, 1, 2, area, 1.0);
    // std::vector<SoftTriangle> soft_triangles{soft_tri};
    // span<const SoftTriangle> soft_tris_span(soft_triangles);
    // SoftTriangleForce stf(tri_provider);
  
    // stf.compute(dsd, 1.0);

    // // Require that forces on all are zero
    // REQUIRE(dsd->a(0).X() == 0.0 && dsd->a(0).Y() == 0.0 && dsd->a(0).Z() == 0.0);

    // // Now perturb the triangle and check forces
    // dsd->set_position(0, Vector(-1, -1, 0));
    

}

TEST_CASE("test soft triangle force can be used with Accumulating Force"){
     auto dsd = std::make_shared<SoftBody>();
    dsd->add(3);
    dsd->set_position(0, Vector(0, 0, 0));
    dsd->set_position(1, Vector(1, 0, 0));
    dsd->set_position(2, Vector(0, 1, 0));

    double area = Triangle::get_area(dsd->p(0), dsd->p(1), dsd->p(2));
    SoftTriangle soft_tri(0, 1, 2, area, 1.0);
    std::vector<SoftTriangle> soft_triangles{soft_tri};
    auto stf = SoftTriangleForce();

    dsd->soft_triangles = soft_triangles;
    dsd->set_position(0, Vector(-1, -1, 0));
    stf.compute(dsd, 1.0);
    REQUIRE(dsd->a(0).magnitude() != 0);
}


TEST_CASE("Test piping"){
    auto dsd = std::make_shared<SoftBody>();
    dsd->add(3);
    dsd->set_position(0, Vector(0, 0, 0));
    dsd->set_position(1, Vector(1, 0, 0));
    dsd->set_position(2, Vector(0, 1, 0));

    double area = Triangle::get_area(dsd->p(0), dsd->p(1), dsd->p(2));
    SoftTriangle soft_tri(0, 1, 2, area, 1.0);
    SoftTriangle soft_tri_2(1, 2, 0, area, 1.0);
    SoftTriangle soft_tri_3(2, 0, 1, area, 1.0);
    std::vector<SoftTriangle> soft_triangles{soft_tri};

    auto stf = SoftTriangleForce();

    stf.compute(dsd, 1.0, soft_triangles);

    // assert accelerations are zero
    for (int i = 0; i < dsd->n_particles(); i++){
        REQUIRE(dsd->a(i).magnitude() == 0);
        printf("Pre acceleration %zu = %f, %f, %f \n", i, dsd->a(i).X(), dsd->a(i).Y(), dsd->a(i).Z());
    }

    // move a point
    dsd->set_position(0, Vector(-1, -1, 0));
    stf.compute(dsd, 1.0, soft_triangles);
    Vector total_acceleration(0, 0, 0);
    for (size_t i = 0; i < dsd->n_particles(); i++){
        REQUIRE(dsd->a(i).magnitude() != 0);
        REQUIRE(dsd->a(i).Z() == 0);
        total_acceleration += dsd->a(i);
        printf("Post acceleration %zu = %f, %f, %f \n", i, dsd->a(i).X(), dsd->a(i).Y(), dsd->a(i).Z());
    }

    REQUIRE(total_acceleration.magnitude() == 0);
}

TEST_CASE("Test no net force for different massed particles"){
    //REQUIRE(false);
}