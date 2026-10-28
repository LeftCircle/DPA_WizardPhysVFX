#include "IFSFunction.h"

Point Linear::operator()(const Point& P) const {
	return P * _scale;
}

Point Spherical::operator()(const Point& P) const {
	Point shifted_p = P;
	shifted_p -= _center;
	shifted_p *= _scale;
	double rsq = shifted_p.magnitude_sq();
	if (abs(rsq) < EPSILON){
		//std::cout << "Epsilon" << std::endl;
		return P;
	}
	shifted_p /= rsq;
	shifted_p += _center;
	return shifted_p;
}

Point Handkerchief::operator()(const Point& P) const {
	Point new_p(0.0, 0.0);
	double r = P.magnitude();
	double theta = atan2(P.y, P.x);
	new_p.x = r * sin(theta + r);
	new_p.y = r * cos(theta - r);
	return new_p;
}

JuliaIterations::JuliaIterations(const Point& complex_center, int iters, int cycles){
	 _iterations = iters, _cycles = std::max(cycles, 2);
	 _complex_center.x = complex_center.x;
	 _complex_center.y = complex_center.y; 
}

Point JuliaIterations::operator()(const Point& P) const {
	std::complex<double> Pc(P.x, P.y);
	std::complex<double> cc(_complex_center.x, _complex_center.y);

	for (int i = 0; i < _iterations; i++){
		std::complex<double> temp = Pc;
		for (int c = 1; c < _cycles; c++){
			temp = temp * Pc;
		}
		Pc = temp + cc;
	}

	Point p_out(Pc.real(), Pc.imag());
	return p_out;
}

Rotation::Rotation(float radians){
	_radians = radians;
	float cos_r = cos(radians);
	float sin_r = sin(radians);
	_rotation_matrix_2D[0] = cos_r;
	_rotation_matrix_2D[1] = -sin_r;
	_rotation_matrix_2D[2] = sin_r;
	_rotation_matrix_2D[3] = cos_r;
}

Point Rotation::operator()(const Point& P) const {
	// Rotates the point by _radians.
	Point new_p(0.0, 0.0);
	new_p.x = _rotation_matrix_2D[0] * P.x + _rotation_matrix_2D[1] * P.y;
	new_p.y = _rotation_matrix_2D[2] * P.x + _rotation_matrix_2D[3] * P.y;
	return new_p;
}

Point negate_x::operator()(const Point& P) const {
	Point new_p(-P.x, P.y);
	return new_p;
}


Point Sinusoidal::operator()(const Point& P) const {
	Point new_p(0.0, 0.0);
	new_p.x = sin(P.x);
	new_p.y = sin(P.y);
	return new_p;
}

IFSFunctionSystem::IFSFunctionSystem(
		const std::vector<FlameIFSFunction*>& functions,
		std::vector<float>& weights,
		const std::vector<Color>& colors,
		const std::vector<SymmetryIFS*>& symmetry_functions,
		std::vector<float>& symmetry_weights,
		FlameIFSFunction* final_function,
		int width,
		int height
	) {
	// Set the funcs and colors, normalize the weights, and move on!
	_ifs_functions = functions;
	_colors = colors;
	_weights = weights;
	_symmetry_functions = symmetry_functions;
	_symmetry_weights = symmetry_weights;
	_final_function = final_function;
	bool _inputs_correct = _are_inputs_correct();
	if (!_inputs_correct){
		throw std::invalid_argument("IFSFunctionSystem inputs are not correct. See error messages above.");
	}

	normalize(_symmetry_weights);
	normalize(_weights);
	img.set_dimensions(width, height, N_CHANNELS);
}

void IFSFunctionSystem::normalize(std::vector<float>& vec){
	float sum = 0.0;
	float n_elements = vec.size();
	for (int i = 0; i < n_elements; i++){
		sum += vec[i];
	}
	float one_over_sum = 1.0 / sum;
	for (int i = 0; i < n_elements; i++){
		vec[i] *= one_over_sum;
	}
}

int IFSFunctionSystem::get_random_weighted_index(const std::vector<float>& weights){
	float randf = drand48();
	float weight_sum = 0.0f;
	for (size_t i = 0; i < weights.size(); i++){
		weight_sum += weights[i];
		if (randf <= weight_sum){
			return i;
		}
	}

	std::cout << "Random weight somehow didn't get picked. Should not happen" << std::endl;
	return static_cast<int>(weights.size() - 1);
}


void IFSFunctionSystem::fractal_frame(int iters){
	img.set_pixel_values(0.0f);
	Point p(double(2 * drand48() - 1), double(2 * drand48() - 1));
	int width = img.get_width();
	int height = img.get_height();
	//std::vector<float> rgb(3, 0.0f);

	// Alpha channel will contain hit count for now
	int rand_index;
	FlameIFSFunction* ifs;
	SymmetryIFS* sym;
	float itersf = static_cast<float>(iters);
	bool has_sym = has_symmetry();

	for (int i = 1; i <= iters; i++){
		// Roll for symmetry func
		if (drand48() < 0.5 && has_sym){
			// symmetry!
			rand_index = get_random_weighted_index(_symmetry_weights);
			sym = get_symmetry_function(rand_index);
			p = (*sym)(p);
		}
		// Update p and also adjust the color
		rand_index = get_random_weighted_index(_weights);
		ifs = get_ifs_function(rand_index);
		const Color& c = get_color(rand_index);
		
		// First apply the affine transform from the FlameIFSFunction
		p = (*ifs)(p);
		
		// Now add values to the img
		int xp = int(((p.x + 1.0f) / 2.0f) * width);
		int yp = int(((p.y + 1.0f) / 2.0f) * height);
		p = (*_final_function)(p);
		if (xp < 0 || xp >= width || yp < 0 || yp >= height){
			continue;
		} else {
			// add values to the pixel
			img.mix_rgb_values(xp, yp, c.r, c.g, c.b);
			img.add_value(xp, yp, 3, (float)i / itersf);
		}
	}
	// Now we have to do our post processing and adjust the color and alpha parameters
	// based off of log(alpha) / alpha
	#pragma omp parallel for
	for (int j = 0; j < img.get_height(); j++){
		for (int i = 0; i < img.get_width(); i++){
			float a = img.get_pixel_value(i, j, 3);
			if (a == 0){
				continue;
			}
			float loga_over_a = log(a) / a;
			//std::cout << "log a over a = " << loga_over_a << " a = " << a << std::endl;
			img.scale_pixel_values(i, j, loga_over_a);
		}
	}
}

bool IFSFunctionSystem::_are_inputs_correct() const {
	bool correct = true;
	if (_ifs_functions.size() != _weights.size()) {
		std::cerr << "Error: Number of IFS functions does not match number of weights." << std::endl;
		correct = false;
	}
	if (_ifs_functions.size() != _colors.size()) {
		std::cerr << "Error: Number of IFS functions does not match number of colors." << std::endl;
		correct = false;
	}
	if (_symmetry_functions.size() != _symmetry_weights.size()) {
		std::cerr << "Error: Number of symmetry functions does not match number of symmetry weights." << std::endl;
		correct = false;
	}
	return correct;
}