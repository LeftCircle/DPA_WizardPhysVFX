#ifndef _BOID_SOLVERS_H
#define _BOID_SOLVERS_H


#include "partial_solvers.h"
#include "occupancy_volume.h"
#include "boids_state_data.h"
#include "boids_acceleration.h"

namespace pba{

class AdvanceBoidVelocityWithForces : public AdvanceVelocityWithForces {
public:
	AdvanceBoidVelocityWithForces(DSD_sp dsd, ForceSystem_sp fs, idx_volume_sp ov)
		: AdvanceVelocityWithForces(dsd, fs), _occupancy_grid(ov) {}

	void solve(const double dt) override;

private:
	idx_volume_sp _occupancy_grid;
	BoidBehaviors _boid_behaviors;
};

} // end namespace pba


#endif