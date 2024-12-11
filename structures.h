#ifndef STRUCTURES_H
#define STRUCTURES_H
#include <vector>
#include <cstring>
#include <sstream>
#include <cstdint>

extern const int32_t FAT_UNUSED;
extern const int32_t FAT_FILE_END;
extern const int32_t FAT_BAD_CLUSTER;

extern const int32_t CLUSTER_SIZE;
extern const int32_t DISK_SIZE;

// Description structure
struct description{
    char signature[9];
    int32_t disk_size;
    int32_t cluster_size;
    int32_t cluster_count;
    int32_t fat_count;
    int32_t fat1_start_address;
    int32_t fat2_start_address;
    int32_t data_start_address;
    int32_t directory_start_address; // New field for directory metadata start
};

// Directory item structure
struct directory_item{
    char item_name[12]; // 8 chars for name + 3 for extension + 1 for null terminator
    bool is_file;
    int32_t size;
    int32_t start_cluster;
    int32_t parent_id;
    int32_t id;
    std::vector<directory_item> children;

    // Constructor
    directory_item(const std::string &name = "", bool is_file = false)
        : is_file(is_file), size(0), start_cluster(-1), parent_id(-1), id(-1){
        std::memset(item_name, 0, sizeof(item_name));
        if (name.length() >= sizeof(item_name)){
            std::strncpy(item_name, name.c_str(), sizeof(item_name) - 1);
        }
        else{
            std::strcpy(item_name, name.c_str());
        }
    }
};
#endif //STRUCTURES_H
