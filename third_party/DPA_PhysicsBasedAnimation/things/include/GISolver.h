#ifndef _GI_SOLVER_H
#define _GI_SOLVER_H

#include <memory>
#include <vector>
#include <stdexcept>

namespace pba
{

class GISolverBase
{
public:
	GISolverBase() = default;
	virtual ~GISolverBase() = default;

	virtual void init() = 0;
	virtual void solve(const double dt) = 0;
};

using GISolver_sp = std::shared_ptr<pba::GISolverBase>;


class GISolverSystem : public GISolverBase
{
public:
	GISolverSystem() = default;
	~GISolverSystem() = default;

	void init() override;
	void solve(const double dt) override;

	void add_solver(GISolver_sp solver, double time_step);	

private:
	std::vector<GISolver_sp> _solvers;
	std::vector<double> _time_steps;
};

std::shared_ptr<GISolverSystem> create_gi_solver_system(const std::vector<GISolver_sp>& solvers, const std::vector<double>& time_steps);
inline std::shared_ptr<GISolverSystem> create_gi_solver_system() { return std::make_shared<GISolverSystem>(); }

} // namespace pba



#endif