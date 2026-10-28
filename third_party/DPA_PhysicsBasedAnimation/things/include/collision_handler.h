#ifndef _COLLISION_HANDLER_H
#define _COLLISION_HANDLER_H


#include <vector>
#include <memory>
#include <string>
#include <iostream>
#include <unordered_map>
#include <optional>

#include "collision_surface.h"
#include "dynamical_state_data.h"
#include "rigid_body.h"


namespace pba{

template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

struct CollisionHandleInfo {
	CollisionHandleInfo() {}
	CollisionHandleInfo(CollisionSurface_sp cs, CollisionHitInfo hi) : 
		collision_surface(cs), hit_info(hi) {}
	CollisionSurface_sp collision_surface;
	CollisionHitInfo hit_info;
	size_t particle_idx;

	void set_time(double time) { hit_info.time_of_impact = time; }
};

struct ParticleUpdateInfo {
	ParticleUpdateInfo(Vector& sp, Vector& up, Vector& v, double r_dt) :
		start_pos(sp), updated_pos(up), velocity(v), remaining_dt(r_dt) {}
	Vector& start_pos;
	Vector& updated_pos;
	Vector& velocity;
	double remaining_dt;
};

class CollisionHandler {
public:
	CollisionHandler() = default;
	~CollisionHandler() = default;

	virtual void register_collision_surface(const CollisionSurface_sp cs) { collision_surfaces.push_back(cs); }
	virtual void handle_collisions(DSD_sp dsd, const std::string& updated_pos_attr_name, const double dt);
	void handle_collisions_no_start_pos_update(DSD_sp dsd, const std::string& updated_pos_attr, const std::string& vel_attr, const double dt);
	
protected:
	CollisionHandleInfo _find_earliest_particle_static_geo_collision(
		DSD_sp dsd,
		const std::string& updated_pos_attr_name,
		const double dt
	) const;
	
	virtual void _add_required_attributes(DSD_sp dsd) const {}
	void _handle_particle_collisions(
		size_t particle_idx,
		Vector& start_pos,
		Vector& updated_pos,
		Vector& velocity,
		const double dt
	) const;
	bool _check_for_collision_against_all_surfaces(CollisionHandleInfo& earliest_hit, CollisionHitInfo& temp_hit, ParticleUpdateInfo& pui) const;
	void _on_collision_detected(CollisionHandleInfo& earliest_hit, ParticleUpdateInfo& pui) const noexcept;
	Vector _resolve_collision_against_static_object(
		ParticleUpdateInfo& pui,
		const Vector& collision_position,
		const Vector& hit_normal,
		const double restitution, 
		const double sticky
	) const noexcept;
	void _resolve_particle_collision(DSD_sp dsd, CollisionHandleInfo& hit, double dt);

	std::vector<CollisionSurface_sp> collision_surfaces;
};

using CollisionHandler_sp =  std::shared_ptr<CollisionHandler> ;

inline CollisionHandler_sp create_collision_handler() { return std::make_shared<CollisionHandler>(); };



} // end namespace pba


#endif