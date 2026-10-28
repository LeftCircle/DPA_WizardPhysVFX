#ifndef _BOIDS_ACCELERATION_H
#define _BOIDS_ACCELERATION_H

#include "Vector.h"
#include "the_wheel.h"

#include <algorithm>
#include <numeric>
#include <execution>
#include <vector>

namespace pba{

struct BoidParams {
	double range;
	double range_amp;
	double cos_fov_angle;
	double cos_fov_angle_plus_amp;
	double ca_strength;
	double vm_strength;
	double cent_strength;
	double threshold;
};

struct NeighborData {
	Vector vec_to;
	Vector vel_diff;
	double weight;
	const BoidParams* p;
};

class BoidBehaviors {
public:
	BoidBehaviors() = default;
	~BoidBehaviors() = default;	

	Vector get_acceleration_due_to_neighbors(
		const Vector& pos,
		const Vector& vel,
		const std::vector<size_t>& neighbor_indeces,
		span<const Vector> positions,
		span<const Vector> velocities,
		const BoidParams* params	
	) const;
	
private:
	using BehaviorFn = std::function<Vector(const NeighborData&)>;
	std::array<BehaviorFn, 3> _behaviors = {
		[this](const NeighborData& nd) -> Vector {
			return _collision_avoidance(nd.vec_to, nd.p) * nd.weight;
		},
		[this](const NeighborData& nd) -> Vector {
			return _velocity_match(nd.vel_diff, nd.p) * nd.weight;
		},
		[this](const NeighborData& nd) -> Vector {
			return _centering(nd.vec_to, nd.p) * nd.weight;
		}
	};

	NeighborData _make_neighbor_data(
		const Vector& pos,
		const Vector& vel,
		const Vector& neighbor_pos,
		const Vector& neighbor_vel,
		const BoidParams* p
	) const;
	
	double _range_limit(const Vector& vec_to_point, const BoidParams* params) const;
	
	double _fov_limiter(
		const Vector& vec_to_point,
		const Vector& vel,
		const BoidParams* params
	) const;

	Vector _collision_avoidance(
		const Vector& a_to_b,
		const BoidParams* params
	) const;

	Vector _velocity_match(
		const Vector& vel_diff,
		const BoidParams* params
	) const;

	Vector _centering(
		const Vector& a_to_b,
		const BoidParams* params
	) const;

	void _cache_neighbor_data(
		const Vector& pos,
		const Vector& vel,
		std::vector<size_t> neighbor_idxs,
		span<const Vector> positions,
		span<const Vector> vels,
		const BoidParams* params
	) const;
	
	mutable std::vector<NeighborData> _ndata;
};


}


#endif