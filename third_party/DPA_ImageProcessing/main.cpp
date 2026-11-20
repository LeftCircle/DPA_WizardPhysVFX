#include <OpenImageIO/imageio.h>
#include <cstddef>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "command_line_parser.h"
#include "image_data.h"
#include "image_data_modifier.h"
#include "controller.h"
#include "view.h"

namespace {
std::string watched_image;
std::filesystem::file_time_type watched_time;
bool watched_time_valid = false;
unsigned unchanged_polls = 0;
float display_gamma = 1.0f;

// Apply the existing gamma filter to a display copy, preserving alpha.
// Negative RGB values have no real fractional power and display as black.
void adjust_display_gamma(ImageData& image) {
    if (display_gamma == 1.0f) return;
    const int width = image.get_width(), height = image.get_height();
    const int channels = image.get_channels();
    std::vector<float> alpha;
    if (channels == 4) alpha.resize(static_cast<size_t>(width) * height);
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        if (channels == 4) alpha[static_cast<size_t>(y) * width + x] = image.get_pixel_value(x, y, 3);
        for (int c = 0; c < (channels == 4 ? 3 : channels); ++c)
            if (image.get_pixel_value(x, y, c) < 0) image.set_pixel_value(x, y, c, 0);
    }
    ImageDataModifier::gamma_filter(image, display_gamma);
    if (channels == 4)
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
            image.set_pixel_value(x, y, 3, alpha[static_cast<size_t>(y) * width + x]);
}

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
            adjust_display_gamma(updated);
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
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--display-gamma") {
            try {
                if (++i >= argc) throw std::invalid_argument("missing gamma");
                size_t consumed = 0;
                const std::string value(argv[i]);
                display_gamma = std::stof(value, &consumed);
                if (consumed != value.size() || !std::isfinite(display_gamma) || display_gamma <= 0)
                    throw std::invalid_argument("invalid gamma");
            } catch (const std::exception&) {
                std::cerr << "--display-gamma requires a finite positive number" << std::endl;
                return 1;
            }
        }
    }
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
    adjust_display_gamma(img);
    View* view = View::instance();
    Controller* controller = Controller::instance();
    Model* model = create_model(img);
    (void)controller;
    (void)model;

    // Keep our application-only watch flag away from GLUT's argument parser.
    std::vector<char*> glut_args;
    for (int i = 0; i < argc; ++i) {
        if (std::string(argv[i]) == "--watch") continue;
        if (std::string(argv[i]) == "--display-gamma") { ++i; continue; }
        glut_args.push_back(argv[i]);
    }
    const int glut_argc = static_cast<int>(glut_args.size());
    glut_args.push_back(nullptr);
    view->init(glut_argc, glut_args.data(), img.get_width(), img.get_height());
    if (watch) glutTimerFunc(250, poll_image, 0);
    view->display();
    view->main_loop();
    return 0;
}
