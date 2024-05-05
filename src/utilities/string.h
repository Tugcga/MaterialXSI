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