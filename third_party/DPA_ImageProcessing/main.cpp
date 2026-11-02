#include <OpenImageIO/imageio.h>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "command_line_parser.h"
#include "image_data.h"
#include "controller.h"
#include "view.h"

namespace {
std::string watched_image;
std::filesystem::file_time_type watched_time;
bool watched_time_valid = false;
unsigned unchanged_polls = 0;

// The preview writer atomically renames complete images into place. Read on
// the GLUT thread so replacing the model never races display or keyboard code.
void poll_image(int) {
    std::error_code error;
    const auto current = std::filesystem::last_write_time(watched_image, error);
    // Also refresh occasionally if a filesystem has coarse timestamp resolution.
    if (!error && (!watched_time_valid || current != watched_time || ++unchanged_polls >= 20)) {
        ImageData updated;
        updated.oiio_read(watched_image.c_str());
        if (updated.get_width() > 0 && updated.get_height() > 0 &&
            (updated.get_channels() == 1 || updated.get_channels() == 3 || updated.get_channels() == 4)) {
            Model::instance()->image_editor.set_edited_image_to(updated);
            watched_time = current;
            watched_time_valid = true;
            unchanged_polls = 0;
            glutPostRedisplay();
        }
    }
    glutTimerFunc(250, poll_image, 0);
}
}

int main(int argc, char** argv)
{
    CommandLineParser parser(argc, argv);
    const std::string file_name = parser.find_val_for_flag("-image");
    if (file_name.empty()) {
        std::cerr << "No image file provided. Use -image <file_name> [--watch]" << std::endl;
        return 1;
    }
    const bool watch = parser.has_flag("--watch");
    if (watch) {
        watched_image = file_name;
        std::error_code error;
        watched_time = std::filesystem::last_write_time(watched_image, error);
        watched_time_valid = !error;
    }
    ImageData img;
    img.oiio_read(file_name.c_str());
    if (img.get_width() <= 0 || img.get_height() <= 0) {
        std::cerr << "Could not read image: " << file_name << std::endl;
        return 1;
    }
    View* view = View::instance();
    Controller* controller = Controller::instance();
    Model* model = create_model(img);
    (void)controller;
    (void)model;

    // Keep our application-only watch flag away from GLUT's argument parser.
    std::vector<char*> glut_args;
    for (int i = 0; i < argc; ++i)
        if (std::string(argv[i]) != "--watch") glut_args.push_back(argv[i]);
    const int glut_argc = static_cast<int>(glut_args.size());
    glut_args.push_back(nullptr);
    view->init(glut_argc, glut_args.data(), img.get_width(), img.get_height());
    if (watch) glutTimerFunc(250, poll_image, 0);
    view->display();
    view->main_loop();
    return 0;
}
