#ifndef _ARRAY3D_H_
#define _ARRAY3D_H_

#include <algorithm>

namespace pba {

// This is just a basic wrapper for a std::vector 
// one dimensional representation of a 3D array
template <typename T>
class Array3D {
public:
	Array3D(int x_dim, int y_dim, int z_dim) : _x_dim(x_dim), _y_dim(y_dim), _z_dim(z_dim) {
		_data.resize(x_dim * y_dim * z_dim);
	}

	int index(int x_idx, int y_idx, int z_idx) const noexcept {
		return x_idx + _x_dim * (y_idx + _y_dim * z_idx);
	}

	T& operator()(int x_idx, int y_idx, int z_idx) {
		return _data[index(x_idx, y_idx, z_idx)];
	}

	const T& operator()(int x_idx, int y_idx, int z_idx) const {
		return _data[index(x_idx, y_idx, z_idx)];
	}

	int get_x_dim() const noexcept { return _x_dim; }
	int get_y_dim() const noexcept { return _y_dim; }
	int get_z_dim() const noexcept { return _z_dim; }

	void clear() {
		std::fill(_data.begin(), _data.end(), T());
	}

private:
	int _x_dim;
	int _y_dim;
	int _z_dim;
	std::vector<T> _data;
};

} // end namespace pba


#endif