#include "particle_rbd_collisions.h"

using namespace pba;


void ParticleRBDCollisionHandler::handle_collisions(
	DSD_sp dsd,
	const std::string& updated_pos_attr_name,
	const double dt
){
	bool collision_found = true;
	double time_left = dt;
	while (std::abs(time_left) > 0){
		//printf("Time left = %f\n", time_left);
		// 1.) find earliest particle static geo collision (we might not need this)
		CollisionHandleInfo earliest_particle_hit = _find_earliest_particle_static_geo_collision(
			dsd, "new_positions", time_left
		);

		// 2.) find earliest rbd static geo collision
		std::pair<RB_sp, RBDHitResult> earliest_rbd_hit = _find_earliest_rbd_static_collision_from_all_rbds(time_left);

		// 3.) find earliest particle rbd triangle collision
		// Will need a different data structure that tracks what triangle of what rbd is hit
		std::optional<ParticleRBDHitResult> earliest_particle_triangle_hit = _find_earliest_particle_rbd_collision(dsd, time_left);
		
		// 4.) advance all to the time of earliest collision
		double p_t = std::abs(earliest_particle_hit.hit_info.time_of_impact);
		double rbd_t = std::abs(earliest_rbd_hit.second.time);
		double p_rbd_t = earliest_particle_triangle_hit ? std::abs(earliest_particle_triangle_hit->time) : std::abs(2.0 * dt);
		double abs_2dt = std::abs(2.0 * time_left);
		if (p_t < abs_2dt && p_t < rbd_t && p_t < p_rbd_t){
			// Min time is for particles. Advance particles with collisions and the rest without
			_update_rbd_and_particles_by(dsd, earliest_particle_hit.hit_info.time_of_impact);
			if (earliest_particle_hit.collision_surface == nullptr){
				throw std::runtime_error("null collision surface somehow\n");
			}
			_resolve_particle_collision(dsd, earliest_particle_hit, time_left);
			time_left = time_left - earliest_particle_hit.hit_info.time_of_impact;
		} else if (rbd_t < abs_2dt && rbd_t < p_t && rbd_t < p_rbd_t){
			// Min time is for rbd. Advance rbd with collisions and rest without
			_update_rbd_and_particles_by(dsd, earliest_rbd_hit.second.time);
			_resolve_collision(earliest_rbd_hit.first, earliest_rbd_hit.second, earliest_rbd_hit.second.time);
			time_left = time_left - earliest_rbd_hit.second.time;
		} else if (earliest_particle_triangle_hit && p_rbd_t < abs_2dt && p_rbd_t < p_t && p_rbd_t < rbd_t){
			// particle rbd collision. Advance particles and rbd, then rest without.
			_update_rbd_and_particles_by(dsd, earliest_particle_triangle_hit->time);
			_resolve_particle_rbd_collision(dsd, *earliest_particle_triangle_hit, time_left);
			time_left = time_left - earliest_particle_triangle_hit->time;
		} else{
			collision_found = false;
			// No collisions! update everything by the remaining dt
			_update_rbd_and_particles_by(dsd, time_left);
			time_left = 0;
			break;
		}
	}
}

void ParticleRBDCollisionHandler::_resolve_particle_rbd_collision(DSD_sp dsd, ParticleRBDHitResult& hit_info, const double dt){
	// Everything is already updated to the correct space in time. Now we just need the particle and
	// rbd collision response. 
	size_t i = hit_info.particle;
    const Vector& n = hit_info.normal;
    const Vector& r = hit_info.rbd->get_rotated_lever_arm(hit_info.position);
	const float mp = dsd->get_mass(i);
    //const float M = hit_info.rbd->get_total_mass() + mp;
    Vector rxn = r ^ n;
    RB_sp rbd = hit_info.rbd;
	const float rbd_m = rbd->get_total_mass();
	//Vector COM = (mp * dsd->get_position(i) + rbd_m * rbd->center_of_mass) / M;
    Vector& vp = dsd->get_velocity(i);

	Vector& w = rbd->angular_velocity;
	//double A_numer_a = -2.0 * (dsd->get_velocity(i) * n - rbd->linear_velocity * n);
	//double A_numer_b = w * rxn + (rbd->get_moi() * w) * (rbd->get_inverse_moi() * rxn);
	//double A_numer_b = 2 * w * rxn;
	//double A_denom = (1.0 / mp) + (1.0 / rbd_m) + rxn * rbd->get_inverse_moi() * rxn;
	//double A = (A_numer_a + A_numer_b) / A_denom;
	
	double A_numer = 2.0 * (w * rxn - (vp - rbd->linear_velocity) * n);
	//double A_numer = 2.0 * (-vp * n + rbd->linear_velocity * n + w * rxn);
	double A_denom = 1.0 / mp + 1.0 / rbd_m + (rxn * rbd->get_inverse_moi() * rxn);
	double A = A_numer / A_denom;
	
	//printf("Particle rbd A = %f\n", A);
	//A = std::abs(A);
    // Now update pos and rotation with the bounce
    rbd->linear_velocity -= A / rbd->get_total_mass() * n;
    rbd->angular_velocity -= A * rbd->get_inverse_moi() * rxn;
	dsd->set_velocity(i, dsd->get_velocity(i) + (A / mp) * n);
	// Now we have to set the expected update position for this particle
	Vector new_p = dsd->get_position(i) + dsd->get_velocity(i) * (dt - hit_info.time);
	dsd->set_updated_position(i, new_p);

    // Angular momentum update!!!
    rbd->angular_momentum = rbd->get_moi() * rbd->angular_velocity;
}

void ParticleRBDCollisionHandler::_update_rbd_and_particles_by(DSD_sp dsd, const double dt){
	for (auto& rbd: _rbds){
		AdvanceRotationAndCOM::solve(rbd, dt);
	}
	PartialSolverAdvancePosition::partial_update(dsd, dt);
}

std::pair<RB_sp, RBDHitResult> ParticleRBDCollisionHandler::_find_earliest_rbd_static_collision_from_all_rbds(const double dt) const{
	RBDHitResult earliest_rbd_hit{2.0 * dt};
	std::pair<RB_sp, RBDHitResult> result;
	result.first = nullptr;
	for (auto rbd: _rbds){
		RBDHitResult temp_hit{2.0 * dt};
		bool hit = _find_earliest_rbd_static_collision(rbd, temp_hit, dt);
		if (std::abs(temp_hit.time) < std::abs(earliest_rbd_hit.time)){
			earliest_rbd_hit = temp_hit;
			result.first = rbd;
		}
	}
	result.second = earliest_rbd_hit;
	return result;
}

std::optional<ParticleRBDHitResult> ParticleRBDCollisionHandler::_find_earliest_particle_rbd_collision(DSD_sp dsd, const double dt) const 
{
	auto start_positions = dsd->get_vector_attribute_span("positions");
	auto updated_positions = dsd->get_vector_attribute_span("new_positions");
	auto velocities = dsd->get_vector_attribute_span("velocities");
	ParticleRBDHitResult earliest_particle_triangle_hit;
	bool hit_found = false;
	earliest_particle_triangle_hit.set_time(2.0 * dt);
	for (size_t i = 0; i < dsd->n_particles(); i++){
		for (auto& rbd: _rbds){
			AABB padded_aabb = rbd->get_padded_bounding_box(Vector(0.1, 0.1, 0.1));
			// First check if the particle path intersects the padded AABB of the rbd.
			if (!padded_aabb.contains(start_positions[i]) && !padded_aabb.contains(updated_positions[i])){
				continue;
			}
			// rbds can have a vector attribute that contains all of the indices for each face of the mesh
			auto& tri_indices = rbd->get_face_indices();
			for (auto& cato_tri_vec : tri_indices){
				//Vector tri_vec = Vector(cato_tri_vec.x(), cato_tri_vec.y(), cato_tri_vec.z());
				// We can run collision checks against each tri here
				Triangle tri_start(
					rbd->get_rotated_lever_arm(cato_tri_vec.X()),
					rbd->get_rotated_lever_arm(cato_tri_vec.Y()),
					rbd->get_rotated_lever_arm(cato_tri_vec.Z())
				);
				Triangle tri_end(
					rbd_single_particle_pos_rot_update(rbd, cato_tri_vec.X(), dt),
					rbd_single_particle_pos_rot_update(rbd, cato_tri_vec.Y(), dt),
					rbd_single_particle_pos_rot_update(rbd, cato_tri_vec.Z(), dt)
				);
				if (does_moving_particle_collide_with_moving_rbd_tri_plane(
					start_positions[i],
					updated_positions[i],
					tri_start, tri_end
				)){
					// Bisect to find the collision time
					std::optional<ParticleRBDHitResult> result = _bisect_particle_rbd_collision(
						i, start_positions[i], velocities[i], cato_tri_vec, rbd, dt
					);
					if (result && result->time < earliest_particle_triangle_hit.time){
						earliest_particle_triangle_hit = result.value();
						earliest_particle_triangle_hit.set_rbd(rbd);
						hit_found = true;
					}
				}
			}
		}
	}
	if (hit_found){
		return earliest_particle_triangle_hit;
	} else{
		return std::nullopt;
	}
}

bool ParticleRBDCollisionHandler::does_moving_particle_collide_with_moving_rbd_tri_plane(
	Vector p_start,
	Vector p_end,
	Triangle tri_start,
	Triangle tri_end
) const {
	double f0 = (p_start - tri_start.v0) * tri_start.get_normal();
	double f1 = (p_end - tri_end.v0) * tri_end.get_normal();
	//printf("f0 = %f, f1 = %f\n", f0, f1);
	return f0 * f1 < 0;
}

std::optional<ParticleRBDHitResult> ParticleRBDCollisionHandler::_bisect_particle_rbd_collision(
	size_t particle_idx,
	Vector p_start,
	Vector p_vel,
	cato::Vec3s rbd_indices,
	RB_sp rbd,
	double dt
) const{
	Triangle tri_start(
		rbd->get_rotated_lever_arm(rbd_indices.X()),
		rbd->get_rotated_lever_arm(rbd_indices.Y()),
		rbd->get_rotated_lever_arm(rbd_indices.Z())
	);
	const Vector& n = tri_start.get_normal();
	double t0 = 0, t1 = dt, f1, th;
	Vector p_end = p_start + p_vel * dt;
	// Triangle tri_end(
	// 	rbd_single_particle_pos_rot_update(rbd, rbd_indices.X(), dt),
	// 	rbd_single_particle_pos_rot_update(rbd, rbd_indices.Y(), dt),
	// 	rbd_single_particle_pos_rot_update(rbd, rbd_indices.Z(), dt)
	// );
	f1 = (p_start - tri_start.v0) * tri_start.get_normal();
	Vector particle_mid;
	Triangle tri_mid;
	for (int step = 0; step <= MAX_BISEC_ITERS; step++){
		th = (t0 + t1) / 2.0;
		tri_mid.set(
			rbd_single_particle_pos_rot_update(rbd, rbd_indices.X(), th),
			rbd_single_particle_pos_rot_update(rbd, rbd_indices.Y(), th),
			rbd_single_particle_pos_rot_update(rbd, rbd_indices.Z(), th)
		);
		Vector updated_norm = tri_mid.get_normal();
		particle_mid = p_start + p_vel * th;
		double fmid = (particle_mid - tri_mid.v0) * updated_norm;
		if (std::abs(fmid) < BISEC_TOLERANCE && fmid * f1 > 0){
			// The normal for collision surfaces should point to whatever side the particle
			// starts on.
			// still have to confirm that the collision point is on the surface
			//Vector norm = (rbd->center_of_mass - cobj->get_point_on_obj()) * n > 0 ? n : -n;
			//Vector norm = (p_start - tri_start.v0) * tri_start.get_normal() > 0 ? updated_norm : -updated_norm;
			Vector norm = updated_norm;
			Vector vec_to_surface = particle_mid - (fmid * norm);
			CollisionTriangle ct(tri_mid);
            bool is_within = ct.is_on_surface(vec_to_surface);
            if (!is_within){
				return std::nullopt;
            }
            return ParticleRBDHitResult{th, particle_idx, rbd_indices, particle_mid, norm, true};
		}
		if (f1 * fmid > 0){
			t0 = th;
		} else {
			t1 = th;
		}
	}
	// did not converge. Just return what we have
    //printf("Did not converge PARTICLE_RBD th = %f\n", th);
	return std::nullopt;
	//Vector norm = (p_start - tri_start.v0) * tri_start.get_normal() > 0 ? tri_mid.get_normal() : -tri_mid.get_normal();
	//return ParticleRBDHitResult{th, particle_idx, rbd_indices, particle_mid, norm, false};
}