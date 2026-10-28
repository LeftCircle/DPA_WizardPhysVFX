#ifndef _PARTICLE_EMITTER_H
#define _PARTICLE_EMITTER_H

#include <random>

#include "AABB.h"
#include "Vector.h"

namespace pba{

class ParticleEmitter {
public:
	ParticleEmitter(AABB<Vector> bounds) : _bounds(std::move(bounds)) {}
	void emit(Vector& pos, Vector& vel) const noexcept;

	const double get_min_speed() const noexcept { return _min_speed; }
	const double get_max_speed() const noexcept { return _max_speed; }
	void set_min_speed(const double min_speed) noexcept { _min_speed = min_speed; }
	void set_max_speed(const double max_speed) noexcept { _max_speed = max_speed; }

	static Vector generate_random_bounded_vector(double min_mag, double max_mag);
	static std::mt19937& get_rng() {
		static std::mt19937 rng{std::random_device{}()};
		return rng;
	}
	static double rand_d(double min, double max) {
		std::uniform_real_distribution<double> dist(min, max);
		return dist(get_rng());
	}

private:
	void _generate_random_bounded_position(Vector& pos) const noexcept;
	void _generate_random_bounded_velocity(Vector& vel) const noexcept;
	AABB<Vector> _bounds;
	double _min_speed = 0.0;
	double _max_speed = 1.0;
	mutable std::uniform_real_distribution<double> dist{0.0, 1.0};
};

} // end namespace pba


#endif