#ifndef _DYNAMICAL_STATE_DATA_H
#define _DYNAMICAL_STATE_DATA_H

#include <map>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <variant>

#include "the_wheel.h"
#include "Vector.h"
#include "Color.h"
#include "dynamical_state_attribute.h"
#include "dynamical_state_data_base.h"

namespace pba{

class DynamicalStateData : public DynamicalStateDataBase {
public:

    DynamicalStateData() {_initialize_default_attributes(); };
    virtual ~DynamicalStateData() = default;

	// accessors for common attributes
	const Vector& get_position(size_t i) const { return _pos_map_iter->second.get(i); }
	const Vector& p(size_t i) const { return get_position(i); }
	const Vector& get_updated_position(size_t i) const { return _updated_pos_map_iter->second.get(i); }
	const Vector& get_velocity(size_t i) const { return _vel_map_iter->second.get(i); }
	const Vector& get_acceleration(size_t i) const { return _acc_map_iter->second.get(i); }
	const Vector& a(size_t i) const { return get_acceleration(i); }
	const float& get_mass(size_t i) const { return _mass_map_iter->second.get(i); }
	const Color& get_color(size_t i) const { return _color_map_iter->second.get(i); }
	
	Vector& get_position(size_t i) { return _pos_map_iter->second.get(i); }
	Vector& p(size_t i) { return get_position(i); }
	Vector& get_updated_position(size_t i) { return _updated_pos_map_iter->second.get(i); }
	Vector& get_velocity(size_t i) { return _vel_map_iter->second.get(i); }
	Vector& get_acceleration(size_t i) { return _acc_map_iter->second.get(i); }
	Vector& a(size_t i) { return get_acceleration(i); }
	float& get_mass(size_t i) { return _mass_map_iter->second.get(i); }
	Color& get_color(size_t i) { return _color_map_iter->second.get(i); }

	// setters for common attributes
	void set_position(size_t i, const Vector& v) { _pos_map_iter->second.set(i, v); }
	void set_updated_position(size_t i, const Vector& v) {_updated_pos_map_iter->second.set(i, v); }
	void set_velocity(size_t i, const Vector& v) { _vel_map_iter->second.set(i, v); }
	void set_acceleration(size_t i, const Vector& v) { _acc_map_iter->second.set(i, v); }
	void set_mass(size_t i, const float& m) { _mass_map_iter->second.set(i, m); }
	void set_color(size_t i, const Color& c) { _color_map_iter->second.set(i, c); }

protected:
	// caching these to avoid map lookup for commonly used lookups
	std::map< std::string, DSAv >::iterator _pos_map_iter;
	std::map< std::string, DSAv >::iterator _updated_pos_map_iter;
	std::map < std::string, DSAv >::iterator _vel_map_iter;
	std::map < std::string, DSAv >::iterator _acc_map_iter;
	std::map < std::string, DSAf >::iterator _mass_map_iter;
	std::map < std::string, DSAc >::iterator _color_map_iter;
	//std::string _name;

	virtual void _initialize_default_attributes() override;
};

using DynamicalStateData_sp = std::shared_ptr<DynamicalStateData>;
using DSD_sp = std::shared_ptr<DynamicalStateData>;
using DSD_scp = std::shared_ptr<const DynamicalStateData>;

inline DynamicalStateData_sp create_dynamical_state_data() { return std::make_shared<DynamicalStateData>(); };

}; // end namespace pba

#endif