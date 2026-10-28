#ifndef IMAGE_DATA_MODIFIER_H
#define IMAGE_DATA_MODIFIER_H

#include <vector>

#include "image_data.h"
#include "stencil.h"
#include "IFSFunction.h"
#include "lut.h"
#include "stats.h"


class ImageDataModifier {
public:
	ImageDataModifier() = default;
	~ImageDataModifier() = default;

	static void gamma_filter(ImageData& image, float gamma);
	static void bounded_linear_convolution(ImageData& image, const Stencil& stencil);
	static void bounded_linear_convolution(const Stencil& stencil, const ImageData& input_image, ImageData& output_image);
	static void wrapping_linear_convolution(ImageData& image, const Stencil& stencil);
	static void wrapping_linear_convolution(const Stencil& stencil, const ImageData& input_image, ImageData& output_image);
	static void clear(ImageData& image);
	
	static void julia_set(
		ImageData& image,
		const Point& center,
		const double range,
		const IFSFunction& fract,
		const LUT<Color>& color_lut
	);
	
	static void convert_to_contrast_units(ImageData& image);
	static void histogram_equalize(ImageData& image, const int n_bins = 500, bool ignore_alpha = false);
	static ImageData greyscale(const ImageData& img);

	static void downscale(ImageData& image, const int new_width, const int new_height, bool ignore_alpha_of_zero = false);
	static void palette_match(ImageData& image, const std::vector<float>& colors);
	static void quantize(ImageData& image, int levels);
	static ImageData ensemble_average(const ImageData& image, int half_width = 1);
	static void ensemble_average(const ImageData& img, int half_width, ImageData& output_img);
	static void blend_images(ImageData& img_to_blend, const ImageData& other_image, float weight);

	static void bilinear_interpolate_each_channel(ImageData& image);
};



#endif // IMAGE_EDITING_H