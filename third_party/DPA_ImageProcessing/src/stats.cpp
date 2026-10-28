#include "stats.h"

namespace stats {

template<typename T>
std::vector<T> average_ensemble2D(const T* data_ptr, int width, int height, int channels, int x, int y, int ensemble_size) {
    std::vector<T> avg_vals(channels, 0);
    float total = static_cast<float>(2 * ensemble_size + 1) * static_cast<float>(2 * ensemble_size + 1);
    for (int j = -ensemble_size; j <= ensemble_size; ++j) {
        for (int i = -ensemble_size; i <= ensemble_size; ++i) {
            int xi = x + i;
            int yj = y + j;
            if (xi < 0 || xi >= width || yj < 0 || yj >= height) {
                continue;
            }
            long index = (yj * width + xi) * channels;
            for (int c = 0; c < channels; ++c) {
                avg_vals[c] += data_ptr[index + c];
            }
        }
    }

    if (total > 0.0f) {
        for (int c = 0; c < channels; ++c) {
            avg_vals[c] /= total;
        }
    }
    return avg_vals;
}

template<typename T>
std::vector<T> gaussian_ensemble2D(
    const T* data_ptr,
    int width,
    int height,
    int channels,
    int x,
    int y,
    int ensemble_size
) {
    std::vector<T> avg_vals(channels, 0);
    float total_weight = 0.0f;

    float sigma = static_cast<float>(ensemble_size) / 3.0f;
    // Ensure sigma isn't too small to avoid aliasing
    sigma = std::max(0.5f, sigma);
    float two_sigma_sq = 2.0f * sigma * sigma;
    //float two_sigma_sq = (float)(ensemble_size * ensemble_size) * 2.0f;

    for (int j = -ensemble_size; j <= ensemble_size; ++j) {
        for (int i = -ensemble_size; i <= ensemble_size; ++i) {
            int xi = x + i;
            int yj = y + j;
            if (xi < 0 || xi >= width || yj < 0 || yj >= height) {
                continue;
            }
            //float weight = std::exp(-(i * i + j * j) / (two_sigma_sq));
            float x = ensemble_size - std::abs(i);
            float y = ensemble_size - std::abs(j);
            float weight = std::exp(-(x * x + y * y) / (two_sigma_sq));
            long index = (yj * width + xi) * channels;
            for (int c = 0; c < channels; ++c) {
                avg_vals[c] += data_ptr[index + c] * weight;
            }
            total_weight += weight;
        }
    }

    if (total_weight > 0.0f) {
        for (int c = 0; c < channels; ++c) {
            avg_vals[c] /= total_weight;
        }
    }
    return avg_vals;
}
// Template function definition
template std::vector<float> average_ensemble2D<float>(const float* data_ptr, int width, int height, int channels, int x, int y, int ensemble_size);
template std::vector<float> gaussian_ensemble2D<float>(const float* data_ptr, int width, int height, int channels, int x, int y, int ensemble_size);
}