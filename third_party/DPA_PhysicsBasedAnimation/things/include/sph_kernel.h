#ifndef SPH_KERNEL_H
#define SPH_KERNEL_H

#include "Vector.h"

const double PId = 3.14159265358979;

namespace pba {

class Kernel {
public:
	virtual ~Kernel() = default;
	virtual double operator()(double distance) const noexcept = 0;
	virtual Vector gradient(double distance, const Vector& direction) const noexcept = 0;
	virtual double second_derivative(double distance) const noexcept { return 0; }
};

using Kernel_sp = std::shared_ptr<Kernel>;

class CubicSplineKernel3 : public Kernel {
public:
	explicit CubicSplineKernel3(double radius) {
		_one_over_h = 1.0 / radius;
		_one_over_h2 = _one_over_h * _one_over_h;
		_one_over_pi_h3 = 1.0 / PId * _one_over_h2 * _one_over_h;
		_sigma_over_four = _one_over_pi_h3 / 4.0;
	};

	double operator()(double distance) const noexcept override {
		double q = distance * _one_over_h;
		if (q >= 2.0){
			return 0.0;
		} else if (q >= 1.0){
			return _sigma_over_four * pow(2.0 - q, 3);
		} else {
			return _one_over_pi_h3 * (1.0 - 1.5 * q * q * (1.0 - 0.5 * q));
		}
	};

	Vector gradient(double distance, const Vector& direction) const noexcept override {
		double q = distance * _one_over_h;
		double mag = direction.magnitude();
		Vector scaled_dir;
		if (mag == 0.0){
			scaled_dir = direction;
		} else{
			scaled_dir = _one_over_h * direction / mag;
		}
		if (q >= 2.0){
			return Vector(0.0, 0.0, 0.0);
		} else if (q >= 1.0){
			return -_sigma_over_four * 3.0 * pow(2.0 - q, 2) * scaled_dir;
		} else {
			return -_one_over_pi_h3 * (3.0 * q * (1.0 - 0.75 * q)) * scaled_dir;
		}
	};

private:
	double _one_over_pi_h3;
	double _one_over_h2;
	double _one_over_h;
	double _sigma_over_four;
};



// This is the basic kernel implementation from Doyub Kim's fluid engine development book
template<typename Vec>
struct SphStdKernel3 {
	// 315 / (64 * pi * h^3) (1 - (r^2 / h^2)^3) when 0 <= r < = h
	double h1, h2, h3, h5;

	SphStdKernel3() : h1(0), h2(0), h3(0), h5(0) {};
	explicit SphStdKernel3(double radius) : h1(radius), h2(radius * radius), h3(h2 * radius), h5(h2 * h3) {};

	double operator()(double distance) const noexcept;

	double first_derivative(double distance) const noexcept;
	double second_derivative(double distance) const noexcept;
	Vec gradient(double distance, const Vec& direction) const noexcept;
};

template<typename Vec>
struct SphStdKernel2 {
	double h, h2, h3, h4;

	SphStdKernel2() : h(0), h2(0), h3(0), h4(0) {};
	explicit SphStdKernel2(double radius) : h(radius), h2(radius * radius), h3(h2 * radius), h4(h2 * h2) {};

	double operator()(double distance) const noexcept;

	double first_derivative(double distance) const noexcept;
	double second_derivative(double distance) const noexcept;
	Vec gradient(double distance, const Vec& direction) const noexcept;
};

class SphSpikyKernel3 : public Kernel {
public:
	double h1, h2, h3, h4, h5;

	SphSpikyKernel3() : h1(0), h2(0), h3(0), h4(0), h5(0) {};
	explicit SphSpikyKernel3(double radius) : h1(radius), h2 (h1 * h1), h3(h1 * h2), h4(h2 * h2), h5(h3 * h2) {};
	double operator()(double distance) const noexcept override;
	double first_derivative(double distance) const noexcept;
	double second_derivative(double distance) const noexcept override;
	Vector gradient(double distance, const Vector& direction) const noexcept override;
};

template<typename Vec>
struct SphSpikyKernel2 {
	double h1, h2, h3, h4;

	SphSpikyKernel2() : h1(0), h2(0), h3(0), h4(0) {};
	explicit SphSpikyKernel2(double radius) : h1(radius), h2(radius * radius), h3(radius * h2), h4(h2 * h2) {};

	double operator()(double distance) const noexcept;
	double first_derivative(double distance) const noexcept;
	double second_derivative(double distance) const noexcept;
	Vec gradient(double distance, const Vec& direction) const noexcept;
};


template<typename Vec>
inline double SphStdKernel3<Vec>::operator()(double distance) const noexcept{
	if (distance * distance >= h2){
		return 0.0;
	} else {
		double x = (1.0 - (distance * distance) / h2);
		return 315.0 / (64.0 * PId * h3) * x * x * x;
	}
}

template<typename Vec>
inline double SphStdKernel3<Vec>::first_derivative(double distance) const noexcept {
	if (distance * distance >= h2){
		return 0.0;
	} else {
		double x = (1 - distance * distance / h2);
		return -945.0 / (32.0 * PId * h5) * distance * x * x;
	}
}

template<typename Vec>
inline double SphStdKernel3<Vec>::second_derivative(double distance) const noexcept {
	if (distance * distance >= h2){
		return 0.0;
	} else {
		double x = distance * distance / h2;
		return 945.0 / (32.0 * PId * h5) * (3 * x - 1) * (1 - x);
	}
}

template <typename Vec>
inline Vec SphStdKernel3<Vec>::gradient(double distance, const Vec& direction) const noexcept {
	// Direction is assumed to be normalized
	return -first_derivative(distance) * direction;
}

template<typename Vec>
inline double SphStdKernel2<Vec>::operator()(double distance) const noexcept {
	if (distance >= h){
		return 0.0;
	} else {
		double x = (1 - distance * distance / h2);
		return (4.0 / (PId * h2)) * x * x * x;

	}
}

template<typename Vec>
inline double SphStdKernel2<Vec>::first_derivative(double distance) const noexcept {
	if (distance >= h){
		return 0.0;
	} else {
		double x = (1 - distance * distance / h2);
		return -24.0 / (PId * h2 * h2) * distance * x * x;
	}
}

template<typename Vec>
inline double SphStdKernel2<Vec>::second_derivative(double distance) const noexcept {
	if (distance >= h){
		return 0.0;
	} else {
		double x = distance * distance / h2;
		return 24.0 / (PId * h4) * (1 - x) * (5 * x - 1);
	}
}

template<typename Vec>
inline Vec SphStdKernel2<Vec>::gradient(double distance, const Vec& direction) const noexcept {
	// Direction is assumed to be normalized
	return -first_derivative(distance) * direction;
}

inline double SphSpikyKernel3::operator()(double distance) const noexcept {
	if (distance >= h1){
		return 0.0;
	} else {
		double x = (1.0 - (distance) / h1);
		return 15.0 / (PId * h3) * x * x * x;
	}
}

inline double SphSpikyKernel3::first_derivative(double distance) const noexcept {
	if (distance >= h1){
		return 0.0;
	} else {
		double x = (1.0 - (distance) / h1);
		return -45.0 / (PId * h4) * x * x;
	}
}

inline double SphSpikyKernel3::second_derivative(double distance) const noexcept {
	if (distance >= h1){
		return 0.0;
	} else {
		double x = (1.0 - (distance) / h1);
		return 90.0 / (PId * h5) * x;
	}
}

inline Vector SphSpikyKernel3::gradient(double distance, const Vector& direction) const noexcept {
	// Direction is assumed to be normalized
	return -first_derivative(distance) * direction;
}

template<typename Vec>
inline double SphSpikyKernel2<Vec>::operator()(double distance) const noexcept {
	if (distance * distance >= h2){
		return 0.0;
	} else {
		double x = 1.0 - distance / h1;
		return 10.0 / (PId * h2) * x * x * x;
	}
}

template<typename Vec>
inline double SphSpikyKernel2<Vec>::first_derivative(double distance) const noexcept {
	if (distance >= h1){
		return 0.0;
	} else {
		double x = (1.0 - (distance) / h1);
		return -30.0 / (PId * h3) * x * x;
	}
}

template<typename Vec>
inline double SphSpikyKernel2<Vec>::second_derivative(double distance) const noexcept {
	if (distance >= h1){
		return 0.0;
	} else {
		double x = (1.0 - (distance) / h1);
		return 60.0 / (PId * h4) * x;
	}
}

template<typename Vec>
inline Vec SphSpikyKernel2<Vec>::gradient(double distance, const Vec& direction) const noexcept {
	// Direction is assumed to be normalized
	return -first_derivative(distance) * direction; 
}

} // end namespace pba

#endif