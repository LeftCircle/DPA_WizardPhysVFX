#ifndef image_data_H
#define image_data_H

#include <GL/glut.h>
#include <memory>
#include <vector>
#include <stdexcept>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include "string_funcs.h"


class ImageData {
public:
	using pixel = std::vector<float>;

	ImageData();
	ImageData(const char* filename) { oiio_read(filename); }
	ImageData(const ImageData& other);
	ImageData(const int width, const int height, const int channels);
	~ImageData();

	
	void oiio_read(const char* filename);
	void oiio_write();
	void oiio_write(const std::string& file_extension);
	void oiio_write_to(const std::string& filename);
	void oiio_write_to(const std::string& filename, const std::string& label);
	
	void set_to(const ImageData& other);
	void set_dimensions(int width, int height, int channels);
	bool dimensions_match(const ImageData& other) const;
	
	const pixel get_pixel_values(int x, int y) const;
	pixel get_pixel_values(int x, int y);
	void get_pixel_values(int x, int y, pixel& values) const;
	const float& get_pixel_value(int x, int y, int c) const { return _image_data_ptr[(y * _width + x) * _channels + c]; }

	void set_pixel_value(const int x, const int y, const int c, const float val);
	void set_pixel_values(int x, int y, const pixel& values);
	void set_pixel_values(const pixel& values);
	void set_pixel_values(const float val) noexcept;
	void set_first_three_channels(int x, int y, const pixel& values);
	void set_first_three_channels(int x, int y, float r, float g, float b);


	void add_values(int x, int y, float r, float g, float b, float a);
	void add_value(int x, int y, int channel, float val);
	void add_pixels_into(int x, int y, pixel& pixel_values);
	void divide_each_pixel_by(const pixel& vals);
	
	void mix_rgb_values(int x, int y, float r, float g, float b);
	void mix_rgb_values(int x, int y, float r, float g, float b, float weight);

	void scale_pixel_values(const int x, const int y, const float scale_factor);
	void scale_values(float scale_factor);

	long get_index(int x, int y, int channel) const;
	long get_index(int x, int y) const;
	
	const int& get_width() const { return _width; }
	const int& get_height() const { return _height; }
	const int& get_channels() const { return _channels; }
	int get_data_len() const;
	
	pixel get_max() const;
	pixel get_min() const;

	pixel get_average() const;
	int get_n_pixels_withc_channel_value(int channel, float value, float epsilon = 0.001) const;
	pixel get_rms() const;
	pixel get_rms(const pixel& avg) const;

	pixel interpolate_nearest_neighbor(const float x, const float y) const;
	pixel interpolate_bilinear(const float x, const float y) const;
	float interpolate_bilinear(const float x, const float y, const int c) const;
	pixel get_average_ensemble(int x, int y, int half_width) const;


	ImageData get_x_y_gradients() const;
	ImageData get_x_gradient() const;
	ImageData get_y_gradient() const;

	void write_x_gradient_into(ImageData& out_image) const;
	void write_y_gradient_into(ImageData& out_image) const;

	void clear();
	void flip();
	
	std::string get_output_file_name() const { return _file_name + "_out." + _file_type; }
	std::string get_ext() const { return _file_type; }
	
	ImageData duplicate() const;

	ImageData& operator=(const ImageData& other);
	ImageData& operator*=(const ImageData& other);
	ImageData& operator+=(const ImageData& other);
	ImageData& subtract_then_multiply(const ImageData& sub_img, const ImageData& mul_img);

	template<typename Func>
	void transform_all_values_by(Func func) {
		int data_len = get_data_len();
		#pragma omp parallel for
		for (int i = 0; i < data_len; ++i) {
			_image_data_ptr[i] = func(_image_data_ptr[i]);
		}
	}
	
	void gl_draw_pixels() const;

protected:
	int _width;
	int _height;
	int _channels;
	
	std::unique_ptr<float[]> _image_data_ptr;

private:
	std::string _file_type;
	std::string _file_name;
};


#endif