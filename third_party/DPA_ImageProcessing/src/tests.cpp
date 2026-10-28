#include "tests.h"


const char* test_image_path = "/home/leftcircle/programming/clemson/DPA_ComputerGraphicsImages/hw_01_image_reader/images/test_image.jpeg";
const char* test_rgb_1_2_image_path = "/home/leftcircle/programming/clemson/DPA_ComputerGraphicsImages/hw_03_image_dataessing/test_images/rgb_1_2.png";

void test_command_line_parser(){
    const char* argv[] = {"program", "-flag", "value", "-t"};
    const int argc = 4;
    CommandLineParser parser(argc, const_cast<char**>(argv));
    assert(parser.find_val_for_flag("-flag") == "value");
    assert(parser.has_flag("-t"));
}


void test_get_image_length() {
    // Returns the lenght of the data array based on the width, height, and channels.

    int width = 4;
    int height = 4;
    int channels = 3;
    int expected_length = width * height * channels;

    ImageData image_data;
    image_data.set_dimensions(width, height, channels);
    int length = image_data.get_data_len();
    assert(length == expected_length);
    std::cout << "test_get_image_length passed." << std::endl;
}

void test_get_image_index(){
    // Calculates the index in the data array for given (x, y) coordinates.

    int width = 4;
    int height = 4;
    int channels = 3;
    ImageData image_data;
    image_data.set_dimensions(width, height, channels);

    //expected is channel + n_channels * (x + y * width)
    int x = 2;
    int y = 1;
    int c = 2;
    int expected_index = c + channels * (x + y * width);

    int index = image_data.get_index(x, y, c);
    assert(index == expected_index);

    // Now test non interleaved
    expected_index = channels * (x + y * width);
    index = image_data.get_index(x, y);
    assert(index == expected_index);

    std::cout << "test_get_image_index passed." << std::endl;
}

void test_read_image(){
    // Reads an image file and verifies its dimensions and channels.
    ImageData image_data;
    const char* filename = "/home/leftcircle/programming/clemson/DPA_ComputerGraphicsImages/hw_01_image_reader/images/test_image.jpeg";
    image_data.oiio_read(filename);

    // Assuming we know the expected dimensions and channels of the test image
    int expected_width = 3296;  // Replace with actual expected width
    int expected_height = 2472; // Replace with actual expected height
    int expected_channels = 3; // Replace with actual expected channels

    std::cout << "Read image: " << filename << std::endl;
    std::cout << "Image dimensions: " << image_data.get_width() << " x "
              << image_data.get_height() << " x " << image_data.get_channels() << std::endl;

    assert(image_data.get_width() == expected_width);
    assert(image_data.get_height() == expected_height);
    assert(image_data.get_channels() == expected_channels);

    std::cout << "test_read_image passed." << std::endl;
}

void test_write_image(){
    // Writes an image to a file and incremenets the filename. 
    ImageData image_data;
    image_data.oiio_read(test_image_path);
    std::string out_file = image_data.get_output_file_name();
    image_data.oiio_write();

    // confirm that the file exists:
    std::ifstream infile(out_file);
    assert(infile.good());
    infile.close();
}

void test_get_file_type(){
    const char* filename = "image.jpeg";
    const char* filename2 = "image.png";
    const char* filename3 = "image.tiff";

    std::string expected_type1 = "jpeg";
    std::string expected_type2 = "png";
    std::string expected_type3 = "tiff";

    std::string actual_type1 = StringFuncs::get_file_type(filename);
    std::string actual_type2 = StringFuncs::get_file_type(filename2);
    std::string actual_type3 = StringFuncs::get_file_type(filename3);

    assert(actual_type1 == expected_type1);
    assert(actual_type2 == expected_type2);
    assert(actual_type3 == expected_type3);
    std::cout << "test_get_file_type passed." << std::endl;
}

void test_get_file_name(){
    const char* filepath = "/path/to/image.jpeg";
    const char* filepath2 = "image.png";
    const char* filepath3 = "/another/path/image.tiff";

    std::string expected_name1 = "image";
    std::string expected_name2 = "image";
    std::string expected_name3 = "image";

    std::string actual_name1 = StringFuncs::get_file_name(filepath);
    std::string actual_name2 = StringFuncs::get_file_name(filepath2);
    std::string actual_name3 = StringFuncs::get_file_name(filepath3);

    assert(actual_name1 == expected_name1);
    assert(actual_name2 == expected_name2);
    assert(actual_name3 == expected_name3);
    std::cout << "test_get_file_name passed." << std::endl;
}

void test_image_editor_initialization(){
    ImageData image_data;
    image_data.oiio_read(test_rgb_1_2_image_path);
    ImageEditor editor(image_data);

    // Confirm that the pointers to the image and edited image are not null
    assert(editor.get_starting_image() != nullptr);
    assert(editor.get_edited_image() != nullptr);

    std::cout << "test_image_editor_initialization passed." << std::endl;

}

// void test_test_image(){
//     // The test image is a 10x10 image where the first pixels are
//     // (255, 0, 0), (0, 255, 0), (0, 0, 255), (1, 1, 1), (2, 2, 2),
//     ImageData image_data;
//     image_data.oiio_read(test_rgb_1_2_image_path);
//     std::vector<float> expected_pixel1 = {255.0f, 0.0f, 0.0f};
//     std::vector<float> expected_pixel2 = {0.0f, 255.0f, 0.0f};
//     std::vector<float> expected_pixel3 = {0.0f, 0.0f, 255.0f};
//     std::vector<float> expected_pixel4 = {1.0f, 1.0f, 1.0f};
//     std::vector<float> expected_pixel5 = {2.0f, 2.0f, 2.0f};
//     std::vector<float> actual_pixel1 = image_data.get_pixel_values(0, 0);
//     std::vector<float> actual_pixel2 = image_data.get_pixel_values(1, 0);
//     std::vector<float> actual_pixel3 = image_data.get_pixel_values(2, 0);
//     std::vector<float> actual_pixel4 = image_data.get_pixel_values(3, 0);
//     std::vector<float> actual_pixel5 = image_data.get_pixel_values(4, 0);
//     assert(actual_pixel1 == expected_pixel1);
//     assert(actual_pixel2 == expected_pixel2);
//     assert(actual_pixel3 == expected_pixel3);
//     assert(actual_pixel4 == expected_pixel4);
//     assert(actual_pixel5 == expected_pixel5);
//     std::cout << "test_test_image passed." << std::endl;
    
// }

void test_lut() {
    LUT<Color> color_lut{Color(0.0f), Color(1.0f)};
    Color return_val = color_lut.lerp(0.5);
    assert(return_val == Color(0.5));
    std::cout << "LUT passed!" << std::endl;
}


void Tests::run_tests() {
    // test_get_image_length();
    // test_command_line_parser();
    // test_get_image_index();
    // test_read_image();
    // test_get_file_name();
    // test_get_file_type();
    // test_write_image();
    // // //test_image_editor_initialization();
    // test_lut();
    //std::cout << "All tests passed!" << std::endl;
}

int main(int argc, char** argv){
    Tests tests;
    tests.run_tests();
}