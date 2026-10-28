#include "catch_helpers.h"

#include "AABB.h"
#include "particle_emitter.h"
#include "Vector.h"

using namespace pba;

TEST_CASE("Test particle emitter emits within AABB"){
    // AABB should be of type bounds
    // the particle emitter should be created with some type of bounds
    // When a particle is emitted, it should emit with a random position within the bounds

    AABB aabb(Vector(-10.0, -10.0, -10.0), Vector(10.0, 10.0, 10.0));
    ParticleEmitter emitter(aabb);
    emitter.set_min_speed(5.0);
    emitter.set_max_speed(10.0);
    const size_t n_particles = 1000;
    Vector pos = Vector(0.0, 0.0, 0.0);
    Vector vel = Vector(0.0, 0.0, 0.0);
    for (size_t i=0; i<n_particles; i++){
        emitter.emit(pos, vel);
        // Check position is within AABB
        // and that speed is within min and max
        REQUIRE(aabb.contains(pos));
        REQUIRE(vel.magnitude() >= emitter.get_min_speed());
        REQUIRE(vel.magnitude() <= emitter.get_max_speed());
    }
}




