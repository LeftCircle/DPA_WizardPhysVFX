#include "dynamical_state_data_base.h"

using namespace pba;


size_t DynamicalStateDataBase::add(){
	_n_particles += 1;
	_resize_all_attributes(_n_particles);
	return _n_particles;
}

size_t DynamicalStateDataBase::add(size_t n){
	_n_particles += n;
	_resize_all_attributes(_n_particles);
	return _n_particles;
}

void DynamicalStateDataBase::resize(size_t n){
	_n_particles = n;
	_resize_all_attributes(_n_particles);
}

const Vector& DynamicalStateDataBase::get_vector_attribute(const std::string& name, size_t i) const {
	return _lookup_or_throw(_vec_attr, name, "vector").get(i);
}

const float& DynamicalStateDataBase::get_float_attribute(const std::string& name, size_t i) const {
	return _lookup_or_throw(_float_attr, name, "float").get(i);
}

const double& DynamicalStateDataBase::get_double_attribute(const std::string& name, size_t i) const {
	return _lookup_or_throw(_double_attr, name, "double").get(i);
}

const int& DynamicalStateDataBase::get_int_attribute(const std::string& name, size_t i) const {
	return _lookup_or_throw(_int_attr, name, "int").get(i);
}

const Color& DynamicalStateDataBase::get_color_attribute(const std::string& name, size_t i) const {
	return _lookup_or_throw(_color_attr, name, "color").get(i);
}

void DynamicalStateDataBase::set_vector_attribute(const std::string& name, size_t i, const Vector& v) {
	_lookup_or_throw(_vec_attr, name, "vector").set(i, v);
}

void DynamicalStateDataBase::set_float_attribute(const std::string& name, size_t i, const float& f) {
	_lookup_or_throw(_float_attr, name, "float").set(i, f);
}

void DynamicalStateDataBase::set_double_attribute(const std::string& name, size_t i, const double& d) {
	_lookup_or_throw(_double_attr, name, "double").set(i, d);
}

void DynamicalStateDataBase::set_int_attribute(const std::string& name, size_t i, const int& val) {
	_lookup_or_throw(_int_attr, name, "int").set(i, val);
}

void DynamicalStateDataBase::set_color_attribute(const std::string& name, size_t i, const Color& c) {
	_lookup_or_throw(_color_attr, name, "color").set(i, c);
}

span<const Vector> DynamicalStateDataBase::get_vector_attribute_span(const std::string& name) const {
	return _lookup_or_throw(_vec_attr, name, "vector").get_span();
}

span<const float> DynamicalStateDataBase::get_float_attribute_span(const std::string& name) const {
	return _lookup_or_throw(_float_attr, name, "float").get_span();
}

span<const double> DynamicalStateDataBase::get_double_attribute_span(const std::string& name) const {
	return _lookup_or_throw(_double_attr, name, "double").get_span();
}

span<const int> DynamicalStateDataBase::get_int_attribute_span(const std::string& name) const {
	return _lookup_or_throw(_int_attr, name, "int").get_span();
}

span<const Color> DynamicalStateDataBase::get_color_attribute_span(const std::string& name) const {
	return _lookup_or_throw(_color_attr, name, "color").get_span();
}

span<Vector> DynamicalStateDataBase::get_vector_attribute_span(const std::string& name) {
	return _lookup_or_throw(_vec_attr, name, "vector").get_span();
}

span<float> DynamicalStateDataBase::get_float_attribute_span(const std::string& name) {
	return _lookup_or_throw(_float_attr, name, "float").get_span();
}

span<double> DynamicalStateDataBase::get_double_attribute_span(const std::string& name) {
	return _lookup_or_throw(_double_attr, name, "double").get_span();
}

span<int> DynamicalStateDataBase::get_int_attribute_span(const std::string& name) {
	return _lookup_or_throw(_int_attr, name, "int").get_span();
}

span<Color> DynamicalStateDataBase::get_color_attribute_span(const std::string& name) {
	return _lookup_or_throw(_color_attr, name, "color").get_span();
}

void DynamicalStateDataBase::_resize_all_attributes(size_t n){
	for( auto& [name, attr] : _int_attr ){
		attr.expand_to(_n_particles);
	}
	for( auto& [name, attr] : _float_attr ){
		attr.expand_to(_n_particles);
	}
	for( auto& [name, attr] : _vec_attr ){
		attr.expand_to(_n_particles);
	}
	for( auto& [name, attr] : _color_attr ){
		attr.expand_to(_n_particles);
	}
	for( auto& [name, attr] : _double_attr ){
		attr.expand_to(_n_particles);
	}
}

void DynamicalStateDataBase::clear(){
for( auto& [name, attr] : _int_attr ){
		attr.clear();
	}
	for( auto& [name, attr] : _float_attr ){
		attr.clear();
	}
	for( auto& [name, attr] : _vec_attr ){
		attr.clear();
	}
	for( auto& [name, attr] : _color_attr ){
		attr.clear();
	}
	for( auto& [name, attr] : _double_attr ){
		attr.clear();
	}
	_n_particles = 0;
}





