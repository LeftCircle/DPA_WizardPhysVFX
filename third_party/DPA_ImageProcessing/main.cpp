#include <OpenImageIO/imageio.h>
#include <cstddef>
#include <memory>

#include "command_line_parser.h"
#include "image_data.h"
#include "controller.h"
#include "view.h"


int main(int argc, char** argv)
{
	CommandLineParser parser(argc, argv);

	const std::string file_name = parser.find_val_for_flag("-image");
	if (file_name.empty()){
		std::cerr << "No image file provided. Use -image <file_name>" << std::endl;
		return 1;
	}
	
	ImageData img;
	img.oiio_read(file_name.c_str());
	View* view = View::instance();
	Controller* controller = Controller::instance();
	Model* model = create_model(img);

	// To get rid of compiler warnings:
	(void)controller;
	(void)model;
	//---------
	
	view->init(argc, argv, img.get_width(), img.get_height());
	view->display();

	view->main_loop();
	return 0;
}