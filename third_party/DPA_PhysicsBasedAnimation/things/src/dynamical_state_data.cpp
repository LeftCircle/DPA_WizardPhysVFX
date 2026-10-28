#include "dynamical_state_data.h"

using namespace pba;


void DynamicalStateData::_initialize_default_attributes() {
	// Create and cache default attributes
	_vec_attr["positions"] = DSAv("positions", Vector(0.0, 0.0, 0.0));
	_pos_map_iter = _vec_attr.find("positions");

	_vec_attr["new_positions"] = DSAv("new_positions", Vector(0.0, 0.0, 0.0));
	_updated_pos_map_iter = _vec_attr.find("new_positions");

	_vec_attr["velocities"] = DSAv("velocities", Vector(0.0, 0.0, 0.0));
	_vel_map_iter = _vec_attr.find("velocities");

	_vec_attr["acceleration"] = DSAv("acceleration", Vector(0.0, 0.0, 0.0));
	_acc_map_iter = _vec_attr.find("acceleration");

	_float_attr["mass"] = DSAf("mass", 1.0);
	_mass_map_iter = _float_attr.find("mass");

	_color_attr["color"] = DSAc("color", Color(1.0, 1.0, 1.0, 1.0));
	_color_map_iter = _color_attr.find("color");
}

