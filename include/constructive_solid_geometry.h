#pragma once

#include <vector>

#include "volume.h"
#include "field_operations.h"



namespace lux{

class UnionField : public FieldOperator<float, float>{
public:
	using typename Volume<float>::volumeDataType;
	using typename Volume<float>::volumeGradType;

	UnionField(const VolumeSPtr<float> a, const VolumeSPtr<float> b) : 
						FieldOperator<float, float>(std::move(a), std::move(b)) {}

	const volumeDataType eval(const Vector& p) const override;
	const volumeGradType grad(const Vector& p) const override;
};


class IntersectionField : public FieldOperator<float, float>{
public:
	using typename Volume<float>::volumeDataType;
	using typename Volume<float>::volumeGradType;

	IntersectionField(const VolumeSPtr<float> a, const VolumeSPtr<float> b) : 
						FieldOperator<float, float>(std::move(a), std::move(b)) {}

	const volumeDataType eval(const Vector& p) const override;

	const volumeGradType grad(const Vector& p) const override;
};


class CutoutField : public FieldOperator<float, float>{
public:
	using typename Volume<float>::volumeDataType;
	using typename Volume<float>::volumeGradType;

	CutoutField(const VolumeSPtr<float> a, const VolumeSPtr<float> b) : 
						FieldOperator<float, float>(std::move(a), std::move(b)) {}

	const volumeDataType eval(const Vector& p) const override;

	const volumeGradType grad(const Vector& p) const override;
};

class MaskField : public FieldOperator<float>{
public:
	using typename Volume<float>::volumeDataType;
	using typename Volume<float>::volumeGradType;

	MaskField(const VolumeSPtr<float> a) : 
						FieldOperator<float>(std::move(a)) {}

	const volumeDataType eval(const Vector& p) const override;
};


class ClampField : public FieldOperator<float, float, float>{
public:
	using typename Volume<float>::volumeDataType;
	using typename Volume<float>::volumeGradType;

	ClampField(const VolumeSPtr<float> a, const VolumeSPtr<float> min, const VolumeSPtr<float> max) : 
						FieldOperator<float, float, float>(std::move(a), std::move(min), std::move(max)) {}

	const volumeDataType eval(const Vector& p) const override;
};

class BlinnBlendField : public Volume<float>{
public:
	using typename Volume<float>::volumeDataType;
    using typename Volume<float>::volumeGradType;

	BlinnBlendField(std::shared_ptr<const std::vector<vspf>> fields, float blend_factor, float shape_broadness) : 
					_fields(std::move(fields)), _blend_factor(1.0 / blend_factor), _shape_broadness(shape_broadness) {}

	const volumeDataType eval(const Vector& p) const override;

private:
	std::shared_ptr<const std::vector<vspf>> _fields;
	float _blend_factor;
	float _shape_broadness;
};







} // end namespace lux




