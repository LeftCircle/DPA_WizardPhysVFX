#include "image_data.h"

using namespace OIIO;

ImageData::ImageData()
	: _width(0), _height(0), _channels(0), _image_data_ptr(nullptr) {}

ImageData::~ImageData() {
}

ImageData::ImageData(const int width, const int height, const int channels) {
	_width = width;
	_height = height;
	_channels = channels;
	_image_data_ptr = std::make_unique<float[]>(get_data_len());
	set_pixel_values(0.0f);
}

// Deep copy constructor and assignement operator
ImageData::ImageData(const ImageData& other)
	: _width(other._width), _height(other._height), _channels(other._channels) {
	_file_name = other._file_name;
	_file_type = other._file_type;
	int data_len = other.get_data_len();
	_image_data_ptr = std::make_unique<float[]>(data_len);
	std::copy(other._image_data_ptr.get(), other._image_data_ptr.get() + data_len, _image_data_ptr.get());
}

void ImageData::set_to(const ImageData& other){
	_width = other._width;
	_height = other._height;
	_channels = other._channels;
	//_file_name = other._file_name;
	//_file_type = other._file_type;
	int data_len = other.get_data_len();
	_image_data_ptr = std::make_unique<float[]>(data_len);
	std::copy(other._image_data_ptr.get(), other._image_data_ptr.get() + data_len, _image_data_ptr.get());
}

ImageData ImageData::duplicate() const {
	return ImageData(*this);
}

ImageData& ImageData::operator=(const ImageData& other) {
	if (this != &other) {
		_width = other._width;
		_height = other._height;
		_channels = other._channels;
		_file_name = other._file_name;
		_file_type = other._file_type;
		int data_len = other.get_data_len();
		_image_data_ptr = std::make_unique<float[]>(data_len);
		std::copy(other._image_data_ptr.get(), other._image_data_ptr.get() + data_len, _image_data_ptr.get());
	}
	return *this;
}

ImageData& ImageData::operator*=(const ImageData& other) {
	if (_width != other._width || _height != other._height || _channels != other._channels) {
		throw std::invalid_argument("Image dimensions and channels must match for multiplication.");
	}

	int data_len = get_data_len();
	#pragma omp parallel for
	for (int i = 0; i < data_len; ++i) {
		_image_data_ptr[i] *= other._image_data_ptr[i];
	}
	return *this;
}

ImageData& ImageData::operator+=(const ImageData& other) {
	if (!dimensions_match(other)) {
		throw std::invalid_argument("Image dimensions and channels must match for addition.");
	}

	int data_len = get_data_len();
	#pragma omp parallel for
	for (int i = 0; i < data_len; ++i) {
		_image_data_ptr[i] += other._image_data_ptr[i];
	}
	return *this;
}
	
bool ImageData::dimensions_match(const ImageData& other) const {
	return (_width == other._width) && (_height == other._height) && (_channels == other._channels);
}

ImageData& ImageData::subtract_then_multiply(const ImageData& sub_img, const ImageData& mul_img) {
	if (_width != sub_img._width || _height != sub_img._height || _channels != sub_img._channels ||
		_width != mul_img._width || _height != mul_img._height || _channels != mul_img._channels) {
		throw std::invalid_argument("Image dimensions and channels must match for subtract_then_multiply.");
	}

	int data_len = get_data_len();
	#pragma omp parallel for
	for (int i = 0; i < data_len; ++i) {
		_image_data_ptr[i] = ( _image_data_ptr[i] - sub_img._image_data_ptr[i] ) * mul_img._image_data_ptr[i];
	}
	return *this;
}

void ImageData::set_dimensions(int width, int height, int channels) {
	_width = width;
	_height = height;
	_channels = channels;
	int data_len = get_data_len();
	_image_data_ptr = std::make_unique<float[]>(data_len);
}

int ImageData::get_data_len() const {
	return _width * _height * _channels;
}

long ImageData::get_index(int x, int y, int channel) const {
	if (x < 0 || x >= _width || y < 0 || y >= _height || channel < 0 || channel >= _channels) {
		throw std::out_of_range("Invalid coordinates or channel");
	}
	return channel + _channels * (x + y * _width);
}

long ImageData::get_index(int x, int y) const {
	if (x < 0 || x >= _width || y < 0 || y >= _height) {
		throw std::out_of_range("Invalid coordinates");
	}
	return _channels * (x + y * _width);
}

void ImageData::set_pixel_value(const int x, const int y, const int c, const float val){
	const int index = get_index(x, y, c);
	_image_data_ptr[index] = val;
}

void ImageData::set_pixel_values(int x, int y, const std::vector<float>& values) {
	if (x < 0 || x >= _width || y < 0 || y >= _height || values.size() != static_cast<size_t>(_channels)) {
		throw std::out_of_range("Invalid coordinates or values size");
	}
	int index = (y * _width + x) * _channels;
	for (int c = 0; c < _channels; ++c) {
		_image_data_ptr[index + c] = values[c];
	}
}

void ImageData::set_pixel_values(const std::vector<float>& values) {
	if (values.size() != static_cast<size_t>(get_data_len())) {
		throw std::out_of_range("Invalid values size");
	}
	#pragma omp parallel for
	for (int i = 0; i < get_data_len(); ++i) {
		_image_data_ptr[i] = values[i];
	}
}

void ImageData::set_pixel_values(const float val) noexcept{
	#pragma omp parallel for
	for (int i = 0; i < get_data_len(); i++) {
		_image_data_ptr[i] = val;
	}
}

void ImageData::set_first_three_channels(int x, int y, const std::vector<float>& values){
	int index = (y * _width + x) * _channels;
	for (int c = 0; c < 3; ++c) {
		_image_data_ptr[index + c] = values[c];
	}
}

void ImageData::set_first_three_channels(int x, int y, float r, float g, float b){
	int index = (y * _width + x) * _channels;
	_image_data_ptr[index] = r;
	_image_data_ptr[index + 1] = g;
	_image_data_ptr[index + 2] = b;
}

void ImageData::add_values(int x, int y, float r, float g, float b, float a){
	assert(_channels >= 4);
	int start_index = (y * _width + x) * _channels;
	_image_data_ptr[start_index] += r;
	_image_data_ptr[start_index + 1] += g;
	_image_data_ptr[start_index + 2] += b;
	_image_data_ptr[start_index + 3] += a;
}

void ImageData::add_value(int x, int y, int channel, float val){
	assert(channel < _channels);
	int index = (y * _width + x) * _channels + channel;
	_image_data_ptr[index] += val;
}

void ImageData::add_pixels_into(int x, int y, std::vector<float>& pixel_values){
	int start_index = (y * _width + x) * _channels;
	for (int c = 0; c < _channels; c++){
		pixel_values[c] += _image_data_ptr[start_index + c];
	}
}

void ImageData::divide_each_pixel_by(const std::vector<float>& vals) {
	#pragma omp parallel for
	for (int j = 0; j < _height; j++){
		for (int i = 0; i < _width; i++){
			for (int c = 0; c < _channels; c++){
				int idx = get_index(i, j, c);
				_image_data_ptr[idx] /= vals[j * _width + i];
			}
		}
	}
}

void ImageData::mix_rgb_values(int x, int y, float r, float g, float b){
	int index = (y * _width + x) * _channels;
	_image_data_ptr[index] = (_image_data_ptr[index] + r) / 2.0f;
	_image_data_ptr[index + 1] = (_image_data_ptr[index + 1] + g) / 2.0f;
	_image_data_ptr[index + 2] = (_image_data_ptr[index + 2] + b) / 2.0f;
}

void ImageData::mix_rgb_values(int x, int y, float r, float g, float b, float weight){
	int index = (y * _width + x) * _channels;
	_image_data_ptr[index] = (_image_data_ptr[index] * (1.0f - weight)) + (r * weight);
	_image_data_ptr[index + 1] = (_image_data_ptr[index + 1] * (1.0f - weight)) + (g * weight);
	_image_data_ptr[index + 2] = (_image_data_ptr[index + 2] * (1.0f - weight)) + (b * weight);
}

void ImageData::scale_values(float scale_factor){
	const int len = get_data_len();
	#pragma omp parallel for
	for (int i = 0; i < len; i++){
		_image_data_ptr[i] *= scale_factor;
	}
}

void ImageData::scale_pixel_values(const int x, const int y, const float scale_factor){
	int index = get_index(x, y);
	for (int c = 0; c < _channels; c++){
		_image_data_ptr[index + c] *= scale_factor;
	}
}

/* from https://openimageio.readthedocs.io/en/latest/imageinput.html*/
void ImageData::oiio_read(const char* filename) {
	auto inp = ImageInput::open(filename);
    if (!inp) {
        std::cerr << "Error opening image file: " << filename << std::endl;
        return;
    }
	_file_name = StringFuncs::get_file_name(std::string(filename));
	_file_type = StringFuncs::get_file_type(std::string(filename));
    const ImageSpec& spec = inp->spec();
    int xres              = spec.width;
    int yres              = spec.height;
    int nchannels         = spec.nchannels;
    std::vector<float> pixels(xres * yres * nchannels);
    //inp->read_image(0 /*subimage*/, 0 /*miplevel*/, 0 /*chbegin*/,
    //                nchannels /*chend*/, make_span(pixels));
    inp->read_image(0, 0, 0, nchannels, TypeDesc::FLOAT, &pixels[0]);
	inp->close();

	set_dimensions(xres, yres, nchannels);
	std::cout << "Image dimensions: " << _width << " x " << _height << " x " << _channels << std::endl;
	set_pixel_values(pixels);

}

void ImageData::oiio_write() {
	if (!_image_data_ptr) {
		std::cerr << "No image data to write." << std::endl;
		return;
	}
	std::string filename = get_output_file_name();
	std::unique_ptr<ImageOutput> out = ImageOutput::create(filename);
	if (!out) {
		std::cerr << "Error creating image file: " << filename << std::endl;
		return;
	}
	ImageSpec spec(_width, _height, _channels, TypeDesc::FLOAT);
	if (!out->open(filename, spec)) {
		std::cerr << "Error opening image file for writing: " << filename << std::endl;
		return;
	}
	out->write_image(TypeDesc::FLOAT, _image_data_ptr.get());
	out->close();
}

void ImageData::oiio_write(const std::string& file_extension) {
	if (!_image_data_ptr) {
		std::cerr << "No image data to write." << std::endl;
		return;
	}
	std::string filename = _file_name + "_out" + file_extension;
	std::unique_ptr<ImageOutput> out = ImageOutput::create(filename);
	if (!out) {
		std::cerr << "Error creating image file: " << filename << std::endl;
		return;
	}
	ImageSpec spec(_width, _height, _channels, TypeDesc::FLOAT);
	if (!out->open(filename, spec)) {
		std::cerr << "Error opening image file for writing: " << filename << std::endl;
		return;
	}
	out->write_image(TypeDesc::FLOAT, _image_data_ptr.get());
	out->close();
}

void ImageData::oiio_write_to(const std::string& filename) {
	if (!_image_data_ptr) {
		std::cerr << "No image data to write." << std::endl;
		return;
	}
	std::unique_ptr<ImageOutput> out = ImageOutput::create(filename);
	if (!out) {
		std::cerr << "Error creating image file: " << filename << std::endl;
		return;
	}
	ImageSpec spec(_width, _height, _channels, TypeDesc::FLOAT);
	if (!out->open(filename, spec)) {
		std::cerr << "Error opening image file for writing: " << filename << std::endl;
		return;
	}
	out->write_image(TypeDesc::FLOAT, _image_data_ptr.get());
	out->close();
}

void ImageData::oiio_write_to(
    const std::string& filename,
    const std::string& label
){
    if (!_image_data_ptr) {
        std::cerr << "No image data to write.\n";
        return;
    }

    OIIO::ImageSpec spec(_width, _height, _channels, OIIO::TypeDesc::FLOAT);

    // Wraps _image_data_ptr; OIIO does not own or free it.
    OIIO::ImageBuf image(spec, static_cast<void*>(_image_data_ptr.get()));

    if (!OIIO::ImageBufAlgo::render_text(image, 20, 30, label, 18)) {
        std::cerr << "Could not render label onto image.\n";
    }

    auto out = OIIO::ImageOutput::create(filename);
    if (!out || !out->open(filename, spec)) {
        std::cerr << "Could not open image for writing: " << filename << '\n';
        return;
    }

    out->write_image(OIIO::TypeDesc::FLOAT, _image_data_ptr.get());
    out->close();
}

std::vector<float> ImageData::get_pixel_values(int x, int y) {
	std::vector<float> pixel_data(_channels);
	int start_index = get_index(x, y);
	for (int c = 0; c < _channels; c++) {
		pixel_data[c] = _image_data_ptr[start_index + c];
	}
	return pixel_data;
}

const std::vector<float> ImageData::get_pixel_values(int x, int y) const {
	std::vector<float> pixel_data(_channels);
	int start_index = get_index(x, y);
	for (int c = 0; c < _channels; c++) {
		pixel_data[c] = _image_data_ptr[start_index + c];
	}
	return pixel_data;
}

void ImageData::get_pixel_values(int x, int y, std::vector<float>& values) const {
	int start_index = get_index(x, y);
	for (int c = 0; c < _channels; c++) {
		values[c] = _image_data_ptr[start_index + c];
	}
}

std::vector<float> ImageData::get_max() const {
	std::vector<float> max_vals(_channels, -INFINITY);
	for (int j = 0; j < _height; j++){
		for( int i = 0; i < _width; i++){
			for( int c = 0; c < _channels; c++){
				max_vals[c] = std::max(get_pixel_value(i, j, c), max_vals[c]);
			}
		}
	}
	return max_vals;
}

std::vector<float> ImageData::get_min() const {
	std::vector<float> min_vals(_channels, INFINITY);
	for (int j = 0; j < _height; j++){
		for( int i = 0; i < _width; i++){
			for( int c = 0; c < _channels; c++){
				min_vals[c] = std::min(get_pixel_value(i, j, c), min_vals[c]);
			}
		}
	}
	return min_vals;
}

int ImageData::get_n_pixels_withc_channel_value(int channel, float value, float epsilon) const {
	if (channel < 0 || channel >= _channels) {
		throw std::out_of_range("Invalid channel");
	}
	int count = 0;
	for (int j = 0; j < _height; j++){
		for( int i = 0; i < _width; i++){
			if (abs(get_pixel_value(i, j, channel) - value) < epsilon) {
				count++;
			}
		}
	}
	return count;
}

std::vector<float> ImageData::get_average() const {
	std::vector<double> avg_of_each_channel(_channels, 0.0);
	double n_pixels = static_cast<double>(_width * _height);
	for(int j = 0; j < _height; j++){
		std::vector<double> row_avg(_channels, 0.0);
		for(int i = 0; i < _width; i++){
			for (int c = 0; c < _channels; c++){
				row_avg[c] += static_cast<double>(get_pixel_value(i, j, c));
			}
		}
		for (int c = 0; c < _channels; c++){
			row_avg[c] /= n_pixels;
			avg_of_each_channel[c] += row_avg[c];
			row_avg[c] = 0.0;
		}
	}
	std::cout << "Avg = ";
	for(int c = 0; c < _channels; c++){
		std::cout << avg_of_each_channel[c] << " ";
	}
	std::cout << std::endl;
	std::vector<float> avg_of_each_channelf(_channels, 0.0);
	for (int c = 0; c < _channels; c++){
		avg_of_each_channelf[c] = static_cast<float>(avg_of_each_channel[c]);
	}
	return avg_of_each_channelf;
}

std::vector<float> ImageData::get_rms() const {
	auto avgf = get_average();
	return get_rms(avgf);
}

std::vector<float> ImageData::get_rms(const std::vector<float>& avgf) const {
	std::vector<double> avg(_channels, 0.0);
	for (int c = 0; c < _channels; c++){
		avg[c] = static_cast<double>(avgf[c]);
	}
	std::vector<double> rms(_channels, 0.0);
	float n_pixels = static_cast<double>(_width * _height);
	for(int j = 0; j < _height; j++){
		for(int i = 0; i < _width; i++){
			for (int c = 0; c < _channels; c++){
				double pc = static_cast<double>(get_pixel_value(i, j, c));
				double diff = pc - avg[c];
				rms[c] += diff * diff;
			}
		}
	}

	// now sqrt everything
	std::vector<float> rmsf(_channels, 0.0f);
	std::cout << "RMS = ";
	for (int c = 0; c < _channels; c++){
		rms[c] = sqrt(rms[c] / n_pixels);
		rmsf[c] = static_cast<float>(rms[c]);
		std::cout << rms[c] << " ";
	}
	std::cout << std::endl;
	return rmsf;
}

std::vector<float> ImageData::interpolate_nearest_neighbor(const float x, const float y) const {
	int x_nn = static_cast<int>(std::round(x));
	int y_nn = static_cast<int>(std::round(y));
	// Clamp to image boundaries
	x_nn = std::max(0, std::min(x_nn, _width - 1));
	y_nn = std::max(0, std::min(y_nn, _height - 1));
	return get_pixel_values(x_nn, y_nn);
}

std::vector<float> ImageData::interpolate_bilinear(const float x, const float y) const {
	int x0 = static_cast<int>(std::floor(x));
	int x1 = x0 + 1;
	int y0 = static_cast<int>(std::floor(y));
	int y1 = y0 + 1;

	x0 = std::max(0, std::min(x0, _width - 1));
	x1 = std::max(0, std::min(x1, _width - 1));
	y0 = std::max(0, std::min(y0, _height - 1));
	y1 = std::max(0, std::min(y1, _height - 1));


	float wx = x - x0;
	float wy = y - y0;
	float w00 = (1.0f - wx) * (1.0f - wy);
	float w10 = (1.0f - wx) * wy;
	float w01 = wx * (1.0f - wy);
	float w11 = wx * wy;

	std::vector<float> result(_channels, 0.0);

	std::vector<float> c00 = get_pixel_values(x0, y0);
	auto c10 = get_pixel_values(x1, y0);
	auto c01 = get_pixel_values(x0, y1);
	auto c11 = get_pixel_values(x1, y1);
	for (int c = 0; c < _channels; c++){
		result[c] += c00[c] * w00;
		result[c] += c10[c] * w10;
		result[c] += c01[c] * w01;
		result[c] += c11[c] * w11;
	}
	return result;
}

std::vector<float> ImageData::get_average_ensemble(int x, int y, int half_width) const {
	std::vector<float> avg_vals(_channels, 0.0f);
    float total = static_cast<float>(2 * half_width + 1) * static_cast<float>(2 * half_width + 1);
    for (int j = -half_width; j <= half_width; ++j) {
        for (int i = -half_width; i <= half_width; ++i) {
            int xi = x + i;
            int yj = y + j;
            if (xi < 0 || xi >= _width || yj < 0 || yj >= _height) {
                continue;
            }
            long index = (yj * _width + xi) * _channels;
            for (int c = 0; c < _channels; ++c) {
                avg_vals[c] += _image_data_ptr[index + c];
            }
        }
    }

    if (total > 0.0f) {
        for (int c = 0; c < _channels; ++c) {
            avg_vals[c] /= total;
        }
    }
    return avg_vals;
}

float ImageData::interpolate_bilinear(const float x, const float y, const int channel) const {
	int x0 = static_cast<int>(std::floor(x));
	int x1 = x0 + 1;
	int y0 = static_cast<int>(std::floor(y));
	int y1 = y0 + 1;

	x0 = std::max(0, std::min(x0, _width - 1));
	x1 = std::max(0, std::min(x1, _width - 1));
	y0 = std::max(0, std::min(y0, _height - 1));
	y1 = std::max(0, std::min(y1, _height - 1));


	float wx = x - x0;
	float wy = y - y0;
	float w00 = (1.0f - wx) * (1.0f - wy);
	float w10 = (1.0f - wx) * wy;
	float w01 = wx * (1.0f - wy);
	float w11 = wx * wy;

	float result = 0.0f;

	auto c00 = get_pixel_value(x0, y0, channel);
	auto c10 = get_pixel_value(x1, y0, channel);
	auto c01 = get_pixel_value(x0, y1, channel);
	auto c11 = get_pixel_value(x1, y1, channel);
	result += c00 * w00;
	result += c10 * w10;
	result += c01 * w01;
	result += c11 * w11;
	return result;
}

ImageData ImageData::get_x_y_gradients() const {
	// Return a new ImageData object with 2*channels:
	// The first half of the channels are the x gradients, the second half are the y gradients
	ImageData gradients(_width, _height, 2 * _channels);
	#pragma omp parallel for
	for (int j = 0; j < _height; j++){
		for (int i = 0; i < _width; i++){
			for (int c = 0; c < _channels; c++){
				// Compute x gradient
				float gx = 0.0f;
				if (i == 0){
					gx = get_pixel_value(i + 1, j, c) - get_pixel_value(i, j, c);
				} else if (i == _width - 1){
					gx = get_pixel_value(i, j, c) - get_pixel_value(i - 1, j, c);
				} else {
					gx = (get_pixel_value(i + 1, j, c) - get_pixel_value(i - 1, j, c)) / 2.0f;
				}
				gradients.set_pixel_value(i, j, c, gx);

				// Compute y gradient
				float gy = 0.0f;
				if (j == 0){
					gy = get_pixel_value(i, j + 1, c) - get_pixel_value(i, j, c);
				} else if (j == _height - 1){
					gy = get_pixel_value(i, j, c) - get_pixel_value(i, j - 1, c);
				} else {
					gy = (get_pixel_value(i, j + 1, c) - get_pixel_value(i, j - 1, c)) / 2.0f;
				}
				gradients.set_pixel_value(i, j, c + _channels, gy);
			}
		}
	}
	return gradients;
}

ImageData ImageData::get_x_gradient() const {
	// Return a new ImageData object with same channels:
	ImageData gradients(_width, _height, _channels);
	#pragma omp parallel for
	for (int j = 0; j < _height; j++){
		for (int i = 0; i < _width; i++){
			for (int c = 0; c < _channels; c++){
				float g = 0.0f;
				if (i == 0){
					g = get_pixel_value(i + 1, j, c) - get_pixel_value(i, j, c);
				} else if (i == _width - 1){
					g = get_pixel_value(i, j, c) - get_pixel_value(i - 1, j, c);
				} else {
					g = (get_pixel_value(i + 1, j, c) - get_pixel_value(i - 1, j, c)) / 2.0f;
				}
				gradients.set_pixel_value(i, j, c, g);
			}
		}
	}
	return gradients;
}

void ImageData::write_x_gradient_into(ImageData& out_image) const {
	if (out_image._width != _width || out_image._height != _height || out_image._channels != _channels) {
		throw std::invalid_argument("Output image dimensions and channels must match for write_x_gradient_into.");
	}
	#pragma omp parallel for
	for (int j = 0; j < _height; j++){
		for (int i = 0; i < _width; i++){
			for (int c = 0; c < _channels; c++){
				float g = 0.0f;
				if (i == 0){
					g = get_pixel_value(i + 1, j, c) - get_pixel_value(i, j, c);
				} else if (i == _width - 1){
					g = get_pixel_value(i, j, c) - get_pixel_value(i - 1, j, c);
				} else {
					g = (get_pixel_value(i + 1, j, c) - get_pixel_value(i - 1, j, c)) / 2.0f;
				}
				out_image.set_pixel_value(i, j, c, g);
			}
		}
	}
}

void ImageData::write_y_gradient_into(ImageData& output_img) const {
	if (output_img._width != _width || output_img._height != _height || output_img._channels != _channels) {
		throw std::invalid_argument("Output image dimensions and channels must match for write_y_gradient_into.");
	}
	#pragma omp parallel for
	for (int j = 0; j < _height; j++){
		for (int i = 0; i < _width; i++){
			for (int c = 0; c < _channels; c++){
				float g = 0.0f;
				if (j == 0){
					g = get_pixel_value(i, j + 1, c) - get_pixel_value(i, j, c);
				} else if (j == _height - 1){
					g = get_pixel_value(i, j, c) - get_pixel_value(i, j - 1, c);
				} else {
					g = (get_pixel_value(i, j + 1, c) - get_pixel_value(i, j - 1, c)) / 2.0f;
				}
				output_img.set_pixel_value(i, j, c, g);
			}
		}
	}
}


ImageData ImageData::get_y_gradient() const {
	// Return a new ImageData object with same channels:
	ImageData gradients(_width, _height, _channels);
	#pragma omp parallel for
	for (int j = 0; j < _height; j++){
		for (int i = 0; i < _width; i++){
			for (int c = 0; c < _channels; c++){
				float g = 0.0f;
				if (j == 0){
					g = get_pixel_value(i, j + 1, c) - get_pixel_value(i, j, c);
				} else if (j == _height - 1){
					g = get_pixel_value(i, j, c) - get_pixel_value(i, j - 1, c);
				} else {
					g = (get_pixel_value(i, j + 1, c) - get_pixel_value(i, j - 1, c)) / 2.0f;
				}
				gradients.set_pixel_value(i, j, c, g);
			}
		}
	}
	return gradients;
}

void ImageData::clear() {
	_width = 0;
	_height = 0;
	_channels = 0;
	_image_data_ptr.reset();
}

void ImageData::flip() {
	int row_size = _width * _channels;
	
	std::vector<float> temp_row(row_size);

	for (int y = 0; y < _height / 2; ++y) {
		float* top_row    = _image_data_ptr.get() + y * row_size;
		float* bottom_row = _image_data_ptr.get() + (_height - 1 - y) * row_size;

		std::copy(top_row, top_row + row_size, temp_row.data());
		std::copy(bottom_row, bottom_row + row_size, top_row);
		std::copy(temp_row.data(), temp_row.data() + row_size, bottom_row);
	}
}

void ImageData::gl_draw_pixels() const {
	if (_channels == 4){
		glDrawPixels( _width, _height, GL_RGBA, GL_FLOAT, _image_data_ptr.get() );
	}
	else if (_channels == 3){
		glDrawPixels( _width, _height, GL_RGB, GL_FLOAT, _image_data_ptr.get() );
	}
	else if (_channels == 1){
		glDrawPixels( _width, _height, GL_LUMINANCE, GL_FLOAT, _image_data_ptr.get() );
	} else {
		// Default to RGB if channels are unexpected
		glDrawPixels( _width, _height, GL_RGB, GL_FLOAT, _image_data_ptr.get() );
	}
}