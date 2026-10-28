#include "controller.h"


Controller* Controller::pController = nullptr;

Controller* create_controller()
{
	Controller* ctrl = Controller::instance();
	return ctrl;
}

Controller::Controller() {}

Controller::~Controller() {}

void Controller::keyboard( unsigned char key, int x, int y )
{
	Model* model = Model::instance();
	ImageEditor _image_editor = Model::instance()->image_editor;
	const std::shared_ptr<ImageData> edited_image = _image_editor.get_edited_image();
	switch (key)
	{
		case 27: // esc
			exit(0);
			break;
		case 'a':{
			std::cout << "Starting ensemble average ..." << std::endl;
			ImageData avg_img = ImageDataModifier::ensemble_average(*_image_editor.get_edited_image(), 1);
			_image_editor.set_edited_image_to(avg_img);
			std::cout << "Ensemble average done!" << std::endl;
			break;
		}
		case 'b':{
			std::cout << "Applying bilinear interpolation to each color channel ..." << std::endl;
			ImageDataModifier::bilinear_interpolate_each_channel(*edited_image);
			std::cout << "Bilinear interpolation done!" << std::endl;
			break;
		}
		case 'c':{
			model->convert_to_contrast_units();
			break;
			}
		case 'C':{
			model->convert_to_contrast_units();
			break;
		}
		case 'd':{
			std::cout << "Starting downscale ..." << std::endl;
			//int new_width = _image_editor.get_edited_image()->get_width() / 1.25;
			//int new_height = _image_editor.get_edited_image()->get_height() / 1.25;
			int new_width = 64;
			int new_height = 48;
			new_width = std::max(new_width, 6);
			new_height = std::max(new_height, 8);
			//std::cout << "New width = " << new_width << std::endl;
			//_image_editor.downscale(new_width, new_height);
			ImageDataModifier::downscale(*edited_image, new_width, new_height, false);
			std::cout << "Downscale complete!" << std::endl;
			break;
		}
		case 'f':
			std::cout << "f key pressed! Should flip" << std::endl;
			edited_image->flip();
			break;
		case 'F':
			std::cout << "Fractal Flame!" << std::endl;
			model->fractal_flames();
			std::cout << "Done" << std::endl; 
			break;
		case 'g':
			std::cout << "gamma of 0.9" << std::endl;
			ImageDataModifier::gamma_filter(*edited_image, 0.9f);
			break;
		case 'G':
			std::cout << "gamma of 1.1111" << std::endl;
			ImageDataModifier::gamma_filter(*edited_image, 1.0f + 1.0f / 9.0f);
			break;
		case 'h':{
			ImageDataModifier::histogram_equalize(*edited_image, 500, false);
			break;
		}
		case 'H':{
			ImageDataModifier::histogram_equalize(*edited_image, 500, true);
			break;
		}
		case 'J': {
			//std::cout << "Applying Julia fractal!" << std::endl;
			//std::pair<int, double> iters_and_range = _get_julia_set_paramters();
			//model->apply_julia_set(iters_and_range.first, iters_and_range.second);
			//std::cout << "Julia fractal finished!" << std::endl;
			model->on_J_pressed();
			break;
		}
		case 'j':{
			std::cout << "Key j pressed" << std::endl;
			_image_editor.save_edited_image();
			break;
		}
		case 'm':{
			// Palette match
			std::cout << "Starting palette match ..." << std::endl;
			
			ImageDataModifier::palette_match(*edited_image, colors);
			std::cout << "Palette match done!" << std::endl;
			break;
		}
		case 'o':{
			// Output the image as .exr
			_image_editor.save_edited_image(".exr");
			break;
		}
		case 'O':{
			// Optical flow
			std::cout << "Starting optical flow ..." << std::endl;
			
			// Start by prompting the user for a path to a directory of images
			std::string dir_path;
			std::cout << "Enter the path to the directory of images: " << std::flush;
			std::cin >> dir_path;
			
			// Now ask for the name of the image sequence
			std::string img_sequence_name;
			std::cout << "Enter the base name of the image sequence (without numbering or extension): " << std::flush;
			std::cin >> img_sequence_name;
			// Now use file Utils to get the names of the images in the directory
			std::vector<std::string> image_file_names = get_all_files_starting_with(dir_path, img_sequence_name);
			sort_based_on_number_suffix(image_file_names, true);
			
			// Now ask for an output directory
			std::string output_dir;
			std::cout << "Enter the output directory for flowed images (or leave blank to not save): " << std::flush;
			std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // clear the input buffer
			std::getline(std::cin, output_dir);

			// Ask for movie or single image
			std::cout << "Apply to target image or to video sequence or retime (i/v/r): " << std::flush;
			char choice;
			// Loop until i or v is entered
			while (choice != 'i' && choice != 'v' && choice != 'r') {
				std::cin >> choice;
				if (choice != 'i' && choice != 'v' && choice != 'r') {
					std::cout << "Invalid choice. Please enter 'i' for image or 'v' for video: " << std::flush;
				}
			}
			if (choice == 'v'){
				_image_editor.optical_flow_video(image_file_names, 4, output_dir, 5, true, false);
				std::cout << "Optical flow video done!" << std::endl;
			} else if (choice == 'i'){
				_image_editor.optical_flow(image_file_names, *_image_editor.get_edited_image(), output_dir, 1);
				std::cout << "Optical flow done!" << std::endl;
			} else if (choice == 'r'){
				// ask for a fps and a new duration
				int fps;
				float duration;
				std::cout << "Enter the fps of the original video: " << std::flush;
				std::cin >> fps;
				std::cout << "Enter the target duration in seconds: " << std::flush;
				std::cin >> duration;
				_image_editor.extend_video_duration_to(image_file_names, fps, duration, output_dir, 5);
				std::cout << "Optical flow retiming done!" << std::endl;
			} else {
				std::cout << "Invalid choice, skipping optical flow." << std::endl;
			}
			// Kill the program when it is done
			exit(0);
			break;
		}
		case 'p':{
			// If the width/height of the image is less than 20x20, print out
			// the pixel values

			// Create a map of colors to index in color vector
			auto color_hash = [](const std::vector<float>& color) {
			size_t h1 = std::hash<float>{}(color[0]);
			size_t h2 = std::hash<float>{}(color[1]);
			size_t h3 = std::hash<float>{}(color[2]);
			return h1 ^ (h2 << 1) ^ (h3 << 2);
			};

			std::unordered_map<std::vector<float>, int, decltype(color_hash)> color_map(10, color_hash);
			int n_colors = static_cast<int>(colors.size() / 3);
			for (int i = 0; i < n_colors; i++){
				std::vector<float> color = {colors[i*3], colors[i*3+1], colors[i*3+2]};
				color_map[color] = i;
			}


			if (_image_editor.get_edited_image()->get_width() <= 20 &&
				_image_editor.get_edited_image()->get_height() <= 20){
				std::cout << "Image pixel values:" << std::endl;
				for (int j = 0; j < _image_editor.get_edited_image()->get_height(); j++){
					for (int i = 0; i < _image_editor.get_edited_image()->get_width(); i++){
						auto pixels = _image_editor.get_edited_image()->get_pixel_values(i, j);
						std::cout << "(";
						// print the rgb values based on the color map
						std::vector<float> color = {pixels[0], pixels[1], pixels[2]};
						
						if (color_map.find(color) != color_map.end()){
							std::cout << color_map[color];
						} else{
							std::cout << "ERROR";
						}
						std::cout << ") ";
					}
					std::cout << std::endl;
				}
			} else{
				std::cout << "Image too large to print pixel values" << std::endl;
			}

			break;
		}
		case 's':{
			_apply_stencil();
			break;
		}
		case 'q':{
			ImageDataModifier::quantize(*edited_image, 5);
			std::cout << "Quanitze is done!" << std::endl;
			break;
		}
	}
}

void Controller::special_keys(int key, int x, int y){
	Model* model = Model::instance();
	switch (key){
		case GLUT_KEY_UP:
			std::cout << "Up arrow pressed" << std::endl;
			model->on_up_arrow_pressed();
			break;
		case GLUT_KEY_DOWN:
			std::cout << "Down arrow pressed" << std::endl;
			model->on_down_arrow_pressed();
			break;
		case GLUT_KEY_RIGHT:
			std::cout << "Right arrow pressed" << std::endl;
			model->on_right_arrow_pressed();
			break;
		case GLUT_KEY_LEFT:
			std::cout << "Left arrow pressed" << std::endl;	
			model->on_left_arrow_pressed();
			break;
	}
}

std::pair<int, double> Controller::_get_julia_set_paramters(){
	int iters;
	double range;

	std::cout << "Enter the number of iterations: (int) " << std::flush;
	if (!(std::cin >> iters)){
		std::cin.clear();
		std::string discard;
		std::getline(std::cin, discard);
		std::cout << "Invalid parameters. Just using defaults" << "std::endl";
		return {100, 1.0};
	}
	std:: cout << "Enter a range can be of format ie 1.0 or 1.0e-6: (double)" << std::flush;
	if (!(std::cin >> range)) {
		std::cin.clear();
		std::string discard;
		std::getline(std::cin, discard);
		std::cout << "Invalid parameters. Just using defaults" << "std::endl";
		return {100, 1.0};
	}
	return {iters, range};
}

void Controller::_apply_stencil() {
	ImageEditor _image_editor = Model::instance()->image_editor;
	std::shared_ptr<ImageData> edited_image = _image_editor.get_edited_image();
	std::cout << "Applying stencil" << std::endl;
			
	Model::instance()->stencil.randomize_values();
	
	ImageDataModifier::bounded_linear_convolution(*edited_image, Model::instance()->stencil);
	std::cout << "Stencil applied" << std::endl;
}