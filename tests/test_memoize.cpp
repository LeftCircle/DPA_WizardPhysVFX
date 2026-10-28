#include <catch2/catch_test_macros.hpp>
#include "field_interface.h"
#include "profile_helpers.h"

using namespace lux;

TEST_CASE("test profile memoize"){

    auto sphere = isf_sphere({}, 3);
    auto fspn = make_fspn(3, 7, {}, 4, 4);
    auto u = make_constant<Vector>(Vector(1, 1, 1));
    u = u * fspn;
    auto memu = make_memofield(u);

    const float dt = 0.0167;
    auto advect = make_advect_semi_lagrange(sphere, u, dt);
    auto mem_advect = make_advect_semi_lagrange(sphere, memu, dt);

    int iters = 100;
    for (int i = 0; i < iters; i++){
        advect = make_advect_semi_lagrange(advect, u, dt);
        mem_advect = make_advect_semi_lagrange(mem_advect, memu, dt);
    }
    Vector eval_point(0.01, 0.01, 0.01);
    auto start = std::chrono::high_resolution_clock::now();
    auto reg = advect->eval(eval_point);
    auto end = std::chrono::high_resolution_clock::now();

    std::cout << "regular took "
                  << std::chrono::duration<double>(end - start).count()
                  << " seconds\n";

    start = std::chrono::high_resolution_clock::now();
    auto memorized = mem_advect->eval(eval_point);
    end = std::chrono::high_resolution_clock::now();
    std::cout << "memorized took "
                  << std::chrono::duration<double>(end - start).count()
                  << " seconds\n";


    REQUIRE(reg == memorized);

}



