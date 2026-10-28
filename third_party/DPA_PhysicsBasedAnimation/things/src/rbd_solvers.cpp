#include "rbd_solvers.h"

using namespace pba;

Vector pba::rbd_single_particle_pos_rot_update(const RB_sp& rbd, const size_t idx, const double dt){
    Vector rotor = rbd->angular_velocity * dt;
    double angle = rotor.magnitude();
    Matrix angular_rotation = (angle > 1e-12)
        ? rotation(rotor / angle, -angle) * rbd->angular_rotation
        : rbd->angular_rotation;
    Vector center_of_mass = rbd->center_of_mass + rbd->linear_velocity * dt;
    return angular_rotation * rbd->get_lever_arm(idx) + center_of_mass;
}

void AdvanceRotationAndCOM::solve(const double dt) {
	solve(_rbd, dt);
    auto pos = _rbd->get_vector_attribute_span("positions");
    auto updated_pos = _rbd->get_vector_attribute_span("new_positions");
    std::copy(std::execution::par, updated_pos.begin(), updated_pos.end(), pos.begin());
}

AdvanceRotationAndCOMWithCollisions::AdvanceRotationAndCOMWithCollisions(
	RB_sp rbd,
	std::shared_ptr<RBDCollisionHandler> rbd_ch
    ) : AdvanceRotationAndCOM(rbd), rbd_coll_handler(rbd_ch)
{
    if (!_rbd->has_vector_attribute("new_positions")){
        _rbd->add_attribute<Vector>("new_positions", DSAv());
    }
}

void AdvanceRotationAndCOMWithCollisions::solve(const double dt) {
    rbd_coll_handler->handle_collisions(_rbd, "new_positions", dt);
}

void AdvanceRotationAndCOM::solve(RB_sp rbd, const double dt) {
    Vector rotor = rbd->angular_velocity * dt;
    double angle = rotor.magnitude();
    if (angle > 1e-12){
        rbd->angular_rotation = rotation(rotor.unitvector(), -angle) * rbd->angular_rotation;
    }
    rbd->center_of_mass += rbd->linear_velocity * dt;
    rbd->compute_moi();
}


void AdvanceAngularVelocityAndVelocity::solve(const double dt){
    _torque.compute(_rbd, dt); // updates angular accel and center of mass acceleration
    _rbd->angular_momentum += _rbd->angular_accel * dt;
    _rbd->angular_velocity = _rbd->get_inverse_moi() * _rbd->angular_momentum;
    _rbd->linear_velocity += _rbd->com_accel * dt;
}

void RBDCollisionHandler::handle_collisions(
	DSD_sp dsd,
	const std::string& updated_pos_attr_name,
	const double dt
) {
	auto rbd_sp = std::dynamic_pointer_cast<RigidBodyStateData>(dsd);
	if (!rbd_sp) throw std::runtime_error("RBDCollision handler requires rbd");
    RBDHitResult min_hit{2 * dt, 0, Vector(0, 0, 0), Vector(0, 0, 0), nullptr, false, Vector(0, 0, 0)};
	_handle_rbd_collisions(rbd_sp, min_hit, dt);
}

void RBDCollisionHandler::_handle_rbd_collisions(RB_sp rbd, RBDHitResult& min_hit, double dt) {
	const size_t n = rbd->n_particles();
	auto masses = rbd->get_float_attribute_span("mass");
    bool colliding = true;
    int iter = 0;
    const double start_dt = dt;
    while (colliding){
        bool hit_found = _find_earliest_rbd_static_collision(rbd, min_hit, dt);
        // now we have the earliest time and hit position
        if (hit_found){
            AdvanceRotationAndCOM::solve(rbd, min_hit.time);
            _resolve_collision(rbd, min_hit, dt);
            dt = min_hit.time;
        } else {
            colliding = false;
            // no collision! 
            // finish advancing
            AdvanceRotationAndCOM::solve(rbd, dt);
            return;
        }
        // if (iter > 100){
        //     float n_dot_vec_from_plane_to_hit_pos = min_hit.normal * (min_hit.position - min_hit.point_on_cobj);
        //     rbd->center_of_mass += 4 * n_dot_vec_from_plane_to_hit_pos * min_hit.normal;
        //     printf("Itered out\n");
        //     return;
        // }
    }
}

bool RBDCollisionHandler::_find_earliest_rbd_static_collision(RB_sp rbd, RBDHitResult& min_hit, double dt) const {
	bool hit_found = false;
	for (size_t i = 0; i < rbd->n_particles(); i++){
		for (auto& cs : collision_surfaces){
			const std::vector<CollisionObject_sp>& cobjs = cs->get_collision_objects();
			for(size_t j = 0; j < cobjs.size(); j++){
				std::optional<RBDHitResult> hit = bisect_collision(rbd, i, cobjs[j], cs, dt);
				if (hit && std::abs(hit->time) < std::abs(min_hit.time)){
					min_hit = hit.value();
					hit_found = true;
				}
			} // end looping through collision objects
		}  // end looping through collision surfaces
	}
	return hit_found;
}

void RBDCollisionHandler::_resolve_collision(RB_sp rbd, RBDHitResult& min_hit, double dt){
    // now let's update the com position and rotation to the hit point
    // Now some helpers post moving to collision point
    double ks = min_hit.surface->get_sticky();
    double kr = min_hit.surface->get_restitution();
    Vector& lv = rbd->linear_velocity;
    const Vector& n = min_hit.normal;
    const Vector& r = rbd->get_rotated_lever_arm(min_hit.particle);
    Vector& w = rbd->angular_velocity;
    const float M = rbd->get_total_mass();
    Vector rxn = r ^ n;
    
    // Now we have to make the rbd bounce
    double A_numerator = -2.0 * (rbd->linear_velocity * n + rbd->angular_velocity * rxn);
    //double A_numerator = -(2 * rbd->linear_velocity * n + w * rxn + (rbd->get_moi() * w) * (rbd->get_inverse_moi() * rxn));
    double A_denom = (1.0 / M) + rxn * rbd->get_inverse_moi() * rxn;
    double A = A_numerator / A_denom;
    //A = std::abs(A);
    //printf("rbd A = %f\n", A);
    //double A = _get_A(rbd, min_hit.particle, min_hit.normal);

    // Now update pos and rotation with the bounce
    rbd->linear_velocity += A / rbd->get_total_mass() * min_hit.normal;
    rbd->angular_velocity += A * rbd->get_inverse_moi() * rxn;
    
    // Restitution and Sticky
    rbd->linear_velocity = (ks * (rbd->linear_velocity - (rbd->linear_velocity * n) * n)) + kr * (rbd->linear_velocity * n) * n;
    rbd->angular_velocity *= (ks + kr) / 2.0;

    // Angular momentum update!!!
    rbd->angular_momentum = rbd->get_moi() * rbd->angular_velocity;

    // Now we have to update position/rotation
    double time_left = dt - min_hit.time;
    dt = time_left;
    min_hit.time = time_left;
    
    // move it up a smidgen if no convergence. Keeps all particles on the same side
    // of the collision plane
    // float n_dot_vec_from_plane_to_hit_pos = min_hit.normal * (min_hit.position - min_hit.point_on_cobj);
    // if (n_dot_vec_from_plane_to_hit_pos < 0 && !min_hit.converged){//} &&
    //     rbd->center_of_mass += 8 * n_dot_vec_from_plane_to_hit_pos * min_hit.normal;
    //     printf("Adjusted COM\n");
    // }
    // // This is a bit of a hack to avoid an infinite loop and to keep the bunny from freezing
    // // when it is on a surface
    // if (std::abs(min_hit.time) < 0.00000001){
    //     printf("Hit time near zero");
    //     rbd->center_of_mass += 0.000001 * min_hit.normal;
    // }
}

double RBDCollisionHandler::_get_A(RB_sp rbd, size_t particle_idx, Vector& n) const{
    Vector& lv = rbd->linear_velocity;
    const Vector& r = rbd->get_rotated_lever_arm(particle_idx);
    Vector& w = rbd->angular_velocity;
    const float M = rbd->get_total_mass();
    
    Vector rxn = r ^ n;
    double A_numerator = 2.0 * (lv * n) + w * rxn;
    double A_denom = (1.0 / M) + rxn * rbd->get_inverse_moi() * rxn;
    double A = -A_numerator / A_denom;
}

std::optional<RBDHitResult> pba::bisect_collision(
	const RB_sp& rbd, size_t particle_idx,
	const CollisionObject_sp& cobj, CollisionSurface_sp cs,
	double max_t)
{
	const Vector& n = cobj->get_normal();
	double f_start = n * (rbd->get_vert_pos(particle_idx) - cobj->get_point_on_obj());
	// predict x_end for JUST this one particle
	Vector x_end = rbd_single_particle_pos_rot_update(rbd, particle_idx, max_t);
	double f_end = n * (x_end - cobj->get_point_on_obj());

	// Early out if no collisions
	if (f_start * f_end > 0){
		return std::nullopt;
	}
	// We have a collision! Time to bisect
	double t0 = 0, t1 = max_t, f1 = f_start, th;
	Vector x_mid;
	for (int step = 0; step <= MAX_BISEC_ITERS; step++){
		th = (t0 + t1) / 2.0;
		x_mid = rbd_single_particle_pos_rot_update(rbd, particle_idx, th);
		double fmid = (x_mid - cobj->get_point_on_obj()) * n;
		if (std::abs(fmid) < BISEC_TOLERANCE && fmid * f1 > 0){
            // We need to check if the collision is actually on the object.
            // like for a triangle if it is within the barycentric coords
            // We should project the point to the plane that the triangle is on first. 
            Vector norm = (rbd->center_of_mass - cobj->get_point_on_obj()) * n > 0 ? n : -n;
            Vector vec_to_surface = x_mid - (fmid * norm);
            bool is_within = cobj->is_on_surface(vec_to_surface);
            if (!is_within){
                return std::nullopt;
            }
            return RBDHitResult{th, particle_idx, x_mid, norm, cs, true, cobj->get_point_on_obj()};
		}
		if (f1 * fmid > 0){
			t0 = th;
		} else {
			t1 = th;
		}
	}
	// did not converge. Just return what we have
    //printf("Did not converge th = %f\n", th);
    return std::nullopt;
	//Vector norm = (rbd->center_of_mass - x_mid) * n > 0 ? n : -n;
	//return RBDHitResult{th, particle_idx, x_mid, norm, cs, false, cobj->get_point_on_obj()};
}

