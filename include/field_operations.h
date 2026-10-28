#pragma once

#include <memory>
#include <type_traits>
#include <tuple>
#include <cstddef>
#include <functional>

#include "volume.h"
#include "LinearAlgebra.h"
#include "recursive_mem.h"

namespace lux{

template <typename T, typename Func, typename GradFunc = std::nullptr_t>
class FunctionField : public Volume<T> {
public:
	using typename Volume<T>::volumeDataType;
	using typename Volume<T>::volumeGradType;
	// using Func = std::function<volumeDataType(const Vector&)>;
	// using GradFunc = std::function<volumeGradType(const Vector&)>;

	template <typename E, typename G = GradFunc>
	FunctionField(E&& function, G&& grad_func = {}) :
	 	_function(std::forward<E>(function)), _grad_func(std::forward<G>(grad_func)) {}
	
	const volumeDataType eval(const Vector& p) const override { return _function(p); }
	const volumeGradType grad(const Vector&p) const override {
		if constexpr (std::is_same_v<GradFunc, std::nullptr_t>){
			return Volume<T>::grad(p);
		} else {
			return _grad_func(p);
		}
	}

private:
	Func _function;
	GradFunc _grad_func;
};


// template <typename T>
// class MemoizedField : public Volume<T> {
// public:
//     using Value = typename Volume<T>::volumeDataType;
//     using Grad = typename Volume<T>::volumeGradType;

// private:
//     // make_memoized_r calls the wrapped function as f(memoizer, args...).
//     struct Eval {
//         VSPtr<T> field;

//         template <typename Memo>
//         Value operator()(const Memo&, const Vector& p) const {
//             return field->eval(p);
//         }
//     };

//     using Memo = decltype(
//         make_memoized_r<Value(const Vector&)>(Eval{})
//     );

// public:
//     explicit MemoizedField(VSPtr<T> field)
//         : _field(std::move(field)),
//           _memo(make_memoized_r<Value(const Vector&)>(Eval{_field}))
//     {}

//     Value eval(const Vector& p) const override {
//         return _memo(p);
//     }

//     Grad grad(const Vector& p) const override {
//         return _field->grad(p);
//     }

//     void clear_cache() {
//         _memo.clear_cache();
//     }

// private:
//     VSPtr<T> _field;
//     mutable Memo _memo;
// };


template <typename T>
class MemoField : public Volume<T> {
public:
    using Value = typename Volume<T>::volumeDataType;
    using Grad = typename Volume<T>::volumeGradType;

private:
    struct Eval {
        VSPtr<T> field;

        template <typename Memo>
        Value operator()(const Memo&, const Vector& p) const {
            return field->eval(p);
        }
    };

    using Mem = decltype(
        make_memoized_r<Value(const Vector&)>(Eval{})
    );

public:
    explicit MemoField(VSPtr<T> field)
        : _memoize_field(std::move(field)),
          _mem(make_memoized_r<Value(const Vector&)>(
              Eval{_memoize_field}
          ))
    {}

    const Value eval(const Vector& p) const override {
        return _mem(p);
    }

    const Grad grad(const Vector& p) const override {
        return _memoize_field->grad(p);
    }

    void clear_cache() override {
        _mem.clear_cache();
    }

private:
    VSPtr<T> _memoize_field;
    mutable Mem _mem;
};


// ---------------------------------------------------------------------------------
// Templated field operators
// --------------------------------------------------------------------------------- 

template <typename T, typename ... FieldTypes>
class FieldOperator : public Volume<T>{
public:
	using typename Volume<T>::volumeDataType;
	using typename Volume<T>::volumeGradType;
	using ValueTuple = std::tuple<VolumeSPtr<FieldTypes>...>;

	FieldOperator(VolumeSPtr<T> a, VolumeSPtr<FieldTypes>... values): 
		_a(std::move(a)), _values(std::move(values)...) {}
	~FieldOperator() = default;

    virtual const volumeDataType eval( const Vector& P ) const override = 0;

   	// Keeping base class grad implementation and allowing child classes to override
    //virtual const volumeGradType grad( const Vector& P ) const override {}

protected:
	std::shared_ptr<Volume<T>> _a;
	ValueTuple _values;
};

// Pull grad type from T
template <typename T, typename U>
class AddFields : public FieldOperator<T, U>{
public:
	using typename Volume<T>::volumeDataType;
	using typename Volume<T>::volumeGradType;

	AddFields(const VolumeSPtr<T> a, const VolumeSPtr<U> b) : FieldOperator<T, U>(std::move(a), std::move(b)) {}

	const volumeDataType eval(const Vector& p) const override {
		return this->_a->eval(p) + std::get<0>(this->_values)->eval(p);
	}

	const volumeGradType grad(const Vector& p) const override { 
		return this->_a->grad(p) + std::get<0>(this->_values)->grad(p); 
	}
};

template <typename T, typename U>
class SubtractFields : public FieldOperator<T, U>{
public:
	using typename Volume<T>::volumeDataType;
	using typename Volume<T>::volumeGradType;

	SubtractFields(const VolumeSPtr<T> a, const VolumeSPtr<U> b) : FieldOperator<T, U>(std::move(a), std::move(b)) {}

	const volumeDataType eval(const Vector& p) const override {
		return this->_a->eval(p) - std::get<0>(this->_values)->eval(p);
	}

	const volumeGradType grad(const Vector& p) const override { 
		return this->_a->grad(p) - std::get<0>(this->_values)->grad(p); 
	}
};

// NOTE -> the first type is used to determine the gradient type. 
// ie <vector, float> will return a gradient of a matrix
template <typename T, typename U>
class MultiplyFields : public FieldOperator<T, U>{
	static_assert(
		!(std::is_same_v<T, float> && std::is_same_v<U, Vector>),
		"Multiplication only supports <Vector, float>, not <float, Vector>"
	);
public:
	using typename Volume<T>::volumeDataType;
	using typename Volume<T>::volumeGradType;

	MultiplyFields(const VolumeSPtr<T> a, const VolumeSPtr<U> b) : FieldOperator<T, U>(std::move(a), std::move(b)) {}

	const volumeDataType eval(const Vector& p) const override {
		return this->_a->eval(p) * std::get<0>(this->_values)->eval(p);
	}

	const volumeGradType grad(const Vector& p) const override { 
		if constexpr (std::is_same_v<T, Color> && std::is_same_v<U, float>) {
			return {};
		} else {
			return this->_a->grad(p) * std::get<0>(this->_values)->eval(p)
				+ this->_a->eval(p) * std::get<0>(this->_values)->grad(p);
		}
	}	
	// 	    return this->_a->grad(p) * std::get<0>(this->_values)->eval(p) + this->_a->eval(p) * 
	// 			std::get<0>(this->_values)->grad(p);
    // }
};

template <typename T, typename U>
class DivideFields : public FieldOperator<T, U>{
	static_assert(
		!(std::is_same_v<T, float> && std::is_same_v<U, Vector>),
		"Divide only supports <Vector, float>, not <float, Vector>"
	);
public:
	using typename Volume<T>::volumeDataType;
	using typename Volume<T>::volumeGradType;

	DivideFields(const VolumeSPtr<T> a, const VolumeSPtr<U> b) : FieldOperator<T, U>(std::move(a), std::move(b)) {}

	const volumeDataType eval(const Vector& p) const override {
		auto b_eval = std::get<0>(this->_values)->eval(p);
		if (b_eval == U{}){
			return {};
		} else {
			return this->_a->eval(p) / b_eval;
		}
	}

	const volumeGradType grad(const Vector& p) const override {
		auto b_eval = std::get<0>(this->_values)->eval(p);
		if (b_eval == U{}){
			return {};
		} else{
			return this->_a->grad(p) / b_eval + std::get<0>(this->_values)->grad(p) * 
				   this->_a->eval(p) / (b_eval * b_eval);
		}
    }
};

// ---------------------------------------------------------------------------------
// Operators requiring a single field
// ---------------------------------------------------------------------------------

template <typename T>
class NegateField : public FieldOperator<T>{
public:
	using typename Volume<T>::volumeDataType;
	using typename Volume<T>::volumeGradType;

	NegateField(const VolumeSPtr<T> a) : FieldOperator<T>(std::move(a)) {}

	const volumeDataType eval(const Vector& p) const override {
		return -this->_a->eval(p);
	}

	const volumeGradType grad(const Vector& p) const override { 
        return -this->_a->grad(p);
    }
};

class ExpField : public FieldOperator<float>{
public:
	using typename Volume<float>::volumeDataType;
	using typename Volume<float>::volumeGradType;

	ExpField(const VolumeSPtr<float> a) : FieldOperator<float>(std::move(a)) {}

	const volumeDataType eval(const Vector& p) const override {
		return std::exp(this->_a->eval(p));
	}

	const volumeGradType grad(const Vector& p) const override { 
        return std::exp(this->_a->eval(p)) * this->_a->grad(p);
    }
};

class LogField : public FieldOperator<float>{
public:
	using typename Volume<float>::volumeDataType;
	using typename Volume<float>::volumeGradType;

	LogField(const VolumeSPtr<float> a) : FieldOperator<float>(std::move(a)) {}

	const volumeDataType eval(const Vector& p) const override {
		return std::log(this->_a->eval(p));
	}

	const volumeGradType grad(const Vector& p) const override { 
        auto a_eval = this->_a->eval(p);
		if (a_eval == 0.0){
			return {};
		} else{
			return this->_a->grad(p) / a_eval;
		}
    }
};

class SinField : public FieldOperator<float>{
public:
	using typename Volume<float>::volumeDataType;
	using typename Volume<float>::volumeGradType;

	SinField(const VolumeSPtr<float> a) : FieldOperator<float>(std::move(a)) {}

	const volumeDataType eval(const Vector& p) const override {
		return std::sin(this->_a->eval(p));
	}

	const volumeGradType grad(const Vector& p) const override { 
        return std::cos(this->_a->eval(p)) * this->_a->grad(p);
    }
};

class CosField : public FieldOperator<float>{
public:
	using typename Volume<float>::volumeDataType;
	using typename Volume<float>::volumeGradType;

	CosField(const VolumeSPtr<float> a) : FieldOperator<float>(std::move(a)) {}

	const volumeDataType eval(const Vector& p) const override {
		return std::cos(this->_a->eval(p));
	}

	const volumeGradType grad(const Vector& p) const override { 
        return -std::sin(this->_a->eval(p)) * this->_a->grad(p);
    }
};


// ---------------------------------------------------------------------------------
// Some more double fields
// ---------------------------------------------------------------------------------

class PowField : public FieldOperator<float, float>{
public:
	using typename Volume<float>::volumeDataType;
	using typename Volume<float>::volumeGradType;

	PowField(const VolumeSPtr<float> a, const VolumeSPtr<float> to_power) : 
						FieldOperator<float, float>(std::move(a), std::move(to_power)) {}

	const volumeDataType eval(const Vector& p) const override {
		return std::pow(this->_a->eval(p), std::get<0>(this->_values)->eval(p));
	}

	const volumeGradType grad(const Vector& p) const override { 
        return std::get<0>(this->_values)->eval(p) * 
			   std::pow(this->_a->eval(p), std::get<0>(this->_values)->eval(p) - 1.0) * this->_a->grad(p);
    }
};

template<typename T, typename U>
class ScaleField : public FieldOperator<T, U>{
public:
	using typename Volume<T>::volumeDataType;
	using typename Volume<T>::volumeGradType;

	ScaleField(const VolumeSPtr<T> a, const VolumeSPtr<U> scale) :
			 FieldOperator<T, U>(std::move(a), std::move(scale)) {}

	const volumeDataType eval(const Vector& p) const override {
		auto b_eval = std::get<0>(this->_values)->eval(p);
		if (b_eval == U{}){
			return {};
		} else{
			return this->_a->eval(p / b_eval);
		}
	}

	const volumeGradType grad(const Vector& p) const override { 
		auto b_eval = std::get<0>(this->_values)->eval(p);
		if (b_eval == U{}){
			return {};
		} else{
			return this->_a->grad(p / b_eval) / b_eval;
		}
    }
};

template<typename T>
class TranslateField : public FieldOperator<T, Vector>{
public:
	using typename Volume<T>::volumeDataType;
	using typename Volume<T>::volumeGradType;

	TranslateField(const VolumeSPtr<T> a, const VolumeSPtr<Vector> delta_x) : 
						FieldOperator<T, Vector>(std::move(a), std::move(delta_x)) {}

	const volumeDataType eval(const Vector& p) const override {
		return this->_a->eval(p - std::get<0>(this->_values)->eval(p));
	}

	const volumeGradType grad(const Vector& p) const override { 
        return this->_a->grad(p - std::get<0>(this->_values)->eval(p));
    }
};

template<typename T>
class RotateField : public FieldOperator<T, Vector, float>{
public:
	using typename Volume<T>::volumeDataType;
	using typename Volume<T>::volumeGradType;

	RotateField(const VolumeSPtr<T> a, const VolumeSPtr<Vector> axis, const VolumeSPtr<float> angle) : 
						FieldOperator<T, Vector, float>(std::move(a), std::move(axis), std::move(angle)) {}

	const volumeDataType eval(const Vector& p) const override {
		return this->_a->eval(rotation(p, std::get<0>(this->_values)->eval(p), std::get<1>(this->_values)->eval(p)));
	}

	const volumeGradType grad(const Vector& p) const override { 
        if constexpr (std::is_same_v<T, float>) {
            auto grad = this->_a->grad(rotation(p, std::get<0>(this->_values)->eval(p), std::get<1>(this->_values)->eval(p)));
            return rotation(grad, std::get<0>(this->_values)->eval(p), std::get<1>(this->_values)->eval(p));
        } else {
            return Volume<T>::grad(p);
        }
    }
};


// ---------------------------------------------------------------------------------
// Operator overloading shared pointer stuff double fields
// ---------------------------------------------------------------------------------

template <typename T, typename U>
std::shared_ptr<Volume<T>> operator+(
    const VolumeSPtr<T> a,
    const VolumeSPtr<U> b
){
    return std::make_shared<AddFields<T, U>>(std::move(a), std::move(b));
}

template <typename T, typename U>
std::shared_ptr<Volume<T>> operator-(const VolumeSPtr<T> a,
                                     const VolumeSPtr<U> b)
{
    return std::make_shared<SubtractFields<T, U>>(std::move(a), std::move(b));
}


template <typename T, typename U>
std::shared_ptr<Volume<T>> operator*(const VolumeSPtr<T> a,
                                     const VolumeSPtr<U> b)
{
    return std::make_shared<MultiplyFields<T, U>>(std::move(a), std::move(b));
}

inline std::shared_ptr<Volume<Vector>> operator*(const VolumeSPtr<float> a,
                                     const VolumeSPtr<Vector> b)
{
    return std::make_shared<MultiplyFields<Vector, float>>(std::move(b), std::move(a));
}

inline std::shared_ptr<Volume<Vector>> operator*(const VolumeSPtr<Vector> a,
                                     const VolumeSPtr<float> b)
{
    return std::make_shared<MultiplyFields<Vector, float>>(std::move(a), std::move(b));
}

template <typename T, typename U>
std::shared_ptr<Volume<T>> operator/(const VolumeSPtr<T> a,
                                     const VolumeSPtr<U> b)
{
    return std::make_shared<DivideFields<T, U>>(std::move(a), std::move(b));
}

inline std::shared_ptr<Volume<Vector>> operator/(const VolumeSPtr<float> a,
                                     const VolumeSPtr<Vector> b)
{
    return std::make_shared<DivideFields<Vector, float>>(std::move(b), std::move(a));
}

inline std::shared_ptr<Volume<Vector>> operator/(const VolumeSPtr<Vector> a,
                                     const VolumeSPtr<float> b)
{
    return std::make_shared<DivideFields<Vector, float>>(std::move(a), std::move(b));
}


// ---------------------------------------------------------------------------------
// Operator overloading shared pointer stuff single fields
// ---------------------------------------------------------------------------------

template <typename T>
std::shared_ptr<Volume<T>> operator-(const VolumeSPtr<T> a)
{
    return std::make_shared<NegateField<T>>(std::move(a));
}


} // end namespace lux



