#pragma once

#include <string>
#include <vector>

/*
StringUtils - String manipulation utility functions

Provides common string operations used throughout the engine.
*/

namespace StringUtils {

    // Trim whitespace from both ends of a string
    std::string Trim(const std::string& str);

    // Split a string by a delimiter
    std::vector<std::string> Split(const std::string& str, char delimiter);

} // namespace StringUtils
