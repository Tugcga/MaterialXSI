#pragma once
#include <string>

// split string by delimeter
std::vector<std::string> split_string(const std::string& input, char delimeter, bool is_trim);

// convert string in snake case: abc_def_123 to spaced cammel case: Abc Def 123
std::string snake_to_space_cammel(const std::string& input);

// return MaterialX library name for the input file
// this library is a folder name after "libraries" in the path
// if there are no such directory, then library is empty
std::string get_library_name(const std::string &file_path);

// extract class name from shader prog id
// it's actual the second part
std::string prog_id_to_name(const XSI::CString& prog_id);

// simple function to write text data into file
void write_text_file(const std::string& text, const std::string& file_path);

// change output file name - add suffix at the end of the name
std::string add_suffix_to_path(const std::string& input_path, const std::string& suffix);

// add suffix to otuput file name if we would like to export several materials to non-mtlx format
// this suffic is just element name
std::string tranfsorm_output_path(const std::string& output_path, const std::string& element_name, size_t elements_count);

// change letter in the string
// used for convert unique names of shader nodes: from Node<123> to Node_123_, because < and > symbols are nor valid in shader code of some format
XSI::CString replace_letter(const XSI::CString& input, char from, char to);

// create all output directories by using boost
void check_output_path(std::string& output_path);