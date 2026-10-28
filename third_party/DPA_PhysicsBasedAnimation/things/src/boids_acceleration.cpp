#include "boids_acceleration.h"

using namespace pba;


Vector BoidBehaviors::get_acceleration_due_to_neighbors(
    	const Vector& pos,
		const Vector& vel,
        const std::vector<size_t>& neighbor_indices,
		span<const Vector> positions,
		span<const Vector> velocities,
        const BoidParams* params
	) const
{    
    const size_t n_neighbors = neighbor_indices.size();
	Vector acc(0, 0, 0);
	for (const auto& behavior : _behaviors){
		// acc += std::transform_reduce(
		// 	nd.begin(), nd.end(), Vector(0, 0, 0),
		// 	std::plus<>(), behavior
		// );
        for (int i = 0; i < n_neighbors; i++){
            acc += behavior(_make_neighbor_data(
                pos, vel, positions[neighbor_indices[i]],
                velocities[neighbor_indices[i]], params
            ));
        }
        double acc_mag = acc.magnitude();
		if (acc_mag >= params->threshold){
			return acc * (params->threshold / acc_mag);
		}
	}
	return acc;
}

void BoidBehaviors::_cache_neighbor_data(
	const Vector& pos,
	const Vector& vel,
    std::vector<size_t> neighbor_idxs,
	span<const Vector> positions,
	span<const Vector> vels,
	const BoidParams* params) const
{
	const size_t n_neighbors = neighbor_idxs.size();
    _ndata.clear();
    _ndata.resize(n_neighbors);
	for (size_t i = 0; i < n_neighbors; i++){
        _ndata[i] = _make_neighbor_data(pos, vel, positions[neighbor_idxs[i]], vels[neighbor_idxs[i]], params);
    }
}


NeighborData BoidBehaviors::_make_neighbor_data(
    const Vector& pos,
	const Vector& vel,
	const Vector& neighbor_pos,
	const Vector& neighbor_vel,
	const BoidParams* p
	) const
{
    Vector vec_to_n = neighbor_pos - pos;
    double range_limit = _range_limit(vec_to_n, p);
    double fov_limit = _fov_limiter(vec_to_n, vel, p);
    return NeighborData{vec_to_n, neighbor_vel - vel, range_limit * fov_limit, p};
};

double BoidBehaviors::_range_limit(const Vector& vec_to_point, const BoidParams* params) const{
    double d = vec_to_point.magnitude();
    if (d < params->range){
        return 1.0;
    } else if( d < (params->range + params->range_amp)){
        return 1.0 - (d - params->range) / params->range_amp;
    } else {
        return 0.0;
    }
}

double BoidBehaviors::_fov_limiter(
    const Vector& vec_to_point,
    const Vector& vel,
    const BoidParams* params) const
{
    double cos_ab = 0;
    if (vel.magnitude() != 0){
        cos_ab = vec_to_point * vel / (vec_to_point.magnitude() * vel.magnitude());
    }
    if (cos_ab >= params->cos_fov_angle){
        return 1.0;
    } else if (cos_ab > params->cos_fov_angle_plus_amp){
        return 1.0 - (params->cos_fov_angle - cos_ab) / (
            params->cos_fov_angle - params->cos_fov_angle_plus_amp
        );
    } else {
        return 0.0;
    }
}

Vector BoidBehaviors::_collision_avoidance(
    const Vector& a_to_b,
    const BoidParams* params) const
{
    double d = a_to_b.magnitude();
    if (d < 1e-10) return Vector(0, 0, 0);
    return -a_to_b * (params->ca_strength / (d * d));
}

Vector BoidBehaviors::_velocity_match(
	const Vector& vel_diff,
	const BoidParams* params) const
{
    return vel_diff * params->vm_strength;
}

Vector BoidBehaviors::_centering(const Vector& a_to_b, const BoidParams* params) const {
    return params->cent_strength * a_to_b;
}




