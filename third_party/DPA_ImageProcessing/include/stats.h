#ifndef STATS_H
#define STATS_H

#include <vector>
#include <cmath>

namespace stats {
	template<typename T>
	std::vector<T> average_ensemble2D(
		const T* data_ptr,
		int width,
		int height,
		int channels,
		int x,
		int y,
		int ensemble_size
	);

	template<typename T>
	std::vector<T> gaussian_ensemble2D(
		const T* data_ptr,
		int width,
		int height,
		int channels,
		int x,
		int y,
		int ensemble_size
	);


	constexpr auto average_ensemble2Df = average_ensemble2D<float>;
	constexpr auto gaussian_ensemble2Df = gaussian_ensemble2D<float>;
}
#endif