#include <cstdint>
#include <functional>
#include <iostream>
#include "filesystem.h"
#include <algorithm>
#include "path_utils.h"


const int32_t FAT_UNUSED = INT32_MAX -1;
const int32_t FAT_FILE_END = INT32_MAX -2;
const int32_t FAT_BAD_CLUSTER = INT32_MAX -3;

const int32_t CLUSTER_SIZE = 4096;
 
filesystem::filesystem(const std::string &file): file_name(file), current_directory(nullptr), next_dir_id(0){
    std::ifstream file_stream(file_name, std::ios::binary);
    if (!file_stream){
        std::string command, arg;
        while (true){
            std::cout << "You need to format the file, enter format <size><unit(MB,KB)>" << std::endl;
            std::cout << "> ";
            std::cin >> command;

            if (command == "exit"){
                break;
            }

            if (command == "format"){
                std::cin >> arg;
                if (format_fs(arg)) {
                   break;
                }
            }
        }
    } else{
        load_fs();
        update_dir_id();
    }
}

int32_t filesystem::parse_size(const std::string& size_str) {
    int32_t base_size = 0;
    std::string unit = size_str.substr(size_str.size() - 2);
    try {
        base_size = std::stoi(size_str.substr(0, size_str.size() - 2)); // Get the numeric part
    }
    catch (...) {
        std::cerr << "Invalid size" << std::endl;
        return -1;
    }
    if (unit == "B") {
        return base_size;
    }
    if (unit == "KB") {
        return base_size * 1024;
    }
    if (unit == "MB") {
        return base_size * 1024 * 1024;
    }
    if (unit == "GB") {
        return base_size * 1024 * 1024 * 1024;
    }

    std::cerr << "Invalid size unit." << std::endl;
    return -1;
}

    // Format the disk
bool filesystem::format_fs(const std::string &sizeStr){
    const int32_t DISK_SIZE = parse_size(sizeStr);
    if (DISK_SIZE == -1) {
        return false;
    }

    std::strcpy(desc.signature, "reichm");
    desc.disk_size = DISK_SIZE;
    desc.cluster_size = CLUSTER_SIZE;

    desc.cluster_count = desc.disk_size / desc.cluster_size;
    desc.fat_count = desc.cluster_count;

    desc.fat1_start_address = sizeof(description);
    desc.fat2_start_address = desc.fat1_start_address + desc.fat_count * sizeof(int32_t);
    desc.data_start_address = desc.fat2_start_address + desc.fat_count * sizeof(int32_t);
    desc.directory_start_address = desc.data_start_address;

    fat1.resize(desc.fat_count, FAT_UNUSED);
    fat2.resize(desc.fat_count, FAT_UNUSED);

    // Create or overwrite the .dat file with the specified disk size
    std::ofstream out(file_name, std::ios::binary | std::ios::trunc);

    if (!out.is_open()){
        std::cerr << "Cannot create filesystem\n";
        return false;
    }

    const size_t chunk_size = 4096;
    std::vector<char> buffer(chunk_size, 0);
    size_t total_written = 0;

    while (total_written < DISK_SIZE){
        out.write(buffer.data(), std::min(chunk_size, DISK_SIZE - total_written));
        total_written += chunk_size;
    }
    out.close();

    root_folder.clear();
    directory_item root;
    std::strcpy(root.item_name, "/");
    root.is_file = false;
    root.size = 0;
    root.start_cluster = -1;
    root.parent_id = -1;
    root.id = next_dir_id++;
    root_folder.push_back(root);
    current_directory = &root_folder[0];

    save_fs();

    std::cout << "OK\n";
    return true;
}

void filesystem::save_fs(){
    std::ofstream file(file_name, std::ios::binary | std::ios::in | std::ios::out);
    if (!file){
        std::cerr << "Error opening file for saving.\n";
        return;
    }

    file.seekp(0);
    file.write(reinterpret_cast<const char *>(&desc), sizeof(desc));


    file.seekp(desc.fat1_start_address);
    file.write(reinterpret_cast<const char *>(fat1.data()), fat1.size() * sizeof(int32_t));
    file.seekp(desc.fat2_start_address);
    file.write(reinterpret_cast<const char *>(fat2.data()), fat2.size() * sizeof(int32_t));

    file.seekp(desc.directory_start_address);
    for (const auto &dir : root_folder){
        save_directory(file, dir);
    }

    file.close();
}

void filesystem::save_directory(std::ofstream &outFile, const directory_item &dir){

    outFile.write(reinterpret_cast<const char *>(&dir.item_name), sizeof(dir.item_name));
    outFile.write(reinterpret_cast<const char *>(&dir.is_file), sizeof(dir.is_file));
    outFile.write(reinterpret_cast<const char *>(&dir.size), sizeof(dir.size));
    outFile.write(reinterpret_cast<const char *>(&dir.start_cluster), sizeof(dir.start_cluster));
    outFile.write(reinterpret_cast<const char *>(&dir.parent_id), sizeof(dir.parent_id));
    outFile.write(reinterpret_cast<const char *>(&dir.id), sizeof(dir.id));

    size_t childrenCount = dir.children.size();
    outFile.write(reinterpret_cast<const char *>(&childrenCount), sizeof(size_t));

    for (const auto &child : dir.children){
        save_directory(outFile, child);
    }
}

void filesystem::load_fs(){

    std::ifstream in(file_name, std::ios::binary);
    if (!in){
        throw std::runtime_error("Error opening filesystem file");
    }

    in.read(reinterpret_cast<char *>(&desc), sizeof(desc));

    fat1.resize(desc.fat_count);
    in.seekg(desc.fat1_start_address);
    in.read(reinterpret_cast<char *>(fat1.data()), fat1.size() * sizeof(int32_t));
    fat2.resize(desc.fat_count);
    in.seekg(desc.fat2_start_address);
    in.read(reinterpret_cast<char *>(fat2.data()), fat2.size() * sizeof(int32_t));

    in.seekg(desc.directory_start_address);

    root_folder.clear();

    size_t count = 0;
    while (in && count < 1000){
        directory_item dir;
        load_dir(in, dir);
        if (std::strlen(dir.item_name) == 0 || dir.id < 0 || dir.parent_id < -1){
            break;
        }
        root_folder.push_back(dir);
        count++;
    }

    current_directory = &root_folder[0];
    in.close();
    if (!check()) {
        std::cout << "Loaded filesystem with signature: " << desc.signature << std::endl;
        std::cout << "Disk size: " << desc.disk_size << " bytes" << std::endl;
        std::cout << "Cluster size: " << desc.cluster_size << " bytes" << std::endl;
        std::cout << "Total clusters: " << desc.cluster_count << std::endl;
    }
}

void filesystem::update_dir_id(){
    next_dir_id = 0;

    std::function<void(const directory_item &)> findMaxId = [&](const directory_item &dir){
        if (dir.id > next_dir_id){
            next_dir_id = dir.id;
        }

        for (const auto &child : dir.children){
            findMaxId(child);
        }
    };

    for (const auto &dir : root_folder){
        findMaxId(dir);
    }

    next_dir_id++;
}

void filesystem::load_dir(std::ifstream &in, directory_item &dir){
    in.read(reinterpret_cast<char *>(&dir.item_name), sizeof(dir.item_name));
    in.read(reinterpret_cast<char *>(&dir.is_file), sizeof(dir.is_file));
    in.read(reinterpret_cast<char *>(&dir.size), sizeof(dir.size));
    in.read(reinterpret_cast<char *>(&dir.start_cluster), sizeof(dir.start_cluster));
    in.read(reinterpret_cast<char *>(&dir.parent_id), sizeof(dir.parent_id));
    in.read(reinterpret_cast<char *>(&dir.id), sizeof(dir.id));

    size_t childrenCount;
    in.read(reinterpret_cast<char *>(&childrenCount), sizeof(size_t));

    dir.children.resize(childrenCount);

    for (auto &child : dir.children){
        load_dir(in, child);
    }
}

std::string filesystem::current_file_path(directory_item* dir){
    if (dir->parent_id == -1) {
        return "";
    }

    std::vector<std::string> pathParts;
    directory_item* current = dir;

    while (current->parent_id != -1) {
        pathParts.push_back(current->item_name);
        current = find_dir_by_given_id(current->parent_id, &root_folder[0]);
    }

    std::reverse(pathParts.begin(), pathParts.end());

    std::string path;
    for (const auto& part : pathParts) {
        path += part + "/";
    }

    if (!path.empty()) {
        path.pop_back();
    }

    return trim_spaces(path);
}

std::string filesystem::trim_spaces(const std::string &input){
    std::string result = input;
    result.erase(std::remove(result.begin(), result.end(), ' '), result.end());
    return result;
}

directory_item *filesystem::find_dir_by_given_id(int32_t id, directory_item *dir){
    if (dir->id == id){
        return dir;
    }

    for (auto &child : dir->children){
        directory_item *foundDir = find_dir_by_given_id(id, &child);
        if (foundDir){
            return foundDir;
        }
    }

    return nullptr;
}

bool filesystem::directory_exists(directory_item* current_dir, const std::string& dir_name) {
    for (const auto& child : current_dir->children) {
        if (std::string(child.item_name) == dir_name) {
            return true;
        }
    }
    return false;
}

bool filesystem::make_directory(directory_item* current_dir, const std::string& path) {
    std::string new_dir_name;
    directory_item* parent = get_parent_directory(path, current_dir, new_dir_name);

    if (!parent) {
        std::cerr << "Parent directory does not exist\n";
        return false;
    }

    if (new_dir_name.length() >= 12) {
        std::cerr << "Directory name too long (max 11 characters)\n";
        return false;
    }

    if (directory_exists(parent, new_dir_name)) {
        std::cerr << "Directory already exists\n";
        return false;
    }

    directory_item new_dir(new_dir_name, false);
    new_dir.parent_id = parent->id;
    new_dir.id = next_dir_id++;
    new_dir.start_cluster = -1;

    parent->children.push_back(new_dir);
    save_fs();
    return true;
}

void filesystem::list_directory(directory_item* dir) {
    std::string path = current_file_path(dir);

    if (dir->children.empty()) {
        std::cout << "Directory is empty\n";
        return;
    }

    for (const auto& item : dir->children) {
        std::string type = item.is_file ? "F" : "D";
        std::cout << type << " " << item.item_name;
        if (item.is_file) {
            std::cout << " (" << item.size << " bytes)";
        }
        std::cout << "\n";
    }
}

directory_item* filesystem::change_directory(directory_item* current_dir, const std::string& path) {
    directory_item* new_dir = find_directory_by_path(current_dir, path);
    if (!new_dir) {
        std::cerr << "Directory not found\n";
        return current_dir;
    }
    return new_dir;
}

directory_item* filesystem::find_directory_by_path(directory_item* start_dir, const std::string& path) {
    if (path.empty() || path == "/") {
        return &root_folder[0];
    }

    auto parts = split_path(path);
    directory_item* current = start_dir;

    for (const auto& part : parts) {
        if (part == "..") {
            if (current->parent_id != -1) {
                current = find_dir_by_given_id(current->parent_id, &root_folder[0]);
            }
            continue;
        }
        if (part == ".") {
            continue;
        }

        bool found = false;
        for (auto& child : current->children) {
            if (!child.is_file && std::string(child.item_name) == part) {
                current = &child;
                found = true;
                break;
            }
        }
        if (!found) {
            return nullptr;
        }
    }
    return current;
}

directory_item* filesystem::get_parent_directory(const std::string& path, directory_item* current_dir, std::string& child_name) {
    auto parts = split_path(path);
    if (parts.empty()) {
        return current_dir;
    }

    child_name = parts.back();
    parts.pop_back();

    if (parts.empty()) {
        return current_dir;
    }

    directory_item* parent = find_directory_by_path(current_dir, join_path(parts));
    return parent;
}

bool filesystem::remove_directory(directory_item* current_dir, const std::string& path) {
    std::string dir_name;
    directory_item* parent = get_parent_directory(path, current_dir, dir_name);

    if (!parent) {
        std::cerr << "File not found\n";
        return false;
    }

    // Find the directory to remove
    auto it = std::find_if(parent->children.begin(), parent->children.end(),
        [&dir_name](const directory_item& item) {
            return std::string(item.item_name) == dir_name;
        });

    if (it == parent->children.end()) {
        std::cerr << "File not found\n";
        return false;
    }

    if (it->is_file) {
        std::cerr << "Not a directory\n";
        return false;
    }

    // Check if directory is empty
    if (!it->children.empty()) {
        std::cerr << "Directory is not empty\n";
        return false;
    }

    // Remove the directory
    parent->children.erase(it);
    save_fs();
    std::cout << "Ok\n";
    return true;
}

bool filesystem::remove_file(directory_item* current_dir, const std::string& path) {
    std::string dir_name;
    directory_item* parent = get_parent_directory(path, current_dir, dir_name);

    if (!parent) {
        std::cerr << "File not found\n";
        return false;
    }

    // Find the directory to remove
    auto it = std::find_if(parent->children.begin(), parent->children.end(),
        [&dir_name](const directory_item& item) {
            return std::string(item.item_name) == dir_name;
        });

    if (it == parent->children.end()) {
        std::cerr << "File not found\n";
        return false;
    }

    // Check if directory is empty
    if (!it->is_file) {
        std::cerr << "Not a file\n";
        return false;
    }

    int cluster = it->start_cluster;
    while (cluster != FAT_FILE_END && cluster >= 0 && cluster < fat1.size())
    {
        int next_cluster = fat1[cluster];
        fat1[cluster] = FAT_UNUSED; // Mark the cluster as unused
        cluster = next_cluster;
    }

    // Remove the directory
    parent->children.erase(it);
    save_fs();
    std::cout << "Ok\n";
    return true;
}

std::string filesystem::print_working_directory(directory_item* dir) {
    std::string path = current_file_path(dir);
    return path.empty() ? "/" : path;
}

bool filesystem::copy_file_to_fs(const std::string& source_path, directory_item* current_dir,
    const std::string& dest_path, std::vector<int32_t>& fat, int32_t& cluster_count) {

    std::ifstream source(source_path, std::ios::binary | std::ios::ate);
    if (!source) {
        std::cerr << "File not found\n";
        return false;
    }

    std::streamsize file_size = source.tellg();
    source.seekg(0, std::ios::beg);

    // Extract original filename from source path
    size_t last_slash = source_path.find_last_of("/\\");
    std::string original_filename = (last_slash != std::string::npos) ?
        source_path.substr(last_slash + 1) : source_path;

    std::string file_name = original_filename;
    directory_item* parent = get_parent_directory(dest_path, current_dir, file_name);

    if (!parent) {
        std::cerr << "Path not found\n";
        return false;
    }

    // Check for duplicate filename and add unique identifier if needed
    int counter = 1;
    std::string base_name = file_name;
    size_t dot_pos = base_name.find_last_of('.');
    std::string name_part = (dot_pos != std::string::npos) ? base_name.substr(0, dot_pos) : base_name;
    std::string ext_part = (dot_pos != std::string::npos) ? base_name.substr(dot_pos) : "";

    while (std::any_of(parent->children.begin(), parent->children.end(),
           [&file_name](const directory_item& item) {
               return std::string(item.item_name) == file_name;
           })) {
        file_name = name_part + std::to_string(counter) + ext_part;
        counter++;
    }

    int32_t clusters_needed = (file_size + CLUSTER_SIZE - 1) / CLUSTER_SIZE;
    std::vector<int32_t> allocated_clusters;

    for (int32_t i = 0; i < clusters_needed; ++i) {
        int free_cluster = allocate_cluster();
        if (free_cluster == -1){
            std::cerr << "Not enough space\n";
            return false;
        }
        allocated_clusters.push_back(free_cluster);
    }

    std::ofstream outFile(filesystem::file_name, std::ios::binary | std::ios::in | std::ios::out);
    for (int i = 0; i < clusters_needed; ++i){
        char buffer[CLUSTER_SIZE];
        source.read(buffer, CLUSTER_SIZE);
        outFile.seekp(desc.data_start_address + allocated_clusters[i] * CLUSTER_SIZE);
        outFile.write(buffer, CLUSTER_SIZE);

        if (i < clusters_needed - 1){
            fat[allocated_clusters[i]] = allocated_clusters[i + 1];
        }
        else
        {
            fat[allocated_clusters[i]] = FAT_FILE_END;
        }
    }

    directory_item new_file;
    std::strncpy(new_file.item_name, file_name.c_str(), sizeof(new_file.item_name) - 1);
    new_file.is_file = true;
    new_file.start_cluster = allocated_clusters[0];
    new_file.id = next_dir_id++;
    new_file.size = file_size;


    parent->children.push_back(new_file);

    save_fs();

    std::cout << "OK\n";
    return true;

}

int filesystem::allocate_cluster() {
    for (size_t i = 1; i < fat1.size(); ++i){
        if (fat1[i] == FAT_UNUSED){
            fat1[i] = FAT_FILE_END;
            return i;
        }
    }
    return -1; // No free cluster found
}



bool filesystem::copy_file_from_fs(directory_item* current_dir, const std::string& source_path,
                                   const std::string& dest_path, const std::vector<int32_t>& fat) {

    std::string file_name;
    directory_item* parent = get_parent_directory(source_path, current_dir, file_name);
    if (!parent) {
        std::cerr << "File not found\n";
        return false;
    }

    auto it = std::find_if(parent->children.begin(), parent->children.end(),
        [&file_name](const directory_item& item) {
            return std::string(item.item_name) == file_name && item.is_file;
        });

    if (it == parent->children.end()) {
        std::cerr << "File not found\n";
        return false;
    }

    std::ofstream dest(dest_path, std::ios::binary);
    if (!dest) {
        std::cerr << "Path not found\n";
        return false;
    }

    // Copy file content
    std::vector<int32_t> clusters = get_cluster_chain(it->start_cluster, fat);

    char buffer[CLUSTER_SIZE];  // Buffer to hold data while reading from the clusters
    size_t total_bytes_written = 0;
    size_t file_size = it->size;
    size_t bytes_left = file_size;

    for (size_t i = 0; i < clusters.size() && bytes_left > 0; ++i) {
        int32_t cluster = clusters[i];

        // Open the filesystem in binary mode to read data
        std::ifstream fs_file(filesystem::file_name, std::ios::binary);
        if (!fs_file) {
            std::cerr << "Error opening filesystem\n";
            return false;
        }

        // Move the file pointer to the start of the current cluster
        fs_file.seekg(desc.data_start_address + cluster * CLUSTER_SIZE, std::ios::beg);

        // Read the data from the current cluster, but ensure that we don't read more than the remaining bytes
        size_t bytes_to_read = std::min(static_cast<size_t>(CLUSTER_SIZE), bytes_left);
        fs_file.read(buffer, bytes_to_read);
        std::streamsize bytes_read = fs_file.gcount();

        // Write the valid data (up to the remaining file size) to the destination file
        dest.write(buffer, bytes_read);

        if (!dest) {
            std::cerr << "Error writing to destination file\n";
            return false;
        }

        total_bytes_written += bytes_read;
        bytes_left -= bytes_read;
    }

    std::cout << "OK\n";
    return true;
}

std::string filesystem::get_file_clusters(directory_item* current_dir, const std::string& path,
    const std::vector<int32_t>& fat) {

    std::string file_name;
    directory_item* parent = get_parent_directory(path, current_dir, file_name);
    if (!parent) {
        std::cerr << "File not found\n";
        return "";
    }

    auto it = std::find_if(parent->children.begin(), parent->children.end(),
        [&file_name](const directory_item& item) {
            return std::string(item.item_name) == file_name;
        });

    if (it == parent->children.end()) {
        std::cerr << "File not found\n";
        return "";
    }

    std::vector<int32_t> clusters = get_cluster_chain(it->start_cluster, fat);
    std::stringstream ss;
    ss << file_name;
    for (int32_t cluster : clusters) {
        ss << " " << cluster;
    }
    return ss.str();
}

bool filesystem::read_file_content(directory_item* current_dir, const std::string& path) {
    std::string file_name;
    directory_item* parent = get_parent_directory(path, current_dir, file_name);
    if (!parent) {
        std::cerr << "File not found\n";
        return false;
    }

    auto it = std::find_if(parent->children.begin(), parent->children.end(),
        [&file_name](const directory_item& item) {
            return std::string(item.item_name) == file_name && item.is_file;
        });

    if (it == parent->children.end()) {
        std::cerr << "File not found\n";
        return false;
    }

    int cluster = it->start_cluster;
    std::vector<char> data(it->size);
    size_t bytes= 0;

    std::ifstream fs_file(filesystem::file_name, std::ios::binary);
    if (!fs_file) {
        std::cerr << "Error opening filesystem\n";
        return "";
    }
    while (cluster != FAT_FILE_END && bytes < it->size){
        // Calculate the position in the file for the current cluster
        fs_file.seekg(desc.data_start_address + cluster * CLUSTER_SIZE);

        // Determine the number of bytes to read from this cluster
        size_t bytes_to_read = std::min(static_cast<size_t>(CLUSTER_SIZE), it->size - bytes);

        // Read the data from the current cluster into the buffer
        fs_file.read(data.data() + bytes, bytes_to_read);

        bytes += bytes_to_read;

        // Check the FAT to find the next cluster
        int next_cluster = fat1[cluster];
        cluster = next_cluster;
    }

    fs_file.close();
    std::cout.write(data.data(), it->size);
    std::cout << std::endl;
    return true;
}


std::vector<int32_t> filesystem::get_cluster_chain(int32_t start_cluster, const std::vector<int32_t>& fat) {
    std::vector<int32_t> clusters;
    int32_t current = start_cluster;

    while (current != FAT_FILE_END && current >= 0) {
        clusters.push_back(current);
        current = fat[current];
    }

    return clusters;
}

bool filesystem::copy_file_in(const std::string& source_path, const std::string& dest_path) {
    return copy_file_to_fs(source_path, current_directory, dest_path, fat1, desc.cluster_count);
}

bool filesystem::copy_file_out(const std::string& source_path, const std::string& dest_path) {
    return copy_file_from_fs(current_directory, source_path, dest_path, fat1);
}

std::string filesystem::get_file_info(const std::string& path) {
    return get_file_clusters(current_directory, path, fat1);
}

bool filesystem::cat_file(const std::string& path) {
    return read_file_content(current_directory, path);
}

bool filesystem::copy_file(const std::string& source_path, const std::string& dest_path) {
    //Step 1: Locate the source file
    std::string source_file_name;
    directory_item* source_parent = get_parent_directory(source_path, current_directory, source_file_name);
    if (!source_parent) {
        std::cerr << "Source file not found\n";
        return false;
    }

    auto source_it = std::find_if(source_parent->children.begin(), source_parent->children.end(),
        [&source_file_name](const directory_item& item) {
            return std::string(item.item_name) == source_file_name && item.is_file;
        });

    if (source_it == source_parent->children.end()) {
        std::cerr << "Source file not found\n";
        return false;
    }

    // Step 2: Locate the destination directory
    std::string dest_file_name;
    directory_item* dest_parent = get_parent_directory(dest_path, current_directory, dest_file_name);
    if (!dest_parent) {
        std::cerr << "Destination path not found\n";
        return false;
    }

    // If no filename is provided in dest_path, use the source file's name as the destination name
    if (dest_file_name.empty()) {
        dest_file_name = source_file_name;
    }

    // Check for duplicate filename and ensure uniqueness
    std::string final_name = dest_file_name;
    int counter = 1;
    size_t dot_pos = dest_file_name.find_last_of('.');
    std::string name_part = (dot_pos != std::string::npos) ? dest_file_name.substr(0, dot_pos) : dest_file_name;
    std::string ext_part = (dot_pos != std::string::npos) ? dest_file_name.substr(dot_pos) : "";

    while (std::any_of(dest_parent->children.begin(), dest_parent->children.end(),
           [&final_name](const directory_item& item) {
               return std::string(item.item_name) == final_name;
           })) {
        final_name = name_part + std::to_string(counter) + ext_part;
        counter++;
    }


    // Allocate clusters for the copy
    int cluster = source_it->start_cluster;
    std::vector<int> clusters;
    int file_size = source_it->size;

    while (cluster != FAT_FILE_END && cluster >= 0 && cluster < fat1.size()){
        int new_cluster = allocate_cluster();
        if (new_cluster == -1){
            std::cerr << "Not enough space\n";
            return false ;
        }
        clusters.push_back(new_cluster);

        // Copy data from the source cluster to the destination cluster
        char buffer[CLUSTER_SIZE];
        std::ifstream source_file(file_name, std::ios::binary);
        source_file.seekg(desc.data_start_address + cluster * CLUSTER_SIZE);
        source_file.read(buffer, std::min(CLUSTER_SIZE, file_size));


        std::ofstream destin_file(file_name, std::ios::binary | std::ios::in | std::ios::out);
        destin_file.seekp(desc.data_start_address + new_cluster * CLUSTER_SIZE);
        destin_file.write(buffer, std::min(CLUSTER_SIZE, file_size));

        cluster = fat1[cluster];
        file_size -= CLUSTER_SIZE; // Decrease remaining size by cluster size
    }

    // Update FAT to mark the end of the copied file's cluster chain
    for (size_t i = 0; i < clusters.size(); ++i){
        fat1[clusters[i]] = (i == clusters.size() - 1) ? FAT_FILE_END : clusters[i + 1];
    }

    directory_item new_file;
    std::strncpy(new_file.item_name, final_name.c_str(), sizeof(new_file.item_name) - 1);
    new_file.is_file = true;
    new_file.start_cluster = clusters[0];
    new_file.id = next_dir_id++;
    new_file.size = source_it->size;
    new_file.parent_id = dest_parent->id;


    // Step 6: Add the new file to the destination directory
    dest_parent->children.push_back(new_file);
    save_fs();
    std::cout << "OK\n";
    return true;
}

bool filesystem::move_file(const std::string& source_path, const std::string& dest_path) {
    std::string source_file_name;
    directory_item* source_parent = get_parent_directory(source_path, current_directory, source_file_name);
    if (!source_parent) {
        std::cerr << "File not found\n";
        return false;
    }

    auto source_it = std::find_if(source_parent->children.begin(), source_parent->children.end(),
        [&source_file_name](const directory_item& item) {
            return std::string(item.item_name) == source_file_name && item.is_file;
    });

    if (source_it == source_parent->children.end()) {
        std::cerr << "File not found\n";
        return false;
    }

    //Locate the destination directory
    std::string dest_file_name;
    directory_item* dest_parent = get_parent_directory(dest_path, current_directory, dest_file_name);
    if (!dest_parent) {
        std::cerr << "Path not found\n";
        return false;
    }

    // If no filename is provided in dest_path, use the source file's name as the destination name
    if (dest_file_name.empty()) {
        dest_file_name = source_file_name;
    }

    // Check for duplicate filename and ensure uniqueness
    std::string final_name = dest_file_name;
    int counter = 1;
    size_t dot_pos = dest_file_name.find_last_of('.');
    std::string name_part = (dot_pos != std::string::npos) ? dest_file_name.substr(0, dot_pos) : dest_file_name;
    std::string ext_part = (dot_pos != std::string::npos) ? dest_file_name.substr(dot_pos) : "";

    while (std::any_of(dest_parent->children.begin(), dest_parent->children.end(),
           [&final_name](const directory_item& item) {
               return std::string(item.item_name) == final_name;
           })) {
        final_name = name_part + std::to_string(counter) + ext_part;
        counter++;
    }


    directory_item *src_p = (source_it->parent_id == -1) ? &root_folder[0] : source_parent;

    if (src_p){
        // Check if the parent is the root directory
        if (src_p == &root_folder[0]){
            // Attempt to remove the item directly from rootDirectory by comparing IDs explicitly
            src_p->children.erase(
                std::remove_if(src_p->children.begin(), src_p->children.end(),
                               [&](const directory_item &item){
                                   // Compare both the ID and name to avoid accidental deletion of similarly named items
                                   bool isMatch = (item.id == source_it->id && item.item_name == source_it->item_name);
                                   return isMatch;
                               }),
                src_p->children.end());
        }
        else{
            src_p->children.erase(
                std::remove_if(src_p->children.begin(), src_p->children.end(),
                               [&](const directory_item &item){
                                   // Explicitly compare the ID instead of pointer address
                                   bool isMatch = (item.id == source_it->id && item.item_name == source_it->item_name);
                                   return isMatch;
                               }),
                source_parent->children.end());
        }
    }

    // Create a copy of the item with updated attributes for the destination directory
    directory_item moved_item = *source_it;
    moved_item.parent_id = dest_parent->id;
    // std::cout << "New item name: " << newName << '\n';
    std::strcpy(moved_item.item_name, final_name.c_str());
    // std::cout << "Moved item name: " << movedItem.item_name << '\n';
    moved_item.id = source_it->id;
    moved_item.is_file = true;
    // Add the moved item to the destination directory
    dest_parent->children.push_back(moved_item);
    save_fs();
    std::cout << "OK\n";
    return true;
}

bool filesystem::load(directory_item* current_dir, const std::string &filePath){
    std::ifstream file(filePath);
    if (!file){
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return false;
    }

    directory_item *cur_dir = current_dir;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::istringstream lineStream(line);
        std::string command;
        lineStream >> command;

        std::string arguments;
        std::getline(lineStream, arguments);
        arguments.erase(0, arguments.find_first_not_of(" \t"));
        arguments.erase(arguments.find_last_not_of(" \t") + 1);

        if (command == "mkdir") {
            if (!make_directory(cur_dir, arguments)) {
                std::cerr << "Failed to create directory: " << arguments << "\n";
            }
        }
        else if (command == "rmdir") {
            if (!remove_directory(cur_dir, arguments)) {
                std::cerr << "Failed to remove directory: " << arguments << "\n";
            }
        }
        else if (command == "cd") {
            directory_item* new_dir = change_directory(cur_dir, arguments);
            if (new_dir) {
                cur_dir = new_dir;
            } else {
                std::cerr << "Directory not found: " << arguments << "\n";
            }
        }
        else if (command == "ls") {
            directory_item* target_dir = find_directory_by_path(cur_dir, arguments);
            if (target_dir) {
                list_directory(target_dir);
            } else {
                std::cerr << "Directory not found: " << arguments << "\n";
            }
        }
        else if (command == "incp" || command == "outcp" || command == "mv" || command == "cp") {
            std::istringstream argsStream(arguments);
            std::string src, dest;
            argsStream >> src >> dest;

            if (command == "incp" && !copy_file_in(src, dest)) {
                std::cerr << "Failed to copy file from: " << src << " to " << dest << "\n";
            }
            else if (command == "outcp" && !copy_file_out(src, dest)) {
                std::cerr << "Failed to copy file from: " << src << " to " << dest << "\n";
            }
            else if (command == "mv" && !move_file(src, dest)) {
                std::cerr << "Failed to move file from: " << src << " to " << dest << "\n";
            }
            else if (command == "cp" && !copy_file(src, dest)) {
                std::cerr << "Failed to copy file from: " << src << " to " << dest << "\n";
            }
        }
        else if (command == "rm") {
            if (!remove_file(cur_dir, arguments)) {
                std::cerr << "Failed to remove file: " << arguments << "\n";
            }
        }
        else if (command == "cat") {
            read_file_content(cur_dir, arguments);
        }
        else if (command == "info") {
            std::cout << get_file_info(arguments) << std::endl;
        }
        else {
            std::cerr << "Unknown command: " << command << "\n";
        }
    }

    file.close();
    return true;
}

bool filesystem::bug(const std::string &path) {
    std::string source_file_name;
    directory_item* source_parent = get_parent_directory(path, current_directory, source_file_name);
    if (!source_parent) {
        std::cerr << "File not found\n";
        return false;
    }

    auto it = std::find_if(source_parent->children.begin(), source_parent->children.end(),
        [&source_file_name](const directory_item& item) {
            return std::string(item.item_name) == source_file_name && item.is_file;
    });

    if (it == source_parent->children.end()) {
        std::cerr << "File not found\n";
        return false;
    }

    int cluster = it->start_cluster;
    bool corrupted = false;

    while (cluster != FAT_FILE_END && cluster >= 0 && cluster < fat1.size()){
        if (!corrupted){
            fat1[cluster] = FAT_BAD_CLUSTER;
            corrupted = true;
        }
        cluster = fat1[cluster];
    }
    if (corrupted){
        std::cout << "OK\n";
        save_fs();
        return true;
    }

    std::cout << "Error: Unable to corrupt the file.\n";
    return false;
}

bool filesystem::check(){
    bool found_corrupted_files = false;

    // Lambda to check if a file is corrupted based on its cluster chain
    auto is_file_corrupted = [this](int32_t start_cluster) -> bool {
        int cluster = start_cluster;
        while (cluster != FAT_FILE_END && cluster >= 0 && cluster < fat1.size()) {
            if (fat1[cluster] == FAT_BAD_CLUSTER) {
                return true;
            }
            cluster = fat1[cluster];
        }
        return false;
    };

    // Recursive function to check all files in a directory and its subdirectories
    std::function<void(directory_item*)> check_directory;
    check_directory = [&](directory_item* dir) {
        for (directory_item& item : dir->children) {
            if (item.is_file) {
                if (is_file_corrupted(item.start_cluster)) {
                    std::cout << "Filesystem is corrupted please use command 'format' to format the disk" << "\n";
                    corrupted = true;
                    found_corrupted_files = true;
                }
            } else if (!item.is_file) { // if it's a subdirectory
                check_directory(&item);
            }
        }
    };

    // Start from the root folder
    for (directory_item& item : root_folder) {
        if (item.is_file) {
            if (is_file_corrupted(item.start_cluster)) {
                found_corrupted_files = true;
            }
        } else if (!item.is_file) { // if it's a subdirectory
            check_directory(&item);
        }
    }

    if (!found_corrupted_files) {
        corrupted = false;
        std::cout << "Filesystem is not corrupted\n";
    }

    return found_corrupted_files;
}