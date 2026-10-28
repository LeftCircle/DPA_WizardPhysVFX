#ifndef _SOFT_BODY_DATA_H
#define _SOFT_BODY_DATA_H


#include <vector>
#include <memory>
#include <algorithm>
#include <stdexcept>

#include "the_wheel.h"
#include "dynamical_state_data.h"
#include "soft_edge.h"
#include "math.h"


namespace pba{


class SoftBody : public DynamicalStateData {
public:
	void connect_all_particles_together() noexcept;
	void connect_all_particles_in_range(size_t start_idx, size_t end_idx);
	void clear_connections() noexcept {edges.clear(); }

	size_t get_n_soft_edges() const noexcept { return edges.size(); }

	std::vector<SoftEdge> edges;
	std::vector<SoftTriangle> soft_triangles;
};



} // end namespace pba



#endif