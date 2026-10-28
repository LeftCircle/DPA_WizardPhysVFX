#ifndef _PARTICLE_RBD_COLLISIONS_H
#define _PARTICLE_RBD_COLLISIONS_H

#include "rbd_solvers.h"
#include "shapes.h"
#include "partial_solvers.h"

using namespace pba;

struct ParticleRBDHitResult {
	double time;
	size_t particle;
	cato::Vec3s rbd_indices;
	Vector position;
	Vector normal;
	bool converged = false;
	RB_sp rbd;

	void set_rbd(RB_sp new_rbd) {rbd = new_rbd; }
	void set_time(double dt) { time = dt; }
};

class ParticleRBDCollisionHandler : public RBDCollisionHandler{
public:
	virtual void register_rbd(RB_sp rbd) { _rbds.push_back(rbd); }

	void handle_collisions(
		DSD_sp dsd,
		const std::string& updated_pos_attr_name,
		const double dt
	) override;

	bool does_moving_particle_collide_with_moving_rbd_tri_plane(
		Vector p_start,
		Vector p_end,
		Triangle tri_start,
		Triangle tri_end
	) const;

private:
	std::pair<RB_sp, RBDHitResult> _find_earliest_rbd_static_collision_from_all_rbds(const double dt) const;
	std::optional<ParticleRBDHitResult> _find_earliest_particle_rbd_collision(DSD_sp dsd, const double dt) const;
	std::optional<ParticleRBDHitResult> _bisect_particle_rbd_collision(
		size_t particle_idx,
		Vector p_start,
		Vector p_vel,
		cato::Vec3s rbd_indices,
		RB_sp rbd,
		double dt
	) const;
	void _update_rbd_and_particles_by(DSD_sp dsd, const double dt);
	void _resolve_particle_rbd_collision(DSD_sp dsd, ParticleRBDHitResult& hit_info, const double dt);

	std::vector<RB_sp> _rbds;

};



#endif