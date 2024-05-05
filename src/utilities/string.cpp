#include <string>
#include <vector>
#include <cctype>

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