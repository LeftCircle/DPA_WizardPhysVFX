#include "file_utils.h"

//using namespace FileUtils;

std::vector<std::string> get_all_files_starting_with(const std::string& directory, const std::string& prefix){
    std::vector<std::string> matching_files;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (filename.rfind(prefix, 0) == 0) {
                matching_files.push_back(entry.path().string());
            }
        }
    }
    return matching_files;
}

std::string get_file_name(const std::string& filepath){
    size_t slash_pos = filepath.find_last_of("/\\");
    size_t dot_pos = filepath.find_last_of('.');
    if (dot_pos == std::string::npos || dot_pos == 0) {
        return ""; // No extension found or empty filename
    }
    size_t start = (slash_pos == std::string::npos) ? 0 : slash_pos + 1;
    return filepath.substr(start, dot_pos - start);
}

int get_file_number_suffix(const std::string& filename){
    size_t i = filename.length() - 1;
    while (i > 0 && isdigit(filename[i])) {
        --i;
    } 
    if (i == filename.length() - 1) {
        std::cout << "No trailing number found in filename: " << filename << std::endl;
        return -1; // No trailing number
    }
    return std::stoi(filename.substr(i + 1));
}

void sort_based_on_number_suffix(std::vector<std::string>& file_names, bool remove_extension){
    
    std::sort(file_names.begin(), file_names.end(),
        [remove_extension](const std::string& a, const std::string& b) {
            std::string name_a = a;
            std::string name_b = b;
            if (remove_extension){
                name_a = get_file_name(a);
                name_b = get_file_name(b);
            }
            int num_a = get_file_number_suffix(name_a);
            int num_b = get_file_number_suffix(name_b);
            return num_a < num_b;
        });
}