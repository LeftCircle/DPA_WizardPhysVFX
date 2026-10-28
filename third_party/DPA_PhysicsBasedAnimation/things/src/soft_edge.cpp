#include "soft_edge.h"


using namespace pba;

void SoftEdge::compute(
    span<const Vector> positions,
    span<const Vector> vels,
    const double spring_force,
    const double friction
){
    _force_on_a = get_acceleration_on_a(
        positions, vels, spring_force, friction
    );
}

Vector SoftEdge::get_acceleration_on_a(
    span<const Vector> positions,
    span<const Vector> vels,
    const double spring_force,
    const double friction
) const 
{
    const Vector& pos_a = positions[_index_a];
    const Vector& pos_b = positions[_index_b];
    const Vector& vel_a = vels[_index_a];
    const Vector& vel_b = vels[_index_b];

    // Force due to spring is just -kx
    Vector a_to_b = pos_b - pos_a;
    double mag_a_to_b = a_to_b.magnitude();
    Vector dir_a_to_b = a_to_b / mag_a_to_b;
    Vector force_on_a = (mag_a_to_b - _rest_length) * dir_a_to_b * spring_force;
    // Now vel forces
    // Only care about the forces along a_to_b
    double vel_diff = (vel_b - vel_a) * dir_a_to_b;
    force_on_a += vel_diff * friction * dir_a_to_b;
    return force_on_a;
}



