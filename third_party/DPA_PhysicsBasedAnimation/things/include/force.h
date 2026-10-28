#ifndef _FORCE_H
#define _FORCE_H

#include <memory>
#include <algorithm>
#include <execution>

#include "dynamical_state_data.h"

namespace pba{

class ForceBase{

public:
	ForceBase() = default;
	virtual ~ForceBase() = default;
	virtual void compute(DSD_sp dsd, const double dt) const = 0;
};
using Force_sp = std::shared_ptr<ForceBase>;

class ForceSystem : public ForceBase{
public:

	ForceSystem() = default;
	~ForceSystem() = default;
	void add_force(Force_sp force) { _forces.push_back(force); }
	
	template<typename... Forces>
	void add_forces(Forces... forces) {
		(_forces.push_back(forces), ...);
	}
	void compute(DSD_sp dsd, const double dt) const override;
	static void reset_accelerations(DSD_sp dsd) noexcept;

private:
	std::vector<Force_sp> _forces;
};

using ForceFunction = std::function<void(DSD_sp, double)>;

class ForceFunctionSystem : public ForceBase{
public:
	void add(ForceFunction fn) {_forces.push_back(std::move(fn)); }

	void compute(DSD_sp dsd, const double dt) const override;

private:
	std::vector<ForceFunction> _forces;
};

using ForceSystem_sp = std::shared_ptr<ForceSystem>;

} // end namespace pba


#endif