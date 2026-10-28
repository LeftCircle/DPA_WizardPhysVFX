#ifndef _RIGID_BODY_H
#define _RIGID_BODY_H

#include <vector>
#include <memory>
#include <numeric>
#include <execution>
#include <algorithm> // std::transform
#include <numeric> // std::reduce

#include "the_wheel.h"
#include "dynamical_state_data.h"
#include "LinearAlgebra.h"
#include "obj_reader.h"
#include "AABB.h"


namespace pba {


class RigidBodyStateData : public DynamicalStateData {
public:
	RigidBodyStateData();
	~RigidBodyStateData() = default;

	// We need to override the add/resize methods
	// since we _should_ recompute some things from them
	size_t add() override;
	size_t add(size_t n) override;
	void resize(size_t n) override;

	void set_initial_position(size_t n, const Vector& pos);
	void create_from_obj(ObjReader<Vector>& obj, const Vector& center, const double scale = 1.0);
	
	void init_rbd();
	void scale_rbd(const double scale);

	const Vector& get_lever_arm(const size_t p) const { return _lever_arms_iter->second.get(p); }
	
	Vector& get_lever_arm(const size_t p) { return _lever_arms_iter->second.get(p); }
	Vector get_rotated_lever_arm(const size_t p) const;
	Vector get_rotated_lever_arm(const Vector& position) const;
	
	//const Vector& get_acceleration(size_t i) const noexcept { return _acc_map_iter->second.get(i); }
	//const float& get_mass(size_t i) const noexcept { return _mass_map_iter->second.get(i); }
	//void set_acceleration(size_t i, const Vector& v) { _acc_map_iter->second.set(i, v); }
	//void set_mass(size_t i, const float& m) { _mass_map_iter->second.set(i, m); }

	// Calculates the vertex position based on the lever arm, COM, and rotation
	Vector get_vert_pos(const size_t p) const;
	const float get_total_mass() const noexcept {return _total_mass; }
	const Matrix& get_moi() const noexcept { return _moment_of_inertia; }
	const Matrix& get_inverse_moi() const noexcept { return _inverse_moi; }
	const std::vector<cato::Vec3s>& get_face_indices() const noexcept { return _faces; }
	void compute_moi();

	void compute_com();
	void compute_lever_arms();
	void compute_com_for_loop();
	void compute_torque();
	AABB<Vector> get_padded_bounding_box(const Vector& padd);

	Vector center_of_mass = Vector(0, 0, 0);
	Matrix angular_rotation{1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
	Vector linear_velocity = Vector(0, 0, 0);
	Vector angular_velocity = Vector(0, 0, 0);
	Vector com_accel = Vector(0, 0, 0);
	Vector angular_accel = Vector(0, 0, 0);
	Vector angular_momentum = Vector(0, 0, 0);

private:
	void _compute_moi(int i, int j) noexcept;

	std::map< std::string, DSAv >::iterator _lever_arms_iter;
	std::vector<cato::Vec3s> _faces;
	
	Matrix _moment_of_inertia{1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
	Matrix _inverse_moi{1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
	float _total_mass = 1.0;
	void _initialize_default_attributes() override;
};

using RB_sp = std::shared_ptr<RigidBodyStateData>;



} // end namespace pba



#endif