#include "catch_helpers.h"
#include "obj_reader.h"

#include <Vector.h>
#include <vector>
#include <filesystem>

using namespace pba;

TEST_CASE("test get verts from obj file"){
    std::filesystem::path test_dir = std::filesystem::path(__FILE__).parent_path();
    std::filesystem::path obj_file_path = test_dir / "../models/bunny_superlo_scaled.obj";

    ObjReader<Vector> r(obj_file_path);

    REQUIRE(r.get_verts().size() == 322);
    REQUIRE(r.get_faces().size() == 640);
}