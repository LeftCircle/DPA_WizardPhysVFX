#include "model.h"

Model* Model::pModel = nullptr;

Model::Model()
	: stencil(DEFAULT_STENCIL_HALF_WIDTH) {}

Model::~Model() {}

Model* create_model(ImageData& image_data) {
	Model* model = Model::instance();
	model->image_editor = ImageEditor(image_data);
	return model;
}

void Model::on_J_pressed() {
	_current_state = JULIA_SET;
	_julia_iterations = DEFAULT_JULIA_ITERATIONS;
	_julia_range = DEFAULT_JULIA_RANGE;
	std::cout << "Applying Julia Set with " << _julia_iterations << " iterations and range " << _julia_range << std::endl;
	std::cout << " Use the up and down arrow keys to adjust the zoom. The left and right arrow keys to adjust iterations." << std::endl;
	apply_julia_set(_julia_iterations, _julia_range);
}

void Model::on_right_arrow_pressed() {
	// Increase iterations
	_julia_iterations += JULIA_ITERATION_STEP;
	std::cout << "Increasing Julia Set iterations to " << _julia_iterations << std::endl;
	apply_julia_set(_julia_iterations, _julia_range);
}

void Model::on_left_arrow_pressed() {
	// Decrease iterations
	_julia_iterations = std::max(JULIA_ITERATION_STEP, _julia_iterations - JULIA_ITERATION_STEP);
	std::cout << "Decreasing Julia Set iterations to " << _julia_iterations << std::endl;
	apply_julia_set(_julia_iterations, _julia_range);
}

void Model::on_up_arrow_pressed() {
	// Decrease range
	_julia_range /= RANGE_SCALE_FACTOR;
	std::cout << "Decreasing Julia Set range to " << _julia_range << std::endl;
	apply_julia_set(_julia_iterations, _julia_range);
}

void Model::on_down_arrow_pressed() {
	// Increase range
	_julia_range *= RANGE_SCALE_FACTOR;
	std::cout << "Increasing Julia Set range to " << _julia_range << std::endl;
	apply_julia_set(_julia_iterations, _julia_range);
}


void Model::convert_to_contrast_units() {
	
	ImageDataModifier::convert_to_contrast_units(*image_editor.get_edited_image());
}

void Model::fractal_flames() {
	// randomly seed drand48
	srand48(static_cast<unsigned int>(time(nullptr)));

	Spherical ifs_spherical(0.0, 0.0, 1.0, 1.0);
	Spherical ifs_spherical2(-0.5, 0.0, 0.1, 0.1);
	Spherical ifs_spherical3(0.0, 0.5, 0.5, 0.5);
	Spherical ifs_spherical4(0.0, -0.5, 0.3, 0.3);
	Sinusoidal ifs_sin;
	Sinusoidal ifs_sin_1;
	

	Linear ifs_linear(1.0, 1.0);
	Linear ifs_linear_grow_x(1.1, 1.0);
	Linear ifs_linear_grow_y(1.0, 1.1);
	Linear ifs_linear_shrink_x(1.0 / 1.1, 1.0);
	Linear ifs_linear_shrink_y(1.0, 1.0 / 1.1);
	Truncate ifs_truncate;

	negate_x neg_x;
	Rotation flip(PI);
	Rotation small_rot(PI / 75.0);
	Rotation three_way_symmetry_0(2.0 * PI / 3.0);
	Rotation three_way_symmetry_1(4.0 * PI / 3.0);
	Rotation three_way_symmetry_2(0.0);


	
	Randomize final_rand;
	Linear final_linear(1.0, 1.0);

	std::vector<FlameIFSFunction*> ifs_functions = {};


	std::vector<float> ifs_weights = {
		//1.0,
		//1.0,
		// 1.0,
		//1.0
	};

	// Color palette 258
	std::vector<Color> colors = {
		// Color(1.0f, 175.0f, 186.0f) / 255.0f,
		// Color(204.0f, 171.0f, 214.0f) / 255.0f,
		//Color(242.0f, 251.0f, 122.0f) / 255.0f,
		//Color(0.0f, 251.0f, 122.0f) / 255.0f,
		//Color(0.0f, 129.0f, 227.0f) / 255.0f,
		//Color(1.0f, 98.0f, 115.0f) / 255.0f
	};
	
	// Creating a ton of different sin function
	std::vector<std::shared_ptr<FlameIFSFunction>> generated_funcs;
	int n_funcs = 30;
	std::pair<double, double> x_scale_min_max(0.25, 10.0);
	std::pair<double, double> y_scale_min_max(0.25, 10.0);
	std::pair<double, double> x_y_skew_min_max(0.0, 1.0);
	std::pair<double, double> x_y_offset_min_max(-0.9, 0.9);

	std::vector<Color> color_pallette = {
		Color(1.0f, 175.0f, 186.0f) / 255.0f,
		Color(204.0f, 171.0f, 214.0f) / 255.0f,
		Color(242.0f, 251.0f, 122.0f) / 255.0f,
		Color(0.0f, 251.0f, 122.0f) / 255.0f,
		Color(0.0f, 129.0f, 227.0f) / 255.0f,
		Color(1.0f, 98.0f, 115.0f) / 255.0f
	};
	
	for (int i = 0; i < n_funcs; i++){
		std::shared_ptr<FlameIFSFunction> func_a = std::make_shared<Spherical>(0.0, 0.0, 1.0, 1.0);
		std::shared_ptr<FlameIFSFunction> func_b = std::make_shared<Handkerchief>();
		double rand_choice = drand48();
		std::shared_ptr<FlameIFSFunction> func = rand_choice < 0.5 ? func_a : func_b;
		float weight = rand_choice < 0.5 ? 5.0f : 1.0f;
		double rand_x_scale = x_scale_min_max.first + drand48() * (x_scale_min_max.second - x_scale_min_max.first);
		double rand_y_scale = y_scale_min_max.first + drand48() * (y_scale_min_max.second - y_scale_min_max.first);
		//double rand_y_scale = rand_x_scale; // keep aspect ratio for now
		double rand_x_skew = x_y_skew_min_max.first + drand48() * (x_y_skew_min_max.second - x_y_skew_min_max.first);
		double rand_y_skew = x_y_skew_min_max.first + drand48() * (x_y_skew_min_max.second - x_y_skew_min_max.first);
		double rand_x_offset = x_y_offset_min_max.first + drand48() * (x_y_offset_min_max.second - x_y_offset_min_max.first);
		double rand_y_offset = x_y_offset_min_max.first + drand48() * (x_y_offset_min_max.second - x_y_offset_min_max.first);

		// account for the scale now in the rand offset and skew
		rand_x_offset *= (rand_x_scale + rand_y_scale) / 2;
		rand_y_offset *= (rand_x_scale + rand_y_scale) / 2;
		rand_x_skew *= (rand_x_skew + rand_x_skew) / 2;
		rand_y_skew *= (rand_x_skew + rand_y_skew) / 2;

		generated_funcs.push_back(func);
		ifs_functions.push_back(func.get());
		ifs_weights.push_back(weight);
		colors.push_back(color_pallette[i % color_pallette.size()]);
	}

	// ifs_functions.push_back(&final_rand);
	// ifs_weights.push_back(0.0);
	// colors.push_back(color_pallette[0]);


	std::vector<SymmetryIFS*> rotation_functions = {
		// &neg_x,
		// &three_way_symmetry_0,
		// &three_way_symmetry_1,
		// &three_way_symmetry_2
	};

	std::vector<float> rotation_weights = {
		// 1.0,
		// 1.0,
		// 1.0,
		// 1.0
	};


	IFSFunctionSystem ff_system(
		ifs_functions,
		ifs_weights,
		colors,
		rotation_functions,
		rotation_weights,
		&final_linear,
		image_editor.get_edited_image()->get_width(),
		image_editor.get_edited_image()->get_height()
	);

	int iters = 100000000;

	ff_system.fractal_frame(iters);

	image_editor.set_edited_image_to(ff_system.get_image());
}

void Model::apply_julia_set(const int iterations, const double range){

	// Initial parameters are fixed at the moment
	Point center(DEFAULT_CENTER);
	Point complex_center(DEFAULT_COMPLEX_CENTER);

	JuliaIterations jul(complex_center, iterations, 2);
	LUT<Color> color_lut{
		Color(1.0f, 175.0f, 186.0f) / 255.0f,
		Color(204.0f, 171.0f, 214.0f) / 255.0f,
		Color(242.0f, 251.0f, 122.0f) / 255.0f,
		Color(0.0f, 251.0f, 122.0f) / 255.0f,
		Color(0.0f, 1298.0f, 227.0f) / 255.0f
		//Color(1.0f, 98.0f, 115.0f) / 255.0f
	};
	ImageDataModifier::julia_set(*image_editor.get_edited_image(), center, range, jul, color_lut);
}