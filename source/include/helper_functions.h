#ifndef HELPER_FUNCTIONS_H
#define HELPER_FUNCTIONS_H

#include "../include/odf_types.hpp"
#include "ofs_instance.h"
#include <cstring>
#include <string>
#include <vector>

using namespace std;

inline uint32_t findFreeEntryIndex(OFSInstance* fs, uint32_t max_files = 1000) {
    uint64_t file_entry_offset = fs->header.user_table_offset + 
                                (fs->header.max_users * sizeof(UserInfo));
    
    for (uint32_t i = 2; i < max_files; i++) {
        uint64_t offset = file_entry_offset + (i * sizeof(FileEntry));
        fseek(fs->omni_file, offset, SEEK_SET);
        
        FileEntry entry;
        if (fread(&entry, sizeof(FileEntry), 1, fs->omni_file) == 1) {
            if (entry.name[0] == '\0' || !entry.isValid()) { 
                return i;
            }
        }
    }
    
    return 0; 
}

inline string simple_hash(const string& password) {
    string hash = password;
    for (size_t i = 0; i < hash.length(); i++) {
        hash[i] = hash[i] + 1;
    }
    return hash + "_hash";
}

inline bool isValidPath(const char* path) {
    if (!path || path[0] != '/') {
        return false;
    }
    
    if (strlen(path) == 0 || (strlen(path) == 1 && path[0] == '/')) {
        return true;
    }
    
    for (size_t i = 1; path[i] != '\0'; i++) {
        if (path[i] == '/' && path[i-1] == '/') {
            return false; 
        }
    }
    
    for (size_t i = 1; path[i] != '\0'; i++) {
        if (path[i] == '\0' || path[i] == '\n' || path[i] == '\t') {
            return false;
        }
    }
    
    return true;
}

inline uint32_t getUsableBlockSize(uint32_t block_size) {
    return block_size - 4;
}

inline uint32_t getUsableBlockSize(OFSInstance* fs) {
    return fs->header.block_size - 4;
}

inline uint32_t calculateBlocksNeeded(uint64_t size, uint32_t usable_block_size) {
    if (size == 0) return 1;
    return (size + usable_block_size - 1) / usable_block_size;
}

inline vector<uint32_t> allocateFileBlocks(FreeSpaceManager* free_manager, uint32_t blocks_needed) {
    vector<uint32_t> blocks;
    
    if (blocks_needed == 0) return blocks;
    
    vector<uint32_t> contiguous_blocks = free_manager->allocateBlocks(blocks_needed);
    if (!contiguous_blocks.empty()) {
        return contiguous_blocks;
    }
    
    for (uint32_t i = 0; i < blocks_needed; i++) {
        vector<uint32_t> single_block = free_manager->allocateBlocks(1);
        if (!single_block.empty()) {
            blocks.push_back(single_block[0]);
        } else {
            if (!blocks.empty()) {
                free_manager->freeBlockSegments(blocks);
            }
            return vector<uint32_t>();
        }
    }
    
    return blocks;
}

inline vector<uint32_t> getBlockChain(OFSInstance* fs, uint32_t startBlock) {
    vector<uint32_t> blocks;
    uint32_t current_block = startBlock;
    
    if (startBlock == 0) return blocks;
    
    uint64_t content_offset = fs->header.user_table_offset + 
                             (fs->header.max_users * sizeof(UserInfo)) +
                             (1000 * sizeof(FileEntry));
    
    while (current_block != 0) {
        blocks.push_back(current_block);
        
        uint64_t block_offset = content_offset + (current_block * fs->header.block_size);
        fseek(fs->omni_file, block_offset, SEEK_SET);
        
        uint32_t next_block;
        if (fread(&next_block, sizeof(uint32_t), 1, fs->omni_file) != 1) {
            break;
        }
        current_block = next_block;
    }
    
    return blocks;
}

inline string reconstructPath(OFSInstance* fs, uint32_t entry_index) {
    if (entry_index == 0) {
        return "";
    }
    
    if (entry_index == 1) {
        return "/";
    }
    
    const int MAX_DEPTH = 100;
    string path_components[MAX_DEPTH];
    int component_count = 0;
    uint32_t current_index = entry_index;
    
    uint64_t file_entry_offset = fs->header.user_table_offset + 
                                (fs->header.max_users * sizeof(UserInfo));
    
    while (current_index != 0 && current_index != 1 && component_count < MAX_DEPTH) {
        uint64_t offset = file_entry_offset + (current_index * sizeof(FileEntry));
        fseek(fs->omni_file, offset, SEEK_SET);
        
        FileEntry entry;
        if (fread(&entry, sizeof(FileEntry), 1, fs->omni_file) != 1) {
            return "";
        }
        
        if (!entry.isValid()) {
            return "";
        }
        
        path_components[component_count] = string(entry.name);
        component_count++;
        
        current_index = entry.parent_index;
    }
    
    if (component_count == 0) {
        return "";
    }
    
    if (component_count >= MAX_DEPTH) {
        return "";
    }
    
    string path = "/";
    for (int i = component_count - 1; i >= 0; i--) {
        path += path_components[i];
        if (i > 0) {
            path += "/";
        }
    }
    
    return path;
}

inline uint32_t getParentIndexFromPath(OFSInstance* fs, const string& path) {
    if (path == "/" || path.empty()) {
        return 0;
    }
    
    size_t last_slash = path.find_last_of('/');
    if (last_slash == 0) {
        return 1;
    }
    
    string parent_path = path.substr(0, last_slash);
    
    TreeNode* parent_node = fs->file_tree->findNode(parent_path);
    if (parent_node) {
        return parent_node->entryIndex;
    }
    
    return 0;
}

inline string extractFilename(const string& path) {
    if (path == "/") {
        return "/";
    }
    
    size_t last_slash = path.find_last_of('/');
    if (last_slash == string::npos) {
        return path;
    }
    
    return path.substr(last_slash + 1);
}

inline bool validateParentChain(OFSInstance* fs, uint32_t entry_index) {
    if (entry_index <= 1) {
        return true;
    }
    
    uint32_t current_index = entry_index;
    uint64_t file_entry_offset = fs->header.user_table_offset + 
                                (fs->header.max_users * sizeof(UserInfo));
    
    int depth = 0;
    while (current_index != 1 && current_index != 0) {
        uint64_t offset = file_entry_offset + (current_index * sizeof(FileEntry));
        fseek(fs->omni_file, offset, SEEK_SET);
        
        FileEntry entry;
        if (fread(&entry, sizeof(FileEntry), 1, fs->omni_file) != 1) {
            return false;
        }
        
        if (!entry.isValid()) {
            return false;
        }
        
        current_index = entry.parent_index;
        depth++;
        
        if (depth > 100) {
            return false;
        }
    }
    
    return current_index == 1;
}

inline uint64_t calculateContentOffset(const OMNIHeader& header, uint32_t max_files) {
    uint64_t file_entry_offset = header.user_table_offset + 
                                (header.max_users * sizeof(UserInfo));
    return file_entry_offset + (max_files * sizeof(FileEntry));
}

inline uint32_t calculateTotalBlocks(uint64_t total_size, uint64_t content_offset, uint64_t block_size) {
    uint64_t remaining_space = total_size - content_offset;
    return remaining_space / block_size;
}

#endif