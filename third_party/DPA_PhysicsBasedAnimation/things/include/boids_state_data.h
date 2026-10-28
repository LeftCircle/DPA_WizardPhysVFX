#ifndef _BOID_STATE_DATA_H
#define _BOID_STATE_DATA_H

#include "dynamical_state_data.h"
#include "boids_acceleration.h"

namespace pba{

class BoidStateData : public DynamicalStateData{
public:
	BoidStateData() { _initialize_default_attributes(); };
	const BoidParams get_params(const size_t idx) const;
	void set_max_range(double range, double range_amp) { _max_range = range + range_amp; }
	double get_max_range() const noexcept { return _max_range; }
	void set_all_view_and_view_ramp(double cos_ang, double cos_ang_plus_ramp);

private:
	void _initialize_default_attributes() override;
	double _max_range = 2.0;
};


}// end namespace pba


#endif