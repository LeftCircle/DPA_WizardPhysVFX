
#ifndef _OCCUPANCY_VOLUME_H
#define _OCCUPANCY_VOLUME_H

#include <vector>
#include <type_traits>
#include <cassert>

#include "the_wheel.h"
#include "array3D.h"
#include "Vector.h"

#include "AABB.h"

namespace pba {

struct indices {
	size_t i;
	size_t j;
	size_t k;
};

template <typename CellType>
class OccupancyVolume {
public:
	OccupancyVolume(const AABB<Vector>& aabb, double cell_size) :
		_aabb(aabb),
		_cell_size(cell_size),
		_voxels(
			(int)(std::ceil((aabb.upper_right() - aabb.lower_left()).X() / cell_size)),
			(int)(std::ceil((aabb.upper_right() - aabb.lower_left()).Y() / cell_size)),
			(int)(std::ceil((aabb.upper_right() - aabb.lower_left()).Z() / cell_size))
	) {}

	Vector get_dimensions() const noexcept {
		return Vector(_voxels.get_x_dim(), _voxels.get_y_dim(), _voxels.get_z_dim());
	}

	const CellType& get_cell(int x_idx, int y_idx, int z_idx) const {
		return _voxels(x_idx, y_idx, z_idx);
	}

	template <typename T, typename func>
	//	requires std::invocable<func, size_t, T, const CellType&>
	T accumulate_neighbor_cells(size_t idx, const Vector& position, T init, func fn) const {
		auto neighbor_cells = _get_neighbor_cell_indices(position);
		for (const auto& neighbor_cell_id : neighbor_cells) {
			init = fn(idx, std::forward<T>(init), _voxels(neighbor_cell_id.i, neighbor_cell_id.j, neighbor_cell_id.k));
		}
		return init;
	}

	template <typename Inserter> 
	//	requires std::invocable<Inserter, CellType&, size_t>
	void populate(const std::vector<Vector>& positions, Inserter inserter_fn) {
		_voxels.clear();
		for (size_t i = 0; i < positions.size(); i++) {
			CellType& cell = _get_cell(positions[i]);
			inserter_fn(cell, i);
		}
	}

	template <typename Inserter>
	// requires std::invocable<Inserter, CellType&, size_t>
	void populate(span<const Vector> positions, Inserter inserter_fn) {
		_voxels.clear();
		for (size_t i = 0; i < positions.size(); i++) {
			CellType& cell = _get_cell(positions[i]);
			inserter_fn(cell, i);
		}
	}

	void clear() { _voxels.clear(); }
	const AABB<Vector> get_aabb() const { return _aabb; };
	const double get_cell_size() const { return _cell_size; }

private:
	indices _get_cell_indices(const Vector& position) const {
		int x_idx = (int)((position.X() - _aabb.lower_left().X()) / _cell_size);
		int y_idx = (int)((position.Y() - _aabb.lower_left().Y()) / _cell_size);
		int z_idx = (int)((position.Z() - _aabb.lower_left().Z()) / _cell_size);
		if (!(x_idx >= 0 && y_idx >= 0 && z_idx >= 0 && x_idx < _voxels.get_x_dim() && y_idx < _voxels.get_y_dim() && z_idx < _voxels.get_z_dim())) [[unlikely]]{
			//printf("Position is out of bounds of the occupancy volume. This should never happen \n");
		}
		x_idx = std::clamp(x_idx, 0, _voxels.get_x_dim() - 1);
		y_idx = std::clamp(y_idx, 0, _voxels.get_y_dim() - 1);
		z_idx = std::clamp(z_idx, 0, _voxels.get_z_dim() - 1);
		return indices{(size_t)x_idx, (size_t)y_idx, (size_t)z_idx};
	}

	std::vector<indices> _get_neighbor_cell_indices(const Vector& position) const {
		indices idx = _get_cell_indices(position);
		std::vector<indices> neighbor_indices;
		neighbor_indices.reserve(26);
		for (int k = -1; k <= 1; k++) {
			for (int j = -1; j <= 1; j++) {
				for (int i = -1; i <= 1; i++) {
					int neighbor_x_idx = idx.i + i;
					int neighbor_y_idx = idx.j + j;
					int neighbor_z_idx = idx.k + k;
					if (neighbor_x_idx >= 0 && neighbor_x_idx < _voxels.get_x_dim() &&
						neighbor_y_idx >= 0 && neighbor_y_idx < _voxels.get_y_dim() &&
						neighbor_z_idx >= 0 && neighbor_z_idx < _voxels.get_z_dim()) {
						neighbor_indices.push_back(indices{(size_t)neighbor_x_idx, (size_t)neighbor_y_idx, (size_t)neighbor_z_idx});
					}
				}
			}
		}
		return neighbor_indices;
	}

	CellType& _get_cell(const Vector& position) {
		indices idx = _get_cell_indices(position);
		return _voxels(idx.i, idx.j, idx.k);
	}

	AABB<Vector> _aabb;
	double _cell_size;
	Array3D<CellType> _voxels;

};

using idx_volume_sp = std::shared_ptr<OccupancyVolume<std::vector<size_t>>>;

inline idx_volume_sp create_idx_occupancy_volume(const AABB<Vector>& aabb, double cell_size) {
	return std::make_shared<OccupancyVolume<std::vector<size_t>>>(aabb, cell_size);
}

inline idx_volume_sp duplicate_idx_volume(idx_volume_sp ov) { return create_idx_occupancy_volume(ov->get_aabb(), ov->get_cell_size()); }

using idx_vec = std::vector<size_t>;
using idx_func = std::function<void(std::vector<size_t>, size_t)>;


} // end namespace pba


#endif