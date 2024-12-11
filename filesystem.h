#ifndef FileSystem_H
#define FileSystem_H


#include <vector>
#include <fstream>
#include "structures.h"
#include <cstdint>

class filesystem{
public:
    description desc;
    std::vector<int32_t> fat1;
    std::vector<int32_t> fat2;
    std::vector<directory_item> root_folder;
    std::string file_name;
    int32_t next_dir_id;
    directory_item *current_directory;
    bool corrupted = false;

    int32_t parse_size(const std::string& size_str);
    filesystem(const std::string &file_name);
    bool format_fs(const std::string &sizeStr);
    void update_dir_id();
    std::string current_file_path(directory_item *dir);
    void save_fs();
    void load_fs();
    directory_item *find_dir_by_given_id(int32_t id, directory_item *dir);
    void save_directory(std::ofstream &out, const directory_item &dir);
    void load_dir(std::ifstream &in, directory_item &dir);
    std::string trim_spaces(const std::string &input);
    bool make_directory(directory_item* current_dir, const std::string& dir_name);
    void list_directory(directory_item* dir);
    bool directory_exists(directory_item* current_dir, const std::string& dir_name);
    directory_item* change_directory(directory_item* current_dir, const std::string& path);
    directory_item* find_directory_by_path(directory_item* start_dir, const std::string& path);
    directory_item* get_parent_directory(const std::string& path, directory_item* current_dir, std::string& child_name);
    bool remove_directory(directory_item* current_dir, const std::string& path);
    std::string print_working_directory(directory_item* dir);
    bool remove_file(directory_item* current_dir, const std::string& path);
    bool copy_file_in(const std::string& source_path, const std::string& dest_path);
    bool copy_file_out(const std::string& source_path, const std::string& dest_path);
    std::string get_file_info(const std::string& path);
    bool cat_file(const std::string& path);
    bool copy_file_to_fs(const std::string& source_path, directory_item* current_dir, const std::string& dest_path, std::vector<int32_t>& fat, int32_t& cluster_count);
    bool copy_file_from_fs(directory_item* current_dir, const std::string& source_path, const std::string& dest_path, const std::vector<int32_t>& fat);
    std::string get_file_clusters(directory_item* current_dir, const std::string& path, const std::vector<int32_t>& fat);
    bool read_file_content(directory_item* current_dir, const std::string& path);
    std::vector<int32_t> get_cluster_chain(int32_t start_cluster, const std::vector<int32_t>& fat);
    bool copy_file(const std::string& source_path, const std::string& dest_path);
    bool move_file(const std::string& source_path, const std::string& dest_path);
    int allocate_cluster();
    bool load(directory_item* current_dir, const std::string &filePath);
    bool bug(const std::string &filePath);
    bool check();
};

#endif