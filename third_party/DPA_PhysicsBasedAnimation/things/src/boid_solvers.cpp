#include "boid_solvers.h"
#include "omp.h"

using namespace pba;

void AdvanceBoidVelocityWithForces::solve(const double dt){
    _force_system->compute(_state_data, dt);
    const size_t n = _state_data->n_particles();
    auto pos = _state_data->get_vector_attribute_span("positions");
    auto vel = _state_data->get_vector_attribute_span("velocities");
    auto accel = _state_data->get_vector_attribute_span("acceleration");
    // for each boid, get its neighbors, then find acceleration
    std::shared_ptr<BoidStateData> bsd = std::dynamic_pointer_cast<BoidStateData>(_state_data);
    omp_set_num_threads(4);
    #pragma omp parallel for
    for (size_t i = 0; i < n; i++){
        const BoidParams params = bsd->get_params(i);
        Vector boid_acc = _occupancy_grid->accumulate_neighbor_cells(
            i, pos[i], Vector(0, 0, 0),
            [this, &pos, &vel, &params](size_t index, Vector acc, const std::vector<size_t>& n_idxs){
                return acc + _boid_behaviors.get_acceleration_due_to_neighbors(
                    pos[index],
                    vel[index],
                    n_idxs,
                    pos,
                    vel,
                    &params
                );
            }
        );
        accel[i] += boid_acc;
    }
    //#pragma omp parallel for
	for( size_t i=0; i<n; i++ ){
		const Vector& vel = _state_data->get_velocity(i);
		const Vector& acc = _state_data->get_acceleration(i);
		_state_data->set_velocity(i, vel + acc * dt);
	}
}