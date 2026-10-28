#ifndef _TORQUE_H
#define _TORQUE_H

#include <numeric>
#include <execution>
#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>

#include "rigid_body.h"
#include "force.h"

namespace pba{

class Torque {
public:
	Torque(ForceSystem_sp f) : _force(f) {};
	~Torque() = default;

	void compute(RB_sp& rbd, const double dt);

private:
	ForceSystem_sp _force;

};




} // end namespace pba


#endif