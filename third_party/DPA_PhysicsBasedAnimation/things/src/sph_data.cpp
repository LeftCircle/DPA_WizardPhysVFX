#include "sph_data.h"

using namespace pba;


SPHData::SPHData() : DynamicalStateData() {
	add_attribute<double>("density", DSAd("density", 1.0));
	add_attribute<double>("predicted_density", DSAd("predicted_density", 1.0));
	add_attribute<double>("density_error", DSAd("density_error", 1.0));
	add_attribute<double>("pressures", DSAd("pressures", 0.0));
	add_attribute<Vector>("new_velocities", DSAv("new_velocities", Vector(0, 0, 0)));
	add_attribute<Vector>("pressure_force", DSAv("pressure_force", Vector(0, 0, 0)));
}