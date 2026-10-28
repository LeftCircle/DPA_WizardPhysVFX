#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <iostream>

/**
 * @brief Gets all files in a directory that start with a given prefix
 * 
 * @param directory The directory path to search
 * @param prefix The prefix string to match at the start of filenames
 * @return std::vector<std::string> Vector of matching file paths
 * NOTE: the returned file paths are not sorted and contain the file extensions
 */
std::vector<std::string> get_all_files_starting_with(const std::string& directory, const std::string& prefix);

/**
 * @brief Sorts file names based on numeric suffix in the filename
 * 
 * @param file_names Vector of filenames to sort (modified in-place)
 * @param remove_extension Whether to remove file extensions during the sorting, defaults to false.
 * NOTE -> the file extensions will remain regardless of this parameter
 */
void sort_based_on_number_suffix(std::vector<std::string>& file_names, bool remove_extension = false);

/**
 * @brief Extracts the filename from a full file path
 * 
 * Given path/to/file_name.extension, returns file_name
 * 
 * @param filepath Full path to the file
 * @return std::string The filename without path or extension
 */
std::string get_file_name(const std::string& filepath);

/**
 * @brief Extracts the numeric suffix from a filename
 * 
 * @param filename The filename to parse. This will not work if the extension is included.
 * @return int The numeric suffix value
 */
int get_file_number_suffix(const std::string& filename);



#endif 