#ifndef _PARTIAL_SOLVERS_H
#define _PARTIAL_SOLVERS_H

#include "GISolver.h"
#include "dynamical_state_data.h"
#include "collision_handler.h"
#include "force.h"
#include "occupancy_volume.h"
#include "the_wheel.h"

#include <numeric>
#include <execution>

namespace pba{

class PartialSolverAdvancePosition : public GISolverBase
{
public:
	PartialSolverAdvancePosition(DynamicalStateData_sp dsd) : _state_data(dsd) {}
	PartialSolverAdvancePosition(DSD_sp dsd, CollisionHandler_sp ch)
		: _state_data(dsd), _collision_handler(ch) {}
	PartialSolverAdvancePosition(DSD_sp dsd, idx_volume_sp ov)
		: _state_data(dsd), _occupancy_volume(ov) {}
	PartialSolverAdvancePosition(DSD_sp dsd, CollisionHandler_sp ch, idx_volume_sp ov)
		: _state_data(dsd), _collision_handler(ch), _occupancy_volume(ov) {}
	~PartialSolverAdvancePosition() = default;

	void init() override {}
	virtual void solve(const double dt) override;
	// Updates positions by dt. Does not override updated_positions
	static void partial_update(DSD_sp dsd, const double dt);

protected:
	DynamicalStateData_sp _state_data;
	idx_volume_sp _occupancy_volume;
	CollisionHandler_sp _collision_handler;
};

class AdvancePositionWithCollisions : public GISolverBase{
public:
	AdvancePositionWithCollisions(DynamicalStateData_sp dsd, CollisionHandler_sp collision_handler);
	~AdvancePositionWithCollisions() = default;

	void init() override {}
	void solve(const double dt) override;

protected:
	DynamicalStateData_sp _state_data;
	CollisionHandler_sp _collision_handler;
};

inline GISolver_sp create_advance_position_with_collisions(DynamicalStateData_sp dsd, CollisionHandler_sp collision_handler){
	return std::make_shared<AdvancePositionWithCollisions>(dsd, collision_handler);
}

class AdvanceVelocityWithForces : public GISolverBase{
public:
	AdvanceVelocityWithForces(DynamicalStateData_sp dsd, ForceSystem_sp force_system)
		: _state_data(dsd), _force_system(force_system) {}
	~AdvanceVelocityWithForces() = default;

	void init() override {}
	virtual void solve(const double dt) override;

protected:
	DynamicalStateData_sp _state_data;
	ForceSystem_sp _force_system;
};

class GISolverLeapfrog : public GISolverBase{
public:
	GISolverLeapfrog(GISolver_sp solver_a, GISolver_sp solver_b) : _solver_a(solver_a), _solver_b(solver_b) {}
	~GISolverLeapfrog() = default;

	void init() override {
		_solver_a->init();
		_solver_b->init();
	}

	void solve(const double dt) override {
		const double half_dt = dt * 0.5;
		_solver_a->solve(half_dt);
		_solver_b->solve(dt);
		_solver_a->solve(half_dt);
	}

private:
	GISolver_sp _solver_a;
	GISolver_sp _solver_b;
};

// From https://jtessen.people.clemson.edu/cpsc6190/html/_g_i_solver_8h_source.html
class GISolverSixthOrder : public GISolverBase
{
public:
	GISolverSixthOrder(GISolver_sp s ) : 
		_solver (s) 
	{
		_a = 1.0/( 4.0 - std::pow(4.0, 1.0/3.0) );
			_b = 1.0 - 4.0*_a;
	}

	~GISolverSixthOrder(){}

	void init(){ _solver->init(); }

	void solve( const double dt )
	{
		const double dta = _a * dt;
		const double dtb = _b * dt;
		_solver->solve(dta);
		_solver->solve(dta);
		_solver->solve(dtb);
		_solver->solve(dta);
		_solver->solve(dta);
	}


private:

	GISolver_sp _solver;
	double _a, _b;
};


} // end namespace pba

#endif