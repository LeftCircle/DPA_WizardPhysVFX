#include "force.h"


using namespace pba;


void ForceSystem::compute(DSD_sp dsd, const double dt) const {
	reset_accelerations(dsd);
	for( const auto& force : _forces ){
		force->compute(dsd, dt);
	}
}

void ForceSystem::reset_accelerations(DSD_sp dsd) noexcept{
	const size_t n = dsd->n_particles();
	auto acc = dsd->get_vector_attribute_span("acceleration");
	std::fill(acc.begin(), acc.end(), Vector(0, 0, 0));
}

void ForceFunctionSystem::compute(DSD_sp dsd, const double dt) const{
	ForceSystem::reset_accelerations(dsd);
	for (const auto& fn : _forces){
		fn(dsd, dt);
	}
}
