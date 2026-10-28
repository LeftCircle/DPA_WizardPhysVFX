#include "image_data_modifier.h"


void ImageDataModifier::gamma_filter(ImageData& image, float gamma) {
	if (gamma <= 0.0f) {
		throw std::invalid_argument("Gamma value must be greater than 0.");
	}
	
	image.transform_all_values_by([gamma](float val) {
		return pow(val, gamma);
	});
}

/* Applies a linear convolution on the edited image */
void ImageDataModifier::bounded_linear_convolution(ImageData& image, const Stencil& stencil) {
	const ImageData reference_image = image.duplicate();
	bounded_linear_convolution(stencil, reference_image, image);
}

void ImageDataModifier::bounded_linear_convolution(const Stencil& stencil, const ImageData& input_image, ImageData& output_image) {
	if (stencil.get_halfwidth() < 1) {
		throw std::invalid_argument("Stencil half-width must be at least 1.");
	}
	if (input_image.get_width() != output_image.get_width() ||
		input_image.get_height() != output_image.get_height() ||
		input_image.get_channels() != output_image.get_channels()) {
		throw std::invalid_argument("Input and output images must have the same dimensions and channels.");
	}

	int width = input_image.get_width();
	int height = input_image.get_height();
	int channels = input_image.get_channels();
	int half_width = stencil.get_halfwidth();

	#pragma omp parallel for
	for (int y = 0; y < height; y++) {
		int y_reference_min = y - half_width;
		int y_reference_max = y + half_width;
		for (int x = 0; x < width; x++) {
			std::vector<float> reference_pixel(channels, 0.0f);
			std::vector<float> new_pixel(channels, 0.0f);
			int x_reference_min = x - half_width;
			int x_reference_max = x + half_width;
			// Accumulate values from the reference image
			for (int j = y_reference_min; j <= y_reference_max; j++) {
				if (j < 0 || j >= height) {
					continue;
				}
				int stencil_y = j - y + half_width;
				for (int i = x_reference_min; i <= x_reference_max; i++) {
					if (i < 0 || i >= width) {
						continue;
					}
					int stencil_x = i - x + half_width;
					input_image.get_pixel_values(i, j, reference_pixel);
					for (int c = 0; c < channels; ++c) {
						new_pixel[c] += reference_pixel[c] * stencil(stencil_x, stencil_y);
					}
				}
			} // End stencil accumulation
			output_image.set_pixel_values(x, y, new_pixel);
		}
	}
}

void ImageDataModifier::wrapping_linear_convolution(ImageData& image, const Stencil& stencil){
	const ImageData reference_image = image.duplicate();
	wrapping_linear_convolution(stencil, reference_image, image);
}

void ImageDataModifier::wrapping_linear_convolution(const Stencil& stencil, const ImageData& input_image, ImageData& output_image){
	if (stencil.get_halfwidth() < 1) {
		throw std::invalid_argument("Stencil half-width must be at least 1.");
	}
	if (input_image.get_width() != output_image.get_width() ||
		input_image.get_height() != output_image.get_height() ||
		input_image.get_channels() != output_image.get_channels()) {
		throw std::invalid_argument("Input and output images must have the same dimensions and channels.");
	}

	int width = input_image.get_width();
	int height = input_image.get_height();
	int channels = input_image.get_channels();
	int half_width = stencil.get_halfwidth();

	#pragma omp parallel for
	for (int y = 0; y < height; y++) {
		int y_reference_min = y - half_width;
		int y_reference_max = y + half_width;
		for (int x = 0; x < width; x++) {
			std::vector<float> reference_pixel(channels, 0.0f);
			std::vector<float> new_pixel(channels, 0.0f);
			int x_reference_min = x - half_width;
			int x_reference_max = x + half_width;
			// Accumulate values from the reference image
			for (int j = y_reference_min; j <= y_reference_max; j++) {
				int input_y = j;
				if (input_y < 0){
					input_y = height + input_y;
				} else if (input_y >= height){
					input_y -= height;
				}
				int stencil_y = j - y + half_width;
				for (int i = x_reference_min; i <= x_reference_max; i++) {
					int input_x = i;
					if (input_x < 0) {
						input_x += width;
					} else if (input_x >= width){
						input_x -= width;
					}
					int stencil_x = i - x + half_width;
					input_image.get_pixel_values(input_x, input_y, reference_pixel);
					for (int c = 0; c < channels; ++c) {
						new_pixel[c] += reference_pixel[c] * stencil(stencil_x, stencil_y);
					}
				}
			} // End stencil accumulation
			output_image.set_pixel_values(x, y, new_pixel);
		}
	}
}


void ImageDataModifier::clear(ImageData& image){
	image.set_pixel_values(0.0f);
}

void ImageDataModifier::downscale(ImageData& image, const int new_width, const int new_height, bool ignore_alpha_of_zero){
	if (new_width > image.get_width() || new_height > image.get_height()){
		std::cout << "Width or height is not smaller" << std::endl;
		return;
	}

	// Create a new image
	ImageData new_img(new_width, new_height, image.get_channels());
	new_img.set_pixel_values(0.0f);
	const int channels = image.get_channels();
	const float dx_new = 1.0 / static_cast<float>(new_width);
	const float dy_new = 1.0 / static_cast<float>(new_height);

	const float new_widthf = static_cast<float>(new_width);
	const float new_heightf = static_cast<float>(new_height);

	const int old_width = image.get_width();
	const int old_height = image.get_height();
	const float old_widthf = static_cast<float>(old_width);
	const float old_heightf = static_cast<float>(old_height);
	std::vector<float> sum_of_weights(new_width * new_height, 0);
	// We only have to loop over each pixel in the OG image. Each pixel is basically falling into a 2D bucket based on the size
	// of the new image
	for(int j = 0; j < old_height; j++){
		float y = static_cast<float>(j) / old_heightf;
		int new_j = int(y * new_heightf);
		float new_y = static_cast<float>(new_j) / new_heightf;
		for(int i = 0; i < old_width; i++){
			float x = static_cast<float>(i) / old_widthf;
			int new_i = int(x * new_widthf);
			float new_x = static_cast<float>(new_i) / new_widthf;
			// Add the value to the bucket
			float x_w = x - new_x - dx_new;
			float y_w = y - new_y - dy_new;
			float weight = x_w * x_w + y_w * y_w;
			if (ignore_alpha_of_zero && channels == 4){
				float alpha = image.get_pixel_value(i, j, 3);
				if (alpha == 0.0f){
					continue;
				}
			}
			sum_of_weights[new_j * new_width + new_i] += weight;
			for (int c = 0; c < channels; c++){
				float pc = image.get_pixel_value(i, j, c);
				new_img.add_value(new_i, new_j, c, pc * weight);
			}
		}
	}
	// Now divide all values by the weight
	new_img.divide_each_pixel_by(sum_of_weights);
	image.set_to(new_img);

}

void ImageDataModifier::quantize(ImageData& image, int levels){
	int w = image.get_width();
	int h = image.get_height();
	int channels = image.get_channels();
	#pragma omp parallel for
	for (int j = 0; j < h; j++){
		for (int i = 0; i < w; i++){
			for (int c = 0; c < channels; c++){
				float cc = image.get_pixel_value(i, j, c);
				cc = (float)(int)(cc * levels) / (float)levels;
				image.set_pixel_value(i, j, c, cc);
			}
		}
	}
}

void ImageDataModifier::palette_match(ImageData& image, const std::vector<float>& colors){
	// Start by quantizing the image to the same number of colors as the 
	// palette. Then create a dict of color to closest palette
	const int n_colors = colors.size() / 3;
	quantize(image, n_colors);

	// Create a hash function for RGB color vectors
    auto color_hash = [](const std::vector<float>& color) {
        size_t h1 = std::hash<float>{}(color[0]);
        size_t h2 = std::hash<float>{}(color[1]);
        size_t h3 = std::hash<float>{}(color[2]);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
	};

	std::unordered_map<std::vector<float>, std::vector<float>, decltype(color_hash)> color_map(10, color_hash);

	int w = image.get_width();
    int h = image.get_height();

	// Collect unique quantized colors
    for (int j = 0; j < h; j++){
        for (int i = 0; i < w; i++){
            auto quantized_color = image.get_pixel_values(i, j);
            quantized_color.resize(3); // Only RGB
            
            if (color_map.find(quantized_color) == color_map.end()){
                // Find closest palette color
                float min_dist = std::numeric_limits<float>::max();
                std::vector<float> closest_color(3);
                
                for (int p = 0; p < n_colors; p++){
                    float dr = colors[p*3] - quantized_color[0];
                    float dg = colors[p*3+1] - quantized_color[1];
                    float db = colors[p*3+2] - quantized_color[2];
                    float dist = dr*dr + dg*dg + db*db;
                    
                    if (dist < min_dist){
                        min_dist = dist;
                        closest_color = {colors[p*3], colors[p*3+1], colors[p*3+2]};
                    }
                }
                color_map[quantized_color] = closest_color;
            }
			// Early out once we have mapped all colors
			// if (color_map.size() >= static_cast<size_t>(n_colors)){
			// 	break;
			// }
        }
    }
    
    // Apply palette mapping
    for (int j = 0; j < h; j++){
        for (int i = 0; i < w; i++){
            auto quantized_color = image.get_pixel_values(i, j);
            quantized_color.resize(3);
            auto it = color_map.find(quantized_color);
            if (it != color_map.end()){
                image.set_first_three_channels(i, j, it->second);
            } else{
				std::cout << "Color not found in map!" << std::endl;
			}
        }
    }
}

void ImageDataModifier::julia_set(
	ImageData& image,
	const Point& center,
	const double range,
	const IFSFunction& fract,
	const LUT<Color>& color_lut
){
	double w = (double)image.get_width();
	double h = (double)image.get_height();
	double R = 2.0; // b/c bounds are -1, 1
	#pragma omp parallel for
	for (int j = 0; j < image.get_height(); j++){
		for (int i = 0; i < image.get_width(); i++){
			Point P;
			P.x = 2.0 * (double)i / w - 1.0;
			P.y = 2.0 * (double)j / h - 1.0;
			P *= range;
			P += center;
			Point fractal_point = fract(P);
			
			double rate = fractal_point.magnitude() / R;
			Color frac_col(0.0, 0.0, 0.0);
			if (rate <= 1.0){
				frac_col = color_lut.lerp(rate);
			}
			std::vector<float> color_vec(3, 0.0);
			color_vec[0] = frac_col.r;
			color_vec[1] = frac_col.g;
			color_vec[2] = frac_col.b;
			image.set_first_three_channels(i, j, color_vec);
		}
	}
}

ImageData ImageDataModifier::greyscale(const ImageData& img){
	int w = img.get_width();
	int h = img.get_height();
	int channels = img.get_channels();
	ImageData grey_img(w, h, 1);

	#pragma omp parallel for
	for (int j = 0; j < h; j++){
		for (int i = 0; i < w; i++){
			auto pixel_vals = img.get_pixel_values(i, j);
			float grey_val = 0.0f;
			if (channels >= 3){
				grey_val = 0.299f * pixel_vals[0] + 0.587f * pixel_vals[1] + 0.114f * pixel_vals[2];
			} else if (channels == 1){
				grey_val = pixel_vals[0];
			} else{
				grey_val = pixel_vals[0]; // Just use the first channel
			}
			grey_img.set_pixel_value(i, j, 0, grey_val);
		}
	}
	return grey_img;
}

void ImageDataModifier::convert_to_contrast_units(ImageData& image) {
	const auto avg = image.get_average();
	const auto rms = image.get_rms(avg);
	for (int j = 0; j < image.get_height(); j++){
		for (int i = 0; i < image.get_width(); i++){ 
			auto pixels = image.get_pixel_values(i, j);
			for (int c = 0; c < image.get_channels(); c++){
				pixels[c] -= avg[c];
				pixels[c] /= rms[c];
			}
			image.set_pixel_values(i, j, pixels);
		}
	}
}

void ImageDataModifier::histogram_equalize(ImageData& image, const int n_bins, bool ignore_alpha) {
	// Start by getting max and min
	std::vector<float> max = image.get_max();
	auto min = image.get_min();
	const int width = image.get_width();
	const int height = image.get_height();
	const float n_pixelsf = static_cast<float>(width * height);
	const int og_n_channels = image.get_channels();
	int n_channels = ignore_alpha ? 3 : og_n_channels;

	// Now create bins for each channel. We need one bin for each channel, so 
	std::vector<int> bins(n_channels * n_bins, 0);
	std::vector<float> bin_width(n_channels, 0.0);
	for (int i = 0; i < n_channels; i++){
		bin_width[i] = (max[i] - min[i]) / static_cast<float>(n_bins);
	}
	
	// Now accumulate the pixels into the bins
	for (int j = 0; j < image.get_height(); j++){
		for (int i = 0; i < image.get_width(); i++){
			for (int c = 0; c < n_channels; c++){
				float p = image.get_pixel_value(i, j, c);
				int bin_index = int((p - min[c]) / bin_width[c]);
				bin_index = std::min(std::max(bin_index, 0), n_bins - 1);
				bins[bin_index * n_channels + c] += 1;
			}
		}
	}
	
	// Create the probability distribution function. Just divide each bin
	// by the total number of pixels
	std::vector<float> pdf(n_channels * n_bins, 0.0f);
	std::vector<float> cdf(n_channels * n_bins, 0.0f);
	std::vector<float> channel_probabilities(n_channels, 0.0);
	for (int i = 0; i < n_channels * n_bins; i++){
		float bin_prob = static_cast<float>(bins[i]) / n_pixelsf;
		channel_probabilities[i % n_channels] += bin_prob;
		pdf[i] = bin_prob;
		cdf[i] = channel_probabilities[i % n_channels];
	}

	// Now loop over each pixel and replace the pixel with the cdf value of the
	// bin that the pixel falls into
	for (int j = 0; j < image.get_height(); j++){
		for (int i = 0; i < image.get_width(); i++){
			for (int c = 0; c < n_channels; c++){
				float p = image.get_pixel_value(i, j, c);
				int bin_index = int((p - min[c]) / bin_width[c]) * n_channels + c;
				bin_index = std::min(std::max(bin_index, 0), n_bins * n_channels - 1);
				image.set_pixel_value(i, j, c, cdf[bin_index]);
			}
		}
	}
}


ImageData ImageDataModifier::ensemble_average(const ImageData& img, int half_width){
	const int w = img.get_width();
	const int h = img.get_height();
	const int channels = img.get_channels();
	ImageData avg_img(w, h, channels);
	#pragma omp parallel for
	for (int j = 0; j < h; j++){
		for (int i = 0; i < w; i++){
			auto avg = img.get_average_ensemble(i, j, half_width);
			avg_img.set_pixel_values(i, j, avg);
		}
	}
	return avg_img;
}

void ImageDataModifier::ensemble_average(const ImageData& img, int half_width, ImageData& output_img){
	if (!img.dimensions_match(output_img)){
		throw std::invalid_argument("Input image and output image dimensions do not match.");
	}
	const int w = img.get_width();
	const int h = img.get_height();
	const int channels = img.get_channels();

	#pragma omp parallel for
	for (int j = 0; j < h; j++){
		for (int i = 0; i < w; i++){
			auto avg = img.get_average_ensemble(i, j, half_width);
			output_img.set_pixel_values(i, j, avg);
		}
	}
}

void ImageDataModifier::blend_images(ImageData& img_to_blend, const ImageData& other_image, float weight){
	if (!other_image.dimensions_match(img_to_blend)){
		throw std::invalid_argument("Input image and edited image dimensions do not match.");
	}
	const int w = img_to_blend.get_width();
	const int h = img_to_blend.get_height();
	const int channels = img_to_blend.get_channels();

	#pragma omp parallel for
	for (int j = 0; j < h; j++){
		for (int i = 0; i < w; i++){
			for (int c = 0; c < channels; c++){
				float val1 = img_to_blend.get_pixel_value(i, j, c);
				float val2 = other_image.get_pixel_value(i, j, c);
				float blended_val = (1.0f - weight) * val1 + weight * val2;
				img_to_blend.set_pixel_value(i, j, c, blended_val);
			}
		}
	}
}

void ImageDataModifier::bilinear_interpolate_each_channel(ImageData& image){
	const int w = image.get_width();
	const int h = image.get_height();
	const int channels = image.get_channels();
	ImageData interp_img(w, h, channels);

	#pragma omp parallel for
	for (int j = 0; j < h; j++){
		for (int i = 0; i < w; i++){
			for (int c = 0; c < channels; c++){
				float x = static_cast<float>(i);
				float y = static_cast<float>(j);
				float interp_val = image.interpolate_bilinear(x, y, c);
				interp_img.set_pixel_value(i, j, c, interp_val);
			}
		}
	}
	image.set_to(interp_img);
}