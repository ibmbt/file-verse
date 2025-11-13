#include "../include/odf_types.hpp"
#include "../data_structures/free_space_manager.h"
#include "../include/helper_functions.h"
#include "../include/config_parser.h"
#include <fstream>
#include <cstring>
#include <ctime>
#include <iostream>
#include <filesystem>

using namespace std;

// check if file exist 
inline bool fileExists(const char* path) {
    return filesystem::exists(path);
}

extern "C" int fs_format(const char* omni_path, const char* config_path) {
    
    // parse config
    FileSystemConfig config = ConfigParser::parse(config_path);
    
    if (!fileExists(omni_path)) {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }
    
    string path_str(omni_path);
    if (path_str.length() < 5 || path_str.substr(path_str.length() - 5) != ".omni") {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
    }
    
    const uint64_t TOTAL_SIZE = config.total_size;
    const uint64_t BLOCK_SIZE = config.block_size;
    const uint32_t MAX_USERS = config.max_users;
    const uint32_t MAX_FILES = config.max_files;
    
    ofstream file(omni_path, ios::binary | ios::trunc);
    if (!file) {
        return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
    }
    
    // calculating offsets
    uint64_t header_offset = 0;
    uint64_t user_table_offset = sizeof(OMNIHeader);
    uint64_t file_entry_offset = user_table_offset + (MAX_USERS * sizeof(UserInfo));
    uint64_t content_offset = file_entry_offset + (MAX_FILES * sizeof(FileEntry));
    uint64_t remaining_space = TOTAL_SIZE - content_offset;
    uint32_t total_content_blocks = remaining_space / BLOCK_SIZE;

    OMNIHeader header{};
    memset(&header, 0, sizeof(OMNIHeader));
    
    const char* magic = "OMNIFS01";
    memcpy(header.magic, magic, 8);
    
    header.format_version = 0x00010000;
    header.total_size = TOTAL_SIZE;
    header.header_size = sizeof(OMNIHeader);
    header.block_size = BLOCK_SIZE;
    strncpy(header.student_id, "bscs24043", sizeof(header.student_id) - 1);
    
    time_t now = time(nullptr);
    struct tm* time_info = localtime(&now);
    char date_buffer[16];
    strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d", time_info);
    strncpy(header.submission_date, date_buffer, sizeof(header.submission_date) - 1);
    
    header.user_table_offset = user_table_offset;
    header.max_users = MAX_USERS;
    
    file.write(reinterpret_cast<const char*>(&header), sizeof(OMNIHeader));
    
    string admin_hash = simple_hash(config.admin_password);
    UserInfo admin_user(config.admin_username.c_str(), admin_hash, UserRole::ADMIN, time(nullptr));
    file.write(reinterpret_cast<const char*>(&admin_user), sizeof(UserInfo));

    // rest of users empty
    UserInfo empty_user{};
    memset(&empty_user, 0, sizeof(UserInfo));
    for (uint32_t i = 1; i < MAX_USERS; i++) {
        file.write(reinterpret_cast<const char*>(&empty_user), sizeof(UserInfo));
    }
    
    // first entry reserved (dummy)
    FileEntry reserved_entry{};
    memset(&reserved_entry, 0, sizeof(FileEntry));
    file.write(reinterpret_cast<const char*>(&reserved_entry), sizeof(FileEntry));
    
    // root dir
    FileEntry root_entry{};
    memset(&root_entry, 0, sizeof(FileEntry));
    root_entry.setType(EntryType::DIRECTORY);
    strncpy(root_entry.name, "/", sizeof(root_entry.name) - 1);
    strncpy(root_entry.owner, config.admin_username.c_str(), sizeof(root_entry.owner) - 1);
    root_entry.permissions = 0755;
    root_entry.created_time = time(nullptr);
    root_entry.modified_time = root_entry.created_time;
    root_entry.inode = 1;
    root_entry.markValid();
    file.write(reinterpret_cast<const char*>(&root_entry), sizeof(FileEntry));
    
    // empty file entries 
    FileEntry empty_entry{};
    memset(&empty_entry, 0, sizeof(FileEntry));
    for (uint32_t i = 2; i < MAX_FILES; i++) {
        file.write(reinterpret_cast<const char*>(&empty_entry), sizeof(FileEntry));
    }
    
    // init content bloks
    uint64_t current_pos = file.tellp();
    if (current_pos != content_offset) {
        file.close();
        return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
    }
    
    vector<uint8_t> zero_block(BLOCK_SIZE, 0);
    for (uint32_t i = 0; i < total_content_blocks; i++) {
        file.write(reinterpret_cast<const char*>(zero_block.data()), BLOCK_SIZE);
    }
        
    FreeSpaceManager* free_manager = new FreeSpaceManager(total_content_blocks);
    vector<uint8_t> free_space_data = free_manager->serialize();
    file.write(reinterpret_cast<const char*>(free_space_data.data()), free_space_data.size());
    
    delete free_manager;
    file.close();
    
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}
