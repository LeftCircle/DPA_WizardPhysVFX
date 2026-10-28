#ifndef OPTICAL_FLOW_H
#define OPTICAL_FLOW_H

#include <tuple>
#include <vector>
#include <string>

#include "image_data_modifier.h"
#include "image_data.h"

// A bit of a hack at the moment
static std::vector<ImageData> empty_image_sequence;

class OpticalFlow {

public:
    OpticalFlow(std::vector<ImageData>& image_sequence, const ImageData& target_image);
    OpticalFlow(int w, int h, int channels);
	~OpticalFlow() = default;

	ImageData flow(
		const std::vector<size_t>& indices_used,
		std::string output_dir = "", 
		int iterations_per_image = 1,
		float flow_delta = 1.0f
	);

	void set_new_target_image(const ImageData& new_target_image);
	void set_new_image_sequence(std::vector<ImageData>& new_image_sequence);

private:
	OpticalFlow() = delete;	

	using corr_comps = std::tuple<ImageData&, ImageData&, ImageData&>;

	int _ensemble_avg_half_width = 1;
	bool _negative = false;

	std::vector<ImageData>& _image_sequence;

	ImageData _image_to_flow;
	ImageData _image_to_flow_buffer;
	ImageData _iteration_imga;
	ImageData _iteration_imgb;
	ImageData _velocity_field;
	ImageData _iteration_velocity_field;
	ImageData _dIx;
	ImageData _dIy;
	ImageData _dIx_sq;
	ImageData _dIy_sq;
	ImageData _dIx_dIy;
	ImageData _Qx;
	ImageData _Qy;
	ImageData _c00;
	ImageData _c11;
	ImageData _c_off_diag;

	void _init_data_images(const ImageData& img_to_flow) noexcept;

	void _set_gradient_squared_images() noexcept;

	void _build_ensemble_average_in_sequence(
		const ImageData& next_image,
		const ImageData& current_image,
		const ImageData& image_gradient,
		int ensemble_avg_half_width,
		ImageData& output_image
	);
	void _compute_correlation_matrix_components(
		const ImageData& dIx_sq,
		const ImageData& dIy_sq,
		const ImageData& dIx_dIy,
		int ensemble_avg_half_width,
		corr_comps& output_components
	);
	void _compute_velocity_field(
		const ImageData& Qx,
		const ImageData& Qy,
		const ImageData& c00,
		const ImageData& c11,
		const ImageData& c_off_diag,
		ImageData& velocity_field
	);

	void _apply_velocity_field(
		const ImageData& velocity_field,
		const ImageData& input_image,
		ImageData& output_image,
		float flow_delta
	);

};


#endif // OPTICAL_FLOW_H