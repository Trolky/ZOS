#include "path_utils.h"
#include <sstream>

std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;
    
    while (std::getline(ss, part, '/')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    
    return parts;
}

std::string join_path(const std::vector<std::string>& parts) {
    std::string path;
    for (const auto& part : parts) {
        path += "/" + part;
    }
    return path.empty() ? "/" : path;
}