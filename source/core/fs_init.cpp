#include "../include/odf_types.hpp"
#include "../include/ofs_instance.h"
#include "../data_structures/free_space_manager.h"
#include "../include/helper_functions.h"
#include "../include/config_parser.h"
#include "../include/session_manager.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <filesystem>

using namespace std;

inline bool fileExists(const char* path) {
    return filesystem::exists(path);
}

int createNewFileSystem(const char* omni_path, const FileSystemConfig& config) {
    const uint64_t TOTAL_SIZE = config.total_size;
    const uint64_t BLOCK_SIZE = config.block_size;
    const uint32_t MAX_USERS = config.max_users;
    const uint32_t MAX_FILES = config.max_files;
    
    ofstream file(omni_path, ios::binary);
    if (!file) {
        return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
    }
    
    // Calculate offsets
    uint64_t user_table_offset = sizeof(OMNIHeader);
    uint64_t file_entry_offset = user_table_offset + (MAX_USERS * sizeof(UserInfo));
    uint64_t content_offset = file_entry_offset + (MAX_FILES * sizeof(FileEntry));
    uint64_t remaining_space = TOTAL_SIZE - content_offset;
    uint32_t total_content_blocks = remaining_space / BLOCK_SIZE;
    
    // Write Header
    OMNIHeader header;
    memset(&header, 0, sizeof(OMNIHeader));
    
    const char* magic = "OMNIFS01";
    memcpy(header.magic, magic, 8);
    
    header.format_version = 0x00010000;
    header.total_size = TOTAL_SIZE;
    header.header_size = sizeof(OMNIHeader);
    header.block_size = BLOCK_SIZE;
    
    strncpy(header.student_id, "bscs24043", sizeof(header.student_id) - 1);
    
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    char date_buffer[16];
    strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d", tm_info);
    strncpy(header.submission_date, date_buffer, sizeof(header.submission_date) - 1);
    
    header.user_table_offset = user_table_offset;
    header.max_users = MAX_USERS;
    
    file.write(reinterpret_cast<const char*>(&header), sizeof(OMNIHeader));
    
    string admin_hash = simple_hash(config.admin_password);
    UserInfo admin_user(config.admin_username.c_str(), admin_hash, UserRole::ADMIN, time(nullptr));
    file.write(reinterpret_cast<const char*>(&admin_user), sizeof(UserInfo));
    
    UserInfo empty_user;
    memset(&empty_user, 0, sizeof(UserInfo));
    for (uint32_t i = 1; i < MAX_USERS; i++) {
        file.write(reinterpret_cast<const char*>(&empty_user), sizeof(UserInfo));
    }
    
    FileEntry reserved_entry;
    memset(&reserved_entry, 0, sizeof(FileEntry));
    file.write(reinterpret_cast<const char*>(&reserved_entry), sizeof(FileEntry));
    
    FileEntry root_entry;
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
    
    FileEntry empty_entry;
    memset(&empty_entry, 0, sizeof(FileEntry));
    for (uint32_t i = 2; i < MAX_FILES; i++) {
        file.write(reinterpret_cast<const char*>(&empty_entry), sizeof(FileEntry));
    }
    
    vector<uint8_t> zero_block(BLOCK_SIZE, 0);
    for (uint32_t i = 0; i < total_content_blocks; i++) {
        file.write(reinterpret_cast<const char*>(zero_block.data()), BLOCK_SIZE);
    }
    
    file.close();
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

extern "C" int fs_init(void** instance, const char* omni_path, const char* config_path) {
    cout << "OMNI file: " << omni_path << endl;
    cout << "Config file: " << config_path << endl;
    
    FileSystemConfig config = ConfigParser::parse(config_path);
    
    SessionManager::initialize(config);
    
    if (!fileExists(omni_path)) {
        int result = createNewFileSystem(omni_path, config);
        if (result != static_cast<int>(OFSErrorCodes::SUCCESS)) {
            return result;
        }
    } 
    
    OFSInstance* fs = new OFSInstance();
    fs->config = config;  
    
    // Open file
    fs->omni_file = fopen(omni_path, "r+b");
    if (!fs->omni_file) {
        delete fs;
        return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
    }
    
    // Read header
    if (fread(&fs->header, sizeof(OMNIHeader), 1, fs->omni_file) != 1) {
        fclose(fs->omni_file);
        delete fs;
        return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
    }
    
    // compare magic numbers
    if (strncmp(fs->header.magic, "OMNIFS01", 8) != 0) {
        fclose(fs->omni_file);
        delete fs;
        return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
    }
    
    // loading users
    fseek(fs->omni_file, fs->header.user_table_offset, SEEK_SET);
    
    int user_count = 0;
    for (uint32_t i = 0; i < fs->header.max_users; i++) {
        UserInfo user;
        if (fread(&user, sizeof(UserInfo), 1, fs->omni_file) == 1) {
            if (user.is_active && user.username[0] != '\0') {
                fs->users.insert(user.username, user);
                user_count++;
                cout << "   User: " << user.username;
                cout << " (";
                if (user.role == UserRole::ADMIN){
                    cout << "Admin";
                }
                else{
                    cout << "Normal";
                }
                cout << ")" << endl;
            }
        }
    }
    
    // Initialize file tree
    fs->file_tree = new FileTree();
    fs->total_directories = 1;
    fs->total_files = 0;
    
    uint64_t file_entry_offset = fs->header.user_table_offset + 
                                (fs->header.max_users * sizeof(UserInfo));
    
    const uint32_t MAX_ENTRIES = config.max_files;
    
    // Load entries
    FileEntry* entries = new FileEntry[MAX_ENTRIES];
    bool* entry_valid = new bool[MAX_ENTRIES];
    bool* entry_processed = new bool[MAX_ENTRIES];
    
    for (uint32_t i = 0; i < MAX_ENTRIES; i++) {
        entry_valid[i] = false;
        entry_processed[i] = false;
    }
    
    fseek(fs->omni_file, file_entry_offset, SEEK_SET);
    int valid_count = 0;
    for (uint32_t i = 0; i < MAX_ENTRIES; i++) {
        if (fread(&entries[i], sizeof(FileEntry), 1, fs->omni_file) == 1) {
            if (entries[i].isValid() && entries[i].name[0] != '\0') {
                entry_valid[i] = true;
                valid_count++;
            }
        }
    }
        
    entry_processed[0] = true;
    entry_processed[1] = true;
    
    bool progress = true;
    int pass = 0;
    int total_processed = 2;
    
    while (progress && total_processed < valid_count + 2) {
        progress = false;
        pass++;
        
        for (uint32_t entry_idx = 2; entry_idx < MAX_ENTRIES; entry_idx++) {
            if (!entry_valid[entry_idx] || entry_processed[entry_idx]) {
                continue;
            }
            
            const FileEntry& entry = entries[entry_idx];
            
            if (entry.parent_index == 1 || entry_processed[entry.parent_index]) {
                string path = reconstructPath(fs, entry_idx);
                
                if (path.empty()) {
                    continue;
                }
                
                bool is_file = (entry.getType() == EntryType::FILE);
                TreeNode* node = fs->file_tree->createNode(path, is_file, string(entry.owner));
                
                if (node) {
                    node->entryIndex = entry_idx;
                    node->size = entry.size;
                    node->permissions = entry.permissions;
                    node->created_time = entry.created_time;
                    node->modified_time = entry.modified_time;
                    
                    if (is_file) {
                        node->startBlockIndex = entry.inode;
                        fs->total_files++;
                    } else {
                        node->startBlockIndex = 0;
                        fs->total_directories++;
                    }
                    
                    entry_processed[entry_idx] = true;
                    total_processed++;
                    progress = true;
                }
            }
        }
    }
    
    delete[] entries;
    delete[] entry_valid;
    delete[] entry_processed;
        
    uint64_t content_offset = calculateContentOffset(fs->header, config.max_files);
    uint32_t total_blocks = calculateTotalBlocks(fs->header.total_size, content_offset, fs->header.block_size);
    uint64_t free_space_offset = content_offset + (total_blocks * fs->header.block_size);
    
    fseek(fs->omni_file, free_space_offset, SEEK_SET);
    
    uint8_t free_space_header[12];
    if (fread(free_space_header, 1, 12, fs->omni_file) == 12) {
        uint32_t seg_count = ((uint32_t)free_space_header[8] << 24) |
                            ((uint32_t)free_space_header[9] << 16) |
                            ((uint32_t)free_space_header[10] << 8) |
                            (uint32_t)free_space_header[11];
        
        size_t data_size = 12 + (seg_count * 8);
        vector<uint8_t> free_space_data(data_size);
        
        for (int i = 0; i < 12; i++) {
            free_space_data[i] = free_space_header[i];
        }
        
        if (seg_count > 0) {
            size_t remaining = seg_count * 8;
            for (size_t i = 0; i < remaining; i++) {
                uint8_t byte;
                if (fread(&byte, 1, 1, fs->omni_file) == 1) {
                    free_space_data[12 + i] = byte;
                }
            }
        }
        
        fs->free_manager = FreeSpaceManager::deserialize(free_space_data);
        
        if (!fs->free_manager) {
            fs->free_manager = new FreeSpaceManager(total_blocks);
        }
    } else {
        fs->free_manager = new FreeSpaceManager(total_blocks);
    }
    

    SessionManager::setInstance(fs);
    
    *instance = fs;
    
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

extern "C" int fs_shutdown(void* instance) {
    
    if (instance) {
        OFSInstance* fs = (OFSInstance*)instance;
        
        if (fs->free_manager && fs->omni_file) {
            uint64_t content_offset = calculateContentOffset(fs->header, fs->config.max_files);
            uint32_t total_blocks = calculateTotalBlocks(fs->header.total_size, content_offset, 
                                                        fs->header.block_size);
            uint64_t free_space_offset = content_offset + (total_blocks * fs->header.block_size);
            
            vector<uint8_t> free_space_data = fs->free_manager->serialize();
            fseek(fs->omni_file, free_space_offset, SEEK_SET);
            fwrite(free_space_data.data(), 1, free_space_data.size(), fs->omni_file);
            fflush(fs->omni_file);
            
        }
        
        // Clear sessions
        SessionManager::clearAll();
        SessionManager::setInstance(nullptr);
        SessionManager::cleanup();
        
        delete fs;
    }
    
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}