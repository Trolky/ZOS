#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <string>
#include <vector>

std::vector<std::string> split_path(const std::string& path);
std::string join_path(const std::vector<std::string>& parts);

#endif