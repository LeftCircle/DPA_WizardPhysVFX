#include "catch_helpers.h"


#include "array3D.h"

using namespace pba;
// a 3D array is simply a wrapper around a std::vector that provides 3D indexing

TEST_CASE("Array3D can be indexed correctly"){
    int x_size = 8;
    int y_size = 9;
    int z_size = 10;
    Array3D<int> arr(x_size, y_size, z_size);
    int x_idx = 1;
    int y_idx = 2;
    int z_idx = 3;
    int idx_a = arr.index(x_idx, y_idx, z_idx);
    //REQUIRE(idx_a == x_idx * y_size * z_size + y_idx * z_size + z_idx);
    REQUIRE(idx_a == x_idx + x_size * (y_idx + y_size * z_idx));
}