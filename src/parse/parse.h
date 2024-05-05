#pragma once
#include <string>
#include <unordered_map>

#include <xsi_status.h>
#include <xsi_context.h>

// signrature: key - full name of the node (with ND_)
// value - tuple, the first - short name, used fotr the type
// the second - array of input names and corresponding types
// the second - array for outputs
std::unordered_map<std::string, std::tuple<std::string, std::vector<std::tuple<std::string, std::string>>, std::vector<std::tuple<std::string, std::string>>>> get_fullname_to_data();

XSI::CStatus on_query_parser_settings(XSI::Context &context);
XSI::CStatus on_parse_info(XSI::Context& context);
XSI::CStatus on_parse(XSI::Context& context);