#include <catch2/catch_test_macros.hpp>

#include <openvdb/openvdb.h>
#include <Vector.h>
#include <vector>

#include "raytracer.h"

using namespace lux;

TEST_CASE("test ray traingle intersections"){

    //  2 ---- 3
    //  |      |
    //  0 ---- 1
    std::vector<openvdb::Vec3I> triangles = {{0, 1, 2}};
    std::vector<openvdb::Vec4I> faces = {{0, 1, 3, 2}};
    std::vector<Vector> verts = {
        Vector(0, 0, 0),
        Vector(1, 0, 0),
        Vector(0, 1, 0),
        Vector(1, 1, 0)
    };

    Vector start_pos = Vector(0.5, 0.5, 3);
    Vector ray_hit_dir = Vector(0, 0, -1);

    Vector expected_hit_pos(0.5, 0.5, 0);

    auto tri_hit = get_ray_triangle_intersection(start_pos, ray_hit_dir, triangles[0], verts);
    REQUIRE(tri_hit == expected_hit_pos);

    auto quad_hit = get_ray_face_intersection(start_pos, ray_hit_dir, faces[0], verts);
    REQUIRE(*quad_hit == expected_hit_pos);
}

TEST_CASE("Test get earliest triangle hit"){
    std::vector<openvdb::Vec3I> triangles = {{0, 1, 2}, {4, 5, 6}, {8, 9, 10} };
    std::vector<openvdb::Vec4I> faces = {{0, 1, 3, 2}};
    std::vector<Vector> verts = {
        Vector(0, 0, -10),
        Vector(1, 0, -10),
        Vector(0, 1, -10),
        Vector(1, 1, -10),
        Vector(0, 0, -1),
        Vector(1, 0, -1),
        Vector(0, 1, -1),
        Vector(1, 1, -1),
        Vector(0, 0, -2),
        Vector(1, 0, -2),
        Vector(0, 1, -2),
        Vector(1, 1, -2)
    };

     Vector start_pos = Vector(0.5, 0.5, 3);
    Vector ray_hit_dir = Vector(0, 0, -1);

    Vector expected_hit_pos(0.5, 0.5, -1);

    auto earliest_hit = get_first_hit_position(start_pos, ray_hit_dir, triangles, faces, verts);

    REQUIRE(*earliest_hit == expected_hit_pos);
}


// TEST_CASE("Test get all ray tiangle intersections"){
//     std::vector<openvdb::Vec3I> triangles = {{0, 1, 2}, {4, 5, 6}, {8, 9, 10} };
//     std::vector<openvdb::Vec4I> faces = {{0, 1, 3, 2}};
//     std::vector<Vector> verts = {
//         Vector(0, 0, 0),
//         Vector(1, 0, 0),
//         Vector(0, 1, 0),
//         Vector(1, 1, 0),
//         Vector(0, 0, -1),
//         Vector(1, 0, -1),
//         Vector(0, 1, -1),
//         Vector(1, 1, -1),
//         Vector(0, 0, -2),
//         Vector(1, 0, -2),
//         Vector(0, 1, -2),
//         Vector(1, 1, -2)
//     };

//     //auto tri_hits = get_all_ray_triangle_intersections
// }