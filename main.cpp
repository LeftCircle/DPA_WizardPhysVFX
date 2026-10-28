#include <iostream>
#include <memory>

#include "command_line_parser.h"
#include "level_set_render_cuda.h"
#include "db_perlin.h"
#include "fractal_sum_noise.h"

using namespace lux;

const float PI = 3.14159265358979;

int main(int argc, char** argv){
	
	CommandLineParser parser(argc, argv);
	const std::string file_name = parser.find_val_for_flag("-image");
	if (file_name.empty()){
		std::cerr << "No image file name provided. Use -image <file_name> to make and save the image" << std::endl;
		return 1;
	}

	//render_bunny(file_name, "/bunny_rotated");
	// render_bust(file_name, "/ajax_small");
	//render_bunny_in_ajax(file_name, "/ajax_bun.exr");
	//render_humanoid_with_bunnybust(file_name, "/human");
	//render_bunny_in_ajax(file_name, "/human");
	// cuda_render_bunny(file_name, "/cuda_bunny");
	//cuda_render_tests(file_name);
	//render_pyro_wedges(file_name, "/sphere_pyro");
	// render_wedges(file_name, "/wedges_v02");
	//profile_cuda();
	render_wisp_wedges(file_name, "/wis_wedges_v03");

	// Test out fspn
	// int w = 1920;
	// int h = 1080;
	// float min = 99999;
	// float max = -min;
	// auto img = ImageData(w, h, 3);
	// for (int i = 0; i < w; i++){
	// 	for (int j = 0; j < h; j++){
	// 		auto func = [](const Vector& v){
	// 			return db::perlin<float>(v.x(), v.y());
	// 		};
	// 		// get x/y on -1 to 1 range
	// 		float x = (static_cast<float>(i) - 0.5f * w) / (0.5f * w);
	// 		float y = (static_cast<float>(j) - 0.5f * h) / (0.5f * h);
	// 		float val = fractal_sum<float, Vector>({x, y, 0}, func, 4, 4, {}, 3, 2.14159265);
	// 		// float val = db::perlin(x, y);
	// 		min = std::min(val, min);
	// 		max = std::max(val, max);
	// 		//std::cout << val << std::endl;
	// 		val += 1;
	// 		val /= 2.0;
	// 		img.set_first_three_channels(i, j, val, val, val);
	// 	}
	// }
	// std::cout << "Max = " << max << " min = " << min << std::endl;
	// img.oiio_write_to("images/test_noise.png");


	return 0;
}


