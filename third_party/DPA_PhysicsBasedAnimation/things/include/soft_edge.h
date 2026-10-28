#ifndef _SOFT_EDGE_H
#define _SOFT_EDGE_H

#include "the_wheel.h"
#include "Vector.h"

namespace pba{

class SoftEdge{
public:
	using idxs = std::pair<size_t, size_t>;	

	SoftEdge(size_t i, size_t j, double rest_len) : _index_a(i), _index_b(j), _rest_length(rest_len) {}

	// Might be cleaner to pass in the DSD since we have indices already, but that's a lot of shared pointer passing
	void compute(
		span<const Vector> positions,
		span<const Vector> vels,
		const double spring_force,
		const double friction
	);
	Vector get_acceleration_on_a(
		span<const Vector> positions,
		span<const Vector> vels,
		const double spring_force,
		const double friction
	) const;
	const Vector& get_force_on_a() const noexcept { return _force_on_a; }
	idxs get_indices() const noexcept {return idxs(_index_a, _index_b); }

	double get_k() const { return _k; }
	double get_friction() const { return _friction; }

private:
	SoftEdge() = delete;

	size_t _index_a, _index_b;
	double _rest_length;
	double _k;
	double _friction;
	Vector _force_on_a = Vector(0.0, 0.0, 0.0); 
};

class SoftTriangle{
public:
	SoftTriangle(
		size_t idx_a,
		size_t idx_b,
		size_t idx_c,
		double area,
		double force = 1.0,
		double friction = 0.0
	) :	_area(area), _k(force), _friction(friction) {_idxs[0] = idx_a; _idxs[1] = idx_b; _idxs[2] = idx_c; }

	const size_t idx0() const noexcept { return _idxs[0]; }
	const size_t idx1() const noexcept { return _idxs[1]; }
	const size_t idx2() const noexcept { return _idxs[2]; }
	const size_t& operator[] (const int idx) const { return _idxs[idx]; }

	const double rest_area() const noexcept { return _area; } 
	const double k() const noexcept { return _k; }
	const double u() const noexcept { return _friction; }
private:
	size_t _idxs[3];
	double _area;
	double _k;
	double _friction;
};




}// end namepspace pba




#endif