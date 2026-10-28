#include "GISolver.h"

using namespace pba;

void GISolverSystem::init(){
	for( auto& solver : _solvers ){
		solver->init();
	}
}

void GISolverSystem::solve(const double dt){
	for( size_t i=0; i<_solvers.size(); i++ ){
		_solvers[i]->solve( _time_steps[i] );
	}
}

void GISolverSystem::add_solver(GISolver_sp solver, double time_step){
	_solvers.push_back( solver );
	_time_steps.push_back( time_step );
}



std::shared_ptr<GISolverSystem> create_gi_solver_system(const std::vector<GISolver_sp>& solvers, const std::vector<double>& time_steps){
	if (solvers.size() != time_steps.size()){
		throw std::runtime_error("GISolverSystem creation error: solvers and time_steps vectors must be the same size");
	}
	auto system = std::make_shared<GISolverSystem>();
	for( size_t i=0; i<solvers.size(); i++ ){
		system->add_solver( solvers[i], time_steps[i] );
	}
	return system;
}





