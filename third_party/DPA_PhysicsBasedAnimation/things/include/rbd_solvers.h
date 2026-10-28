#ifndef _RBD_SOLVERS_H
#define _RBD_SOLVERS_H

#include "GISolver.h"
#include "rigid_body.h"
#include "torque.h"
#include "the_wheel.h"
#include "collision_handler.h"

namespace pba{

inline constexpr double BISEC_TOLERANCE = 0.000001;
inline constexpr int MAX_BISEC_ITERS = 20;
class RBDCollisionHandler;

Vector rbd_single_particle_pos_rot_update(const RB_sp& rb, const size_t idx, const double dt);

class AdvanceRotationAndCOM : public GISolverBase {
public:
	AdvanceRotationAndCOM(RB_sp rbd) : _rbd(rbd) {}

	void init() override {};
	virtual void solve(const double dt) override;
	static void solve(RB_sp rbd, const double dt);

protected:
	AdvanceRotationAndCOM() = delete;
	RB_sp _rbd;
};


class AdvanceRotationAndCOMWithCollisions : public AdvanceRotationAndCOM {
public:
	explicit AdvanceRotationAndCOMWithCollisions(RB_sp rbd, std::shared_ptr<RBDCollisionHandler> rbd_ch);
	void solve(const double dt) override;

private:
	std::shared_ptr<RBDCollisionHandler> rbd_coll_handler;
};

class AdvanceAngularVelocityAndVelocity : public GISolverBase {
public:
	AdvanceAngularVelocityAndVelocity( RB_sp rbd, ForceSystem_sp f) : _rbd(rbd), _torque(Torque(f)) {}; 
	~AdvanceAngularVelocityAndVelocity() = default;

	void init() override {};
	void solve(const double dt) override;

private:
	RB_sp _rbd;
	Torque _torque;
};


struct RBDHitResult {
	double time;
	size_t particle;
	Vector position;
	Vector normal;
	CollisionSurface_sp surface;
	bool converged = false;
	Vector point_on_cobj;
};

std::optional<RBDHitResult> bisect_collision(
	const RB_sp& rbd, size_t particle_idx,
	const CollisionObject_sp& cobj, CollisionSurface_sp cs,
	double max_t
);


class RBDCollisionHandler : public CollisionHandler {
public:
	RBDCollisionHandler() = default;
	~RBDCollisionHandler() = default;

	virtual void handle_collisions(
		DSD_sp dsd,
		const std::string& updated_pos_attr_name,
		const double dt
	) override;

protected:
	bool _find_earliest_rbd_static_collision(RB_sp rbd, RBDHitResult& min_hit, double dt) const;
	virtual double _get_A(RB_sp rbd, size_t particle_idx, Vector& n) const;
	void _handle_rbd_collisions(RB_sp rbd, RBDHitResult& min_hit, const double dt);
	void _resolve_collision(RB_sp rbd, RBDHitResult& min_hit, double dt);
};

} // end namespace pba


#endif