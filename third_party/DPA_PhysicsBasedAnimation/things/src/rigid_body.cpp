#include "rigid_body.h"

using namespace pba;

struct MassMoment{
	Vector moment;
	float mass;
};

void RigidBodyStateData::init_rbd() {
	compute_com();
	compute_lever_arms();
	angular_momentum = Matrix(1, 0, 0, 0, 1, 0, 0, 0, 1) * angular_velocity;
}

RigidBodyStateData::RigidBodyStateData(){
	_initialize_default_attributes();
}

void RigidBodyStateData::_initialize_default_attributes() 
{
	DynamicalStateData::_initialize_default_attributes();
	add_attribute<Vector>("initial_positions", DSAv());
	
	add_attribute<Vector>("lever_arms", DSAv());
	_lever_arms_iter = _vec_attr.find("lever_arms");
}

void RigidBodyStateData::set_initial_position(size_t p, const Vector& pos) {
	set_vector_attribute("initial_positions", p, pos);
	// TO DO -> optimize
	//init_rbd();
}

size_t RigidBodyStateData::add() {
	auto size = DynamicalStateDataBase::add();
	// TO DO -> optimize instead of recalculating everything
	//init_rbd();
	return size;
}

size_t RigidBodyStateData::add(size_t n){
	auto size = DynamicalStateDataBase::add(n);
	//init_rbd();
	return size;
}

void RigidBodyStateData::resize(size_t n){
	DynamicalStateDataBase::resize(n);
	//init_rbd();
}

void RigidBodyStateData::create_from_obj(ObjReader<Vector>& obj, const Vector& center, const double scale){
    auto verts = obj.get_verts();
    auto faces = obj.get_faces();
    for (size_t i = 0; i < obj.get_verts().size(); i++){
        add();
		Vector pos = verts[i] + center;
        set_initial_position(i, pos);
		set_position(i, pos);
		set_mass(i, 0.05);
    }
	_faces.resize(faces.size());
	for (size_t i = 0; i < faces.size(); i++){
		_faces[i] = cato::Vec3s(faces[i].x(), faces[i].y(), faces[i].z());
	}
    init_rbd();
	scale_rbd(scale);
	init_rbd();
}

void RigidBodyStateData::scale_rbd(const double scale){
	// adjust the default position and lever arms
	if (scale == 1.0) { return; }
	for (size_t i = 0; i < _n_particles; i++){
		Vector lever_arm = get_lever_arm(i);
		lever_arm *= scale;
		set_initial_position(i, center_of_mass + lever_arm);
	}
}

Vector RigidBodyStateData::get_vert_pos(size_t p) const {
	return angular_rotation * get_lever_arm(p) + center_of_mass;
}


Vector RigidBodyStateData::get_rotated_lever_arm(const size_t p) const{
	return angular_rotation * get_lever_arm(p);
}

Vector RigidBodyStateData::get_rotated_lever_arm(const Vector& pos) const {
	return pos - center_of_mass;
}

AABB<Vector> RigidBodyStateData::get_padded_bounding_box(const Vector& padd) {
	AABB<Vector> box;
	for (size_t i = 0; i < _n_particles; i++){
		Vector vert_pos = get_vert_pos(i);
		box.expand_to_include(vert_pos);
	}
	box.expand_to_include(box.lower_left() - padd);
	box.expand_to_include(box.upper_right() + padd);
	return box;
}

void RigidBodyStateData::compute_com() {
	auto masses = get_float_attribute_span("mass");
	auto positions = get_vector_attribute_span("initial_positions");
	// TODO -> implement a solution with std::ranges that zips positions and masses.
	MassMoment result = std::accumulate(masses.begin(), masses.end(), MassMoment(),
		[&positions, i = 0](MassMoment acc, float mass) mutable {
			acc.mass += mass;
			acc.moment += positions[i] * mass;
			i++;
			return acc;
		}
	);

	if (result.mass > 0.0)
	{
		center_of_mass = result.moment / result.mass;
	}
	else
	{
		center_of_mass = Vector(0,0,0);
	}
	_total_mass = result.mass;
}

void RigidBodyStateData::compute_lever_arms() {
	auto larm_span = get_vector_attribute_span("lever_arms");
	auto positions = get_vector_attribute_span("initial_positions");
	const Vector& com = center_of_mass;
	// just a basic transform
	std::transform(std::execution::par, positions.begin(), positions.end(), larm_span.begin(), 
		[&com](const Vector& pos){
			return pos - com;
		});
}

void RigidBodyStateData::compute_moi(){
	for (int i = 0; i <= 2; i++){
		for (int j = 0; j <= 2; j++){
			_compute_moi(i, j);
		}
	}
	_inverse_moi = inverse(_moment_of_inertia);
}

void RigidBodyStateData::_compute_moi(int i, int j) noexcept {
	auto masses = get_float_attribute_span("mass");

	// TODO -> implement a solution with std::ranges that zips positions and masses.
	double res = 0;
	for (size_t index = 0; index < _n_particles; index++){
		auto larm = get_rotated_lever_arm(index);
		double delta = i == j ? 1.0 : 0.0;
		double mag = larm.magnitude();
		res += (double)masses[index] * ((delta * mag * mag) - (larm[i] * larm[j]));
	}
	_moment_of_inertia.Set(i, j, res);
}

void RigidBodyStateData::compute_com_for_loop() {
	float total_mass = 0.0;
	auto masses = get_float_attribute_span("mass");
	auto positions = get_vector_attribute_span("positions");
	Vector pos_times_mass = Vector(0, 0, 0);
	for (size_t i = 0; i < _n_particles; i++){
		pos_times_mass += masses[i] * positions[i];
		total_mass += masses[i];
	}
	if (total_mass > 0.0)
	{
		center_of_mass = pos_times_mass / total_mass;
	}
	else
	{
		center_of_mass = Vector(0,0,0);
	}
}

