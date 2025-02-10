#include <string>
#include <vector>
#include <cctype>
#include <fstream>

#include "boost/filesystem.hpp"

#include "logging.h"

std::string trim_left(const std::string& input) {
	if (input.size() == 0) {
		return "";
	}

	size_t start = 0;
	for (size_t i = 0; i < input.size(); i++) {
		if (input[i] == ' ') {
			start += 1;
		}
		else {
			break;
		}
	}

	if (start < input.size()) {
		return input.substr(start);
	}
	else {
		return "";
	}
}

std::string trim_right(const std::string& input) {
	if (input.size() == 0) {
		return "";
	}

	int end = input.size() - 1;
	for (int i = input.size() - 1; i >= 0; i--) {
		if (input[i] == ' ') {
			end -= 1;
		}
		else {
			break;
		}
	}
	if (end >= 0) {
		return input.substr(0, end + 1);
	}
	else {
		return "";
	}
}

std::string trim(const std::string input) {
	return trim_left(trim_right(input));
}

std::vector<std::string> split_string(const std::string& input, char delimeter, bool is_trim) {
	std::vector<std::string> to_return;
	size_t start = 0;
	for (size_t i = 0; i < input.size(); i++) {
		char c = input[i];
		if (c == delimeter) {
			size_t count = i - start;
			std::string part = input.substr(start, count);
			start = i + 1;

			if (is_trim) { part = trim(part); }
			if (part.size() > 0) { to_return.push_back(part); }
		}
	}

	if (start < input.size()) {
		size_t count = input.size() - start;
		std::string part = input.substr(start, count);

		if (is_trim) { part = trim(part); }
		if (part.size() > 0) { to_return.push_back(part); }
	}

	return to_return;
}

std::string snake_to_space_cammel(const std::string& input) {
	std::string to_return = "";

	size_t start = 0;
	for (size_t i = 0; i < input.size(); i++) {
		char c = input[i];
		if (c == '_') {
			size_t count = i - start;
			std::string part = input.substr(start, count);
			start = i + 1;

			// capitalize the first letter
			part[0] = std::toupper(part[0]);

			to_return += part;
			if (i < input.size() - 1 && count > 0) {
				to_return += " ";
			}
		}
	}

	// add the last segment
	if (start < input.size()) {
		size_t count = input.size() - start;
		std::string part = input.substr(start, count);
		part[0] = std::toupper(part[0]);

		to_return += part;
	}

	return to_return;
}

std::string get_library_name(const std::string& file_path) {
	boost::filesystem::path path = file_path;
	std::vector<std::string> parts;
	for (auto& part : path) {
		parts.push_back(part.string());
	}

	std::string file_name = parts[parts.size() - 1];

	for (size_t i = 0; i < parts.size(); i++) {
		std::string part = parts[parts.size() - 1 - i];
		if (part == "libraries" && i > 0) {
			std::string prev_part = parts[parts.size() - i];
			if (prev_part != file_name) {
				return prev_part;
			}
		}
	}

	return "";
}

std::string prog_id_to_name(const XSI::CString& prog_id) {
	// split by point and extract the second part
	// prog id has the form: Plugin.Name.Version.MinVersion
	XSI::CStringArray parts = prog_id.Split(".");
	if (parts.GetCount() > 1) {
		return parts[1].GetAsciiString();
	}

	return "";
}

void write_text_file(const std::string& text, const std::string& file_path) {
	std::ofstream file;
	file.open(file_path);
	file << text;
	file.close();
}

std::string add_suffix_to_path(const std::string& input_path, const std::string& suffix) {
	size_t dot_pos = input_path.find_last_of(".");
	if (dot_pos == std::string::npos) {
		return std::string(input_path);
	}
	else {
		return input_path.substr(0, dot_pos) + suffix + input_path.substr(dot_pos);
	}
}

std::string tranfsorm_output_path(const std::string& output_path, const std::string& element_name, size_t elements_count) {
	if (elements_count > 1) {
		return add_suffix_to_path(output_path, "_" + element_name);
	}
	else {
		return std::string(output_path);
	}
}

// use std::string, because it's easier to find and change letters in these strings
XSI::CString replace_letter(const XSI::CString& input, char from, char to) {
	std::string to_return(input.GetAsciiString());
	for (size_t i = 0; i < to_return.size(); i++) {
		if (to_return[i] == from) {
			to_return[i] = to;
		}
	}

	return XSI::CString(to_return.c_str());
}

void check_output_path(std::string& output_path) {
	// extract the directory
	size_t pos = output_path.rfind('\\');
	if (pos != std::string::npos) {
		std::string directory = output_path.substr(0, pos);

		try {
			if (!boost::filesystem::exists(directory)) {
				boost::filesystem::create_directories(directory);
			}
		}
		catch (const boost::filesystem::filesystem_error& e) {
			log_message(("Error: " + std::string(e.what())).c_str());
		}
	}
}