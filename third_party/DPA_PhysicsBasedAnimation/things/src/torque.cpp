#include "torque.h"

using namespace pba;


void Torque::compute(RB_sp& rbd, const double dt){
    _force->compute(rbd, dt);
    // Now determine the angular acceleration
    auto acc = rbd->get_vector_attribute_span("acceleration");
    auto masses = rbd->get_float_attribute_span("mass");
    
    // A simple for loop to fall back on if needed
    rbd->angular_accel = Vector(0, 0, 0);
    rbd->com_accel = Vector(0, 0, 0);
    for (int i = 0; i < rbd->n_particles(); i++){
        Vector f = acc[i] * masses[i];
        rbd->angular_accel += rbd->get_rotated_lever_arm(i) ^ f;
        rbd->com_accel += f;
    }
    if (rbd->get_total_mass()){
        rbd->com_accel /= rbd->get_total_mass();
    }
}

// A parrallel reduce that uses onetbb without the 
// need for pointer arithmatic. 
// void Torque::compute(RB_sp& rbd, const double dt){
//     _force->compute(rbd, dt);

//     auto acc = rbd->get_vector_attribute_span("acceleration");
//     auto masses = rbd->get_float_attribute_span("mass");
//     const std::size_t n = rbd->n_particles();

//     rbd->angular_accel = tbb::parallel_reduce(
//         tbb::blocked_range<std::size_t>(0, n),
//         Vector(0.0, 0.0, 0.0),
//         [&](const tbb::blocked_range<std::size_t>& range, Vector local){
//             for (std::size_t i = range.begin(); i != range.end(); ++i){
//                 local += (acc[i] / masses[i]) ^ rbd->get_rotated_lever_arm(i);
//             }
//             return local;
//         },
//         [](const Vector& a, const Vector& b){
//             return a + b;
//         }
//     );
// }