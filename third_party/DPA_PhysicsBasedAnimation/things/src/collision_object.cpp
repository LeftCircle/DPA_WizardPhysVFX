#include "collision_object.h"

using namespace pba;

// Some internal helper functions for special cases
namespace {

bool _handle_end_collision_on_plane(
	const Vector& end_pos,
	const Vector& plane_normal,
	const double dt,
	CollisionHitInfo& hit_info
	){
	hit_info.time_of_impact = dt;
	hit_info.position = end_pos;
	hit_info.normal = plane_normal;
	return true;
}

// bool _handle_start_collision_on_plane(
// 	const Vector& start_pos,
// 	const Vector& plane_normal,
// 	const double vel_dot,
// 	CollisionHitInfo& hit_info
// 	){
// 	if (vel_dot > 0) {
// 		// No collision. The particle starts on the plane but is already moving away. 
// 		return false;
// 	} else {
// 		// We have a collision. The particle starts on the plane but is moving towards it, so the
// 		// time of impact is 0.
// 		hit_info.time_of_impact = 0.0;
// 		hit_info.position = start_pos;
// 		hit_info.normal = plane_normal;
// 		return true;
// 	}
// }

bool _handle_general_plane_collision_case(
	const Vector& start_pos,
	const Vector& velocity,
	const double dt,
	const double start_dot,
	const double end_dot,
	const Vector& plane_normal,
	const Vector& point_on_plane,
	CollisionHitInfo& hit_info
	){
	// Now we have the general collision case
	if (start_dot * end_dot > 0) {
		// The particle is on the same side of the collision plane
		return false;
	} else{
		hit_info.time_of_impact = plane_normal * ( point_on_plane - start_pos ) / ( plane_normal * velocity );
		if( (hit_info.time_of_impact * dt < 0.0) || std::abs(hit_info.time_of_impact) > std::abs(dt) ) [[unlikely]] {
			return false;
		} else{
			hit_info.position = start_pos + velocity * hit_info.time_of_impact;
			hit_info.normal = plane_normal;
			return true;
		}
	}
}

bool _point_plane_collision(
	const Vector& point_on_plane,
	const Vector& plane_normal,
	const Vector& start_pos,
	const Vector& end_pos,
	const Vector& velocity,
	const double dt,
	CollisionHitInfo& hit_info
	){
	double end_dot = (end_pos - point_on_plane) * plane_normal;
	double start_dot = (start_pos - point_on_plane) * plane_normal;
	//double vel_dot = velocity * plane_normal;
    if (std::abs(end_dot) < EPSILON) {
		return _handle_end_collision_on_plane(end_pos, plane_normal, dt, hit_info);	
    } else if (std::abs(start_dot) < EPSILON) {
		//throw(std::runtime_error("CollisionObject::_point_plane_collision: Particle starts too close to the plane. This may cause instability in the collision handling."));
		//printf("Particle starts within epsilon of collision surface. This should never happen \n");
		return _handle_general_plane_collision_case(start_pos, velocity, dt, start_dot, end_dot, plane_normal, point_on_plane, hit_info);
	} else {
		// TODO -> handle general first before checking special
		return _handle_general_plane_collision_case(start_pos, velocity, dt, start_dot, end_dot, plane_normal, point_on_plane, hit_info);
	}
}

} // end anonymous namespace

bool CollisionPlane::hit(
	const Vector& start_pos,
	const Vector& end_pos,
	const Vector& velocity,
	const double dt,
	CollisionHitInfo& hit_info) const
{
	bool hit = _point_plane_collision(_point_on_plane, _normal, start_pos, end_pos, velocity, dt, hit_info);
	if (hit){
		if ( (end_pos - start_pos) * _normal > 0 ) {
			hit_info.normal = _normal * -1.0;
		} else {
			hit_info.normal = _normal;
		}
	}
	return hit;
}

bool CollisionPlane::is_on_surface(const Vector& pos, const double threshold) const {
	double dist = (((pos - _point_on_plane) * _normal) * _normal).magnitude();
	return dist <= threshold;
}

CollisionTriangle::CollisionTriangle(const Triangle& tri)
	: _triangle(tri) 
{
	_edge1 = _triangle.v1 - _triangle.v0;
	_edge2 = _triangle.v2 - _triangle.v0;
	_normal = (_edge1 ^ _edge2);
	_normal.normalize();
	_det = _edge1 * _edge1 * _edge2 * _edge2 - (_edge1 * _edge2) * (_edge1 * _edge2);
	_one_over_area_scale = 1.0 / std::sqrt(_det);
	assert(std::abs(_det) > EPSILON); // Ensure the triangle is not degenerate
}

CollisionTriangle::CollisionTriangle(const Vector& v0, const Vector& v1, const Vector& v2)
	: CollisionTriangle(Triangle{v0, v1, v2}) {}

bool CollisionTriangle::is_on_surface(const Vector& pos, const double threshold) const {
	return _is_in_triangle(pos);
}

bool CollisionTriangle::_is_in_triangle(const Vector& p) const noexcept {
	Vector v_to_p = p - _triangle.v0;
	double u = _normal * (v_to_p ^ _edge2) * _one_over_area_scale;
	double v = _normal * (_edge1 ^ v_to_p) * _one_over_area_scale;
	double w = 1.0 - u - v;
	return (u >= 0.0 && v >= 0.0 && w >= 0.0);
}

bool CollisionTriangle::hit(
	const Vector& start_pos,
	const Vector& end_pos,
	const Vector& velocity,
	const double dt,
	CollisionHitInfo& hit_info) const 
{
	// First check for intersection with the plane of the triangle
	bool plane_hit = _point_plane_collision(_triangle.v0, _normal, start_pos, end_pos, velocity, dt, hit_info);
	if (!plane_hit) {
		return false;
	}
	// Now check if the intersection point is within the triangle
	if (_is_in_triangle(hit_info.position)) {
		Vector unit_normal = _normal.unitvector();
		if ( (end_pos - start_pos) * _normal > 0 ) {
			hit_info.normal = unit_normal * -1.0;
		} else {
			hit_info.normal = unit_normal;
		}
		return true;
	} else {
		return false;
	}
}