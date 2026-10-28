#include <catch2/catch_test_macros.hpp>
#include <boost/algorithm/cxx11/iota.hpp>
#include <vector>



TEST_CASE("Test boost exists"){
    std::vector<int> values(4);

    boost::algorithm::iota(values.begin(), values.end(), 3);

    REQUIRE(values == std::vector<int>{3, 4, 5, 6});

}

