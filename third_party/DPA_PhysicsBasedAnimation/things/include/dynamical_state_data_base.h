#ifndef _DYNAMICAL_STATE_DATA_BASE_H
#define _DYNAMICAL_STATE_DATA_BASE_H

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

namespace pba{

class DynamicalStateDataBase {
public:

    DynamicalStateDataBase() = default;
    virtual ~DynamicalStateDataBase() = default;
	
	virtual size_t add();
	virtual size_t add(size_t n);

	virtual void resize(size_t n);
	virtual void clear();

	// accessors for attributes by name
	const Vector& get_vector_attribute(const std::string& name, size_t i) const;
	const float& get_float_attribute(const std::string& name, size_t i) const;
	const double& get_double_attribute(const std::string& name, size_t i) const;
	const int& get_int_attribute(const std::string& name, size_t i) const;
	const Color& get_color_attribute(const std::string& name, size_t i) const;

	// Span for all attributes
	span<const Vector> get_vector_attribute_span(const std::string& name) const;
	span<const float> get_float_attribute_span(const std::string& name) const;
	span<const double> get_double_attribute_span(const std::string& name) const;
	span<const int> get_int_attribute_span(const std::string& name) const;
	span<const Color> get_color_attribute_span(const std::string& name) const;

	span<Vector> get_vector_attribute_span(const std::string& name);
	span<float> get_float_attribute_span(const std::string& name);
	span<double> get_double_attribute_span(const std::string& name);
	span<int> get_int_attribute_span(const std::string& name);
	span<Color> get_color_attribute_span(const std::string& name);

	// setters for attributes by name
	void set_vector_attribute(const std::string& name, size_t i, const Vector& v);
	void set_float_attribute(const std::string& name, size_t i, const float& f);
	void set_double_attribute(const std::string& name, size_t i, const double& d);
	void set_int_attribute(const std::string& name, size_t i, const int& val);
	void set_color_attribute(const std::string& name, size_t i, const Color& c);

	size_t n_particles() const noexcept { return _n_particles; }
	
	template<typename T>
	void add_attribute(const std::string& name, DSAttribute<T>&& attr){
		attr.expand_to(_n_particles);
		if constexpr (std::is_same_v<T, int>) {
			_int_attr.insert_or_assign(name, std::move(attr));
		} else if constexpr (std::is_same_v<T, float>) {
			_float_attr.insert_or_assign(name, std::move(attr));
		} else if constexpr (std::is_same_v<T, Vector>) {
			_vec_attr.insert_or_assign(name, std::move(attr));
		} else if constexpr (std::is_same_v<T, Color>) {
			_color_attr.insert_or_assign(name, std::move(attr));
		} else if constexpr (std::is_same_v<T, double>) {
			_double_attr.insert_or_assign(name, std::move(attr));
		} else {
			throw std::runtime_error("Unsupported attribute type added");
		}
	}

	template<typename T>
	void add_attribute(const std::string& name, const DSAttribute<T>& attr){
		DSAttribute<T> new_attr = attr;
		add_attribute<T>(name, std::move(new_attr));
	}

	bool has_int_attribute(const std::string& name) const{
		return _int_attr.find(name) !=  _int_attr.end();
	}
	bool has_float_attribute(const std::string& name) const {
		return _float_attr.find(name) !=  _float_attr.end();
	}
	bool has_vector_attribute(const std::string& name) const {
		return _vec_attr.find(name) !=  _vec_attr.end();
	}
	bool has_color_attribute(const std::string& name) const {
		return _color_attr.find(name) !=  _color_attr.end();
	}
	bool has_double_attribute(const std::string& name) const {
		return _double_attr.find(name) !=  _double_attr.end();
	}

protected:
	size_t _n_particles = 0;
	std::map< std::string, DSAi > _int_attr;
	std::map< std::string, DSAf > _float_attr;
	std::map< std::string, DSAv > _vec_attr;
	std::map< std::string, DSAc > _color_attr;
	std::map< std::string, DSAd > _double_attr;
    
	virtual void _initialize_default_attributes() {}
	
	template<typename MapType>
	static auto& _lookup_or_throw(MapType& m, const std::string& name, const std::string& type){
		if (auto it = m.find(name); it != m.end()) return it->second;
		else throw std::runtime_error("No " + type + " attribute with name: " + name);
	}

	void _resize_all_attributes(size_t n);

};

using DynamicalStateDataBase_sp = std::shared_ptr<DynamicalStateDataBase>;
using DSDB_sp = std::shared_ptr<DynamicalStateDataBase>;

inline DynamicalStateDataBase_sp create_dynamical_state_data_base() { return std::make_shared<DynamicalStateDataBase>(); };

}; // end namespace pba

#endif