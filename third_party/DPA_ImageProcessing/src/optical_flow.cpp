#include "optical_flow.h"


OpticalFlow::OpticalFlow(std::vector<ImageData>& image_sequence, const ImageData& target_image)
: _image_sequence(image_sequence) {
	_init_data_images(target_image);
}

OpticalFlow::OpticalFlow(int w, int h, int channels)
: _image_sequence(empty_image_sequence) {
	ImageData img_to_flow(w, h, channels);
	_init_data_images(img_to_flow);
	_image_sequence = empty_image_sequence;
}

ImageData OpticalFlow::flow(
		const std::vector<size_t>& indices_used,
		std::string output_dir, 
		int iterations_per_image,
		float flow_delta
	){
	// Create a double buffer for updating/reading the flowed image
	ImageData* current_image = &_image_to_flow;
	ImageData* next_image = &_image_to_flow_buffer;

	ImageData* iter_a = &_iteration_imga;
	ImageData* iter_b = &_iteration_imgb;

	const int num_images = _image_sequence.size();
	auto c_components = std::tie(_c00, _c11, _c_off_diag);
	for (size_t i = 0; i < indices_used.size() - 1; i++){
		const ImageData& next_img = _image_sequence[indices_used[i + 1]];
		const ImageData& curr_img = _image_sequence[indices_used[i]];
		iter_a->set_to(curr_img);
		iter_b->set_to(curr_img);

		if (!_image_to_flow.dimensions_match(next_img) || !_image_to_flow.dimensions_match(curr_img)){
			// TODO -> allow for resizing the flow target image
			throw std::invalid_argument("Optical flow target image dimensions do not match image sequence dimensions.");
			return _image_to_flow;
		}
		// reset the total velocity field
		_velocity_field.set_pixel_values(0.0f);

		for (int iter = 0; iter < iterations_per_image; iter++){
			// 1. Get the displacement images
			iter_a->write_x_gradient_into(_dIx);
			iter_a->write_y_gradient_into(_dIy);
			
			// Could certainly be optimized
			_set_gradient_squared_images();
	
			// 2. Generate ensemble averages Qxy = <<(Id - Iu) * dixy>>
			_build_ensemble_average_in_sequence(next_img, *iter_a, _dIx, _ensemble_avg_half_width, _Qx);
			_build_ensemble_average_in_sequence(next_img, *iter_a, _dIy, _ensemble_avg_half_width, _Qy);
	
			// 3. Now for the correlation matrix component images
			_compute_correlation_matrix_components(_dIx_sq, _dIy_sq, _dIx_dIy, _ensemble_avg_half_width, c_components);
	
			_compute_velocity_field(_Qx, _Qy, _c00, _c11, _c_off_diag, _iteration_velocity_field);
			_velocity_field += _iteration_velocity_field;

			// 5. Now that we have the velocity field, apply that to the iterated image.
			// If we are on the last iteration, then just pass since we have the velocity field already
			if (iter == iterations_per_image - 1){
				break;
			}
			//iter_b->set_to(*iter_a);
			_apply_velocity_field(_iteration_velocity_field, *iter_a, *iter_b, flow_delta);
			std::swap(iter_a, iter_b);
		}
		// Now that we are done with iterations, flow the original image
		//_apply_velocity_field_to_flow();
		_apply_velocity_field(_velocity_field, *current_image, *next_image, flow_delta);
		std::swap(current_image, next_image);
		// blend the current and next for fun
		//blend_images(*current, *next, 0.5f);
		// Save the flowed image
		std::string ext = _image_to_flow.get_ext();
		if (!output_dir.empty()){
			std::string img_n = StringFuncs::get_zero_padded_number_string(indices_used[i], 4);
			std::string out_filename = output_dir + "/flowed_" + img_n + "." + ext;
			current_image->oiio_write_to(out_filename);
		}
	}
	return *current_image;
}

void OpticalFlow::_set_gradient_squared_images() noexcept {
	_dIx_sq.set_to(_dIx);
	_dIx_sq *= _dIx;
	_dIy_sq.set_to(_dIy);
	_dIy_sq *= _dIy;
	_dIx_dIy.set_to(_dIx);
	_dIx_dIy *= _dIy;
}

void OpticalFlow::_build_ensemble_average_in_sequence(
	const ImageData& next_image,
	const ImageData& current_image,
	const ImageData& image_gradient,
	int ensemble_avg_half_width,
	ImageData& output_image
){

	// Id is the displaced image(the next image in the sequence) and Iu is the current image
	output_image.set_to(next_image);
	output_image.subtract_then_multiply(current_image, image_gradient);

	// 2a. Now compute the ensemble averages of these
	ImageDataModifier::ensemble_average(output_image, ensemble_avg_half_width, output_image);
	//output_image = gaussian_average(Q, ensemble_avg_half_width, output_image);
}

void OpticalFlow::_compute_correlation_matrix_components(
	const ImageData& dIx_sq,
	const ImageData& dIy_sq,
	const ImageData& dIx_dIy,
	int ensemble_avg_half_width,
	corr_comps& output_components
){
	
	// c00 is the first element in output components, c11 is the second, and c_off_diag is the third
	ImageData& c00 = std::get<0>(output_components);
	ImageData& c11 = std::get<1>(output_components);
	ImageData& c_off_diag = std::get<2>(output_components);
	
	ImageDataModifier::ensemble_average(dIx_sq, ensemble_avg_half_width, c00);
	ImageDataModifier::ensemble_average(dIy_sq, ensemble_avg_half_width, c11);
	ImageDataModifier::ensemble_average(dIx_dIy, ensemble_avg_half_width, c_off_diag);
}

void OpticalFlow::_compute_velocity_field(
	const ImageData& Qx,
	const ImageData& Qy,
	const ImageData& c00,
	const ImageData& c11,
	const ImageData& c_off_diag,
	ImageData& velocity_field
){
	const int w = velocity_field.get_width();
	const int h = velocity_field.get_height();
	// Divide by 2 because velocity field has dx/dy per channel
	const int channels = velocity_field.get_channels() / 2;

	// Now compute the velcity field with V = Q * C^-1
	// Inverse of 2x2 matrix C is (1/det) * [c11, -c01;
	//										 -c10, c00]
	// where det = c00 * c11 - c01 * c10
	#pragma omp parallel for
	for (int j = 0; j < h; j++){
		for (int i = 0; i < w; i++){
			// Start with the Qs component
			auto qx = Qx.get_pixel_values(i, j);
			auto qy = Qy.get_pixel_values(i, j);
			
			// Now get the C^-1 components
			auto c00_pix = c00.get_pixel_values(i, j);
			auto c11_pix = c11.get_pixel_values(i, j);
			auto coff_diag = c_off_diag.get_pixel_values(i, j);
			for (int c = 0; c < channels; c++){
				float det = c00_pix[c] * c11_pix[c] - coff_diag[c] * coff_diag[c];
				if (std::abs(det) < 1e-6){
					// Singular matrix, set velocity to zero
					velocity_field.set_pixel_value(i, j, c * 2, 0.0f);
					velocity_field.set_pixel_value(i, j, c * 2 + 1, 0.0f);
				} else {
					float inv_c00 = c11_pix[c] / det;
					float inv_c11 = c00_pix[c] / det;
					float inv_coff_diag = -coff_diag[c] / det;

					// Now compute D = Q * C^-1
					// Q = [qx, qy] = 1 x 2 matrix. 
					// This way Q * C^-1 = 1 x 2 * 2 x 2 = 1 x 2 matrix so we get (dx, dy)
					float dx = qx[c] * inv_c00 + qy[c] * inv_coff_diag;
					float dy = qx[c] * inv_coff_diag + qy[c] * inv_c11;
					
					velocity_field.set_pixel_value(i, j, c * 2, dx);
					velocity_field.set_pixel_value(i, j, c * 2 + 1, dy);
				}
			}
		}
	}
}

void OpticalFlow::_apply_velocity_field(
	const ImageData& velocity_field,
	const ImageData& input_image,
	ImageData& output_image,
	float flow_delta
){
	const int w = input_image.get_width();
	const int h = input_image.get_height();
	const int channels = input_image.get_channels();
	const float neg_factor = _negative ? -1.0f : 1.0f;
	#pragma omp parallel for
	for (int j = 0; j < h; j++){
		float y = (float)j;
		for (int i = 0; i < w; i++){
			float x = (float)i;
			// We have per channel velocity, so we need to do the bilinear interp for just a given channel
			for (int c = 0; c < channels; c++){
				// For greyscale
				//float dx = velocity_field.get_pixel_value(i, j, 0) * neg_factor;
				//float dy = velocity_field.get_pixel_value(i, j, 1) * neg_factor;
				// For standard img
				float dx = velocity_field.get_pixel_value(i, j, c * 2) * neg_factor * flow_delta;
				float dy = velocity_field.get_pixel_value(i, j, c * 2 + 1) * neg_factor * flow_delta;
				
				float sample_x = x + dx;;
				float sample_y = y + dy;

				// Clamp to image bounds
				sample_x = std::min(std::max(sample_x, 0.0f), static_cast<float>(w - 1));
				sample_y = std::min(std::max(sample_y, 0.0f), static_cast<float>(h - 1));
				// Read from current, write to next
				float c_interp = input_image.interpolate_bilinear(sample_x, sample_y, c);
				output_image.set_pixel_value(i, j, c, c_interp);
			}
		}
	}
}

void OpticalFlow::set_new_target_image(const ImageData& new_target_image){
	if (new_target_image.dimensions_match(_image_to_flow)){
		_image_to_flow.set_to(new_target_image);
		_image_to_flow_buffer.set_to(new_target_image);
	} else {
		_init_data_images(new_target_image);
	}
}

void OpticalFlow::set_new_image_sequence(std::vector<ImageData>& new_image_sequence){
	_image_sequence = new_image_sequence;
	// confirm that dimension match at least the first image
	if (!_image_to_flow.dimensions_match(_image_sequence[0])){
		_init_data_images(_image_sequence[0]);
	}
}


void OpticalFlow::_init_data_images(const ImageData& img_to_flow) noexcept{
	_image_to_flow = img_to_flow.duplicate();
	_image_to_flow_buffer.set_to(_image_to_flow);

	const int w = img_to_flow.get_width();
	const int h = img_to_flow.get_height();
	const int channels = img_to_flow.get_channels();
	
	_velocity_field.set_dimensions(w, h, 2 * channels);
	_iteration_velocity_field.set_dimensions(w, h, 2 * channels);

	_dIx.set_dimensions(w, h, channels);
	_dIy.set_dimensions(w, h, channels);
	_dIx_sq.set_dimensions(w, h, channels);
	_dIy_sq.set_dimensions(w, h, channels);
	_dIx_dIy.set_dimensions(w, h, channels);
	_Qx.set_dimensions(w, h, channels);
	_Qy.set_dimensions(w, h, channels);
	_c00.set_dimensions(w, h, channels);
	_c11.set_dimensions(w, h, channels);
	_c_off_diag.set_dimensions(w, h, channels);
}