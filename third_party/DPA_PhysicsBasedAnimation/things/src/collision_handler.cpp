#include "collision_handler.h"

using namespace pba;



void CollisionHandler::handle_collisions(
	DSD_sp dsd,
	const std::string& updated_pos_attr_name,
	const double dt)
{
	//handle_collisions(dsd, updated_pos_attr_name, "velocities", dt);
	const size_t n = dsd->n_particles();
	auto updated_positions = dsd->get_vector_attribute_span(updated_pos_attr_name);
	auto start_positions = dsd->get_vector_attribute_span("positions");
	auto velocities = dsd->get_vector_attribute_span("velocities");
	//#pragma omp parallel for
	for( size_t i=0; i<n; i++ ){
		Vector& start_pos = start_positions[i];
		Vector& updated_pos = updated_positions[i];
		Vector& velocity = velocities[i];
		_handle_particle_collisions(i, start_pos, updated_pos, velocity, dt);
		// Now set the final position to the updated one
		start_positions[i] = updated_positions[i];
	}
}

void CollisionHandler::handle_collisions_no_start_pos_update(
	DSD_sp dsd,
	const std::string& updated_pos_attr,
	const std::string& vel_attr,
	const double dt)
{
	const size_t n = dsd->n_particles();
	auto updated_positions = dsd->get_vector_attribute_span(updated_pos_attr);
	auto start_positions = dsd->get_vector_attribute_span("positions");
	auto velocities = dsd->get_vector_attribute_span(vel_attr);
	//#pragma omp parallel for
	for( size_t i=0; i<n; i++ ){
		Vector start_pos = start_positions[i];
		Vector& updated_pos = updated_positions[i];
		Vector& velocity = velocities[i];
		_handle_particle_collisions(i, start_pos, updated_pos, velocity, dt);
		// Now set the final position to the updated one
		start_positions[i] = updated_positions[i];
	}

}


void CollisionHandler::_handle_particle_collisions(
	size_t particle_idx,
	Vector& start_pos,
	Vector& updated_pos,
	Vector& velocity,
	const double dt) const 
{
	bool keep_checking = true;
	CollisionHandleInfo earliest_hit;
	CollisionHitInfo temp_hit;
	ParticleUpdateInfo pui(start_pos, updated_pos, velocity, dt);
	while (keep_checking && std::abs(pui.remaining_dt) > EPSILON){
		keep_checking = _check_for_collision_against_all_surfaces(earliest_hit, temp_hit, pui);
		if( keep_checking ){
			_on_collision_detected(earliest_hit, pui);
		}
	}
}

bool CollisionHandler::_check_for_collision_against_all_surfaces(
	CollisionHandleInfo& earliest_hit,
	CollisionHitInfo& temp_hit,
	ParticleUpdateInfo& pui) const
{
	bool keep_checking = false;
	earliest_hit.hit_info.time_of_impact = 2.0 * pui.remaining_dt;
	temp_hit.time_of_impact = 2.0 * pui.remaining_dt;
	for( const auto& cs : collision_surfaces ){
		// if there is no hit, cs -> packs 2.0 * pui.remaining_dt into temp_hit
		bool hit = cs->hit(pui.start_pos, pui.updated_pos, pui.velocity, pui.remaining_dt, temp_hit);
		bool earlier_hit = std::abs(temp_hit.time_of_impact) < std::abs(earliest_hit.hit_info.time_of_impact);
		if( hit && earlier_hit){
			earliest_hit.hit_info = temp_hit;
			earliest_hit.collision_surface = cs;
			keep_checking = true;
		}
	}
	return keep_checking;
}

CollisionHandleInfo CollisionHandler::_find_earliest_particle_static_geo_collision(
	DSD_sp dsd,
	const std::string& updated_pos_attr_name,
	const double dt) const
{
	CollisionHandleInfo earliest_hit;
	earliest_hit.set_time(2.0 * dt);
	auto updated_positions = dsd->get_vector_attribute_span(updated_pos_attr_name);
	auto start_positions = dsd->get_vector_attribute_span("positions");
	auto velocities = dsd->get_vector_attribute_span("velocities");
	for (size_t i = 0; i < dsd->n_particles(); i++){
		CollisionHitInfo temp_hit;
		Vector& start_pos = start_positions[i];
		Vector& updated_pos = updated_positions[i];
		Vector& velocity = velocities[i];
		ParticleUpdateInfo pui(start_pos, updated_pos, velocity, dt);
		CollisionHandleInfo this_particle_earliest_hit;
		bool particle_collision = _check_for_collision_against_all_surfaces(this_particle_earliest_hit, temp_hit, pui);
		if (this_particle_earliest_hit.hit_info.time_of_impact < earliest_hit.hit_info.time_of_impact){
			earliest_hit = this_particle_earliest_hit;
			earliest_hit.particle_idx = i;
		}
	}
	return earliest_hit;
}

void CollisionHandler::_on_collision_detected(CollisionHandleInfo& earliest_hit, ParticleUpdateInfo& pui) const noexcept{
	pui.remaining_dt = pui.remaining_dt - earliest_hit.hit_info.time_of_impact;
	pui.updated_pos = _resolve_collision_against_static_object(
		pui,
		earliest_hit.hit_info.position,
		earliest_hit.hit_info.normal,
		earliest_hit.collision_surface->get_restitution(),
		earliest_hit.collision_surface->get_sticky()
	);
	// Now set the start position to the collision position for the next iteration
	// plus a very small epsilon to prevent rehitting the same or similar surfaces
	pui.start_pos = earliest_hit.hit_info.position + earliest_hit.hit_info.normal * MIN_END_DIST_FROM_COLLISION;
}

void CollisionHandler::_resolve_particle_collision(DSD_sp dsd, CollisionHandleInfo& hit, double dt){
	ParticleUpdateInfo info(
		dsd->get_position(hit.particle_idx),
		dsd->get_updated_position(hit.particle_idx),
		dsd->get_velocity(hit.particle_idx),
		dt - hit.hit_info.time_of_impact
	);
	info.updated_pos = _resolve_collision_against_static_object(
		info,
		hit.hit_info.position,
		hit.hit_info.normal,
		hit.collision_surface->get_restitution(),
		hit.collision_surface->get_sticky()
	);
	info.start_pos = hit.hit_info.position + hit.hit_info.normal * MIN_END_DIST_FROM_COLLISION;
}

Vector CollisionHandler::_resolve_collision_against_static_object(
	ParticleUpdateInfo& pui,
	const Vector& collision_position,
	const Vector& hit_normal,
	const double restitution, 
	const double sticky
) const noexcept {
	// Determine the new velocity after collision
	pui.velocity = sticky * pui.velocity  - (sticky + restitution) * (pui.velocity * hit_normal) * hit_normal;
	Vector new_position = collision_position + pui.velocity * pui.remaining_dt;
	
	double dist_to_plane = (new_position - collision_position) * hit_normal;
	if (dist_to_plane < MIN_END_DIST_FROM_COLLISION) {
		new_position += hit_normal * (MIN_END_DIST_FROM_COLLISION);
	}
	return new_position;
}
