#include "../include/odf_types.hpp"
#include "../include/ofs_instance.h"
#include "../include/session_manager.h"
#include "../include/helper_functions.h"
#include "../include/config_parser.h"
#include <iostream>
#include <cstring>
#include <algorithm>

using namespace std;

extern "C" int file_create(void* session, const char* path, const char* data, size_t size) {
    string* session_str = (string*)session;
    ManagedSession* ms = SessionManager::getSession(*session_str);
    
    if (!ms || !ms->instance) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }
    
    OFSInstance* fs = ms->instance;
    
    if (!path || path[0] != '/') {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
    }
    
    if (fs->file_tree->exists(path)) {
        return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);
    }
    
    uint32_t parent_idx = getParentIndexFromPath(fs, string(path));
    if (parent_idx == 0 && string(path) != "/") {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
    }
    
    uint32_t usable_block_size = fs->header.block_size - 4;
    uint32_t blocks_needed = 0;
    if (size > 0) {
        blocks_needed = (size + usable_block_size - 1) / usable_block_size;
    }
    if (blocks_needed == 0) blocks_needed = 1;
    
    vector<uint32_t> blocks = allocateFileBlocks(fs->free_manager, blocks_needed);
    if (blocks.empty()) {
        return static_cast<int>(OFSErrorCodes::ERROR_NO_SPACE);
    }
    
    TreeNode* node = fs->file_tree->createNode(path, true, ms->info.user.username);
    if (!node) {
        fs->free_manager->freeBlockSegments(blocks);
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
    }
    
    uint32_t next_entry_index = findFreeEntryIndex(fs, fs->config.max_files);
    if (next_entry_index == 0) {
        fs->file_tree->deleteNode(path);
        fs->free_manager->freeBlockSegments(blocks);
        return static_cast<int>(OFSErrorCodes::ERROR_NO_SPACE);
    }
    
    node->entryIndex = next_entry_index;
    node->startBlockIndex = blocks[0];
    node->size = size;
    node->permissions = fs->config.require_auth ? 0644 : 0666;
    node->created_time = time(nullptr);
    node->modified_time = node->created_time;
    
    uint64_t content_offset = calculateContentOffset(fs->header, fs->config.max_files);
    
    size_t written = 0;
    
    for (size_t i = 0; i < blocks.size(); i++) {
        uint64_t block_offset = content_offset + (blocks[i] * fs->header.block_size);
        fseek(fs->omni_file, block_offset, SEEK_SET);
        
        uint32_t next_block = (i < blocks.size() - 1) ? blocks[i + 1] : 0;
        fwrite(&next_block, sizeof(uint32_t), 1, fs->omni_file);
        
        size_t to_write = min(size - written, (size_t)usable_block_size);
        if (to_write > 0 && data) {
            fwrite(data + written, 1, to_write, fs->omni_file);
            written += to_write;
        } else if (to_write > 0) {
            vector<char> zeros(to_write, 0);
            fwrite(zeros.data(), 1, to_write, fs->omni_file);
            written += to_write;
        }
        
        if (to_write < usable_block_size) {
            size_t remaining = usable_block_size - to_write;
            vector<char> zeros(remaining, 0);
            fwrite(zeros.data(), 1, remaining, fs->omni_file);
        }
    }
    
    string filename = extractFilename(string(path));
    if (filename.length() > fs->config.max_filename_length) {
        filename = filename.substr(0, fs->config.max_filename_length);
    }
    
    FileEntry file_entry(filename, EntryType::FILE, node->size, node->permissions,
                        node->owner, node->startBlockIndex, parent_idx);
    
    file_entry.created_time = node->created_time;
    file_entry.modified_time = node->modified_time;
    file_entry.markValid();
    
    uint64_t file_entry_offset = fs->header.user_table_offset + 
                                (fs->header.max_users * sizeof(UserInfo)) +
                                (node->entryIndex * sizeof(FileEntry));
    
    fseek(fs->omni_file, file_entry_offset, SEEK_SET);
    fwrite(&file_entry, sizeof(FileEntry), 1, fs->omni_file);
    fflush(fs->omni_file);
    
    fs->total_files++;
    
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

extern "C" int file_read(void* session, const char* path, char** buffer, size_t* size) {
    string* session_str = (string*)session;
    ManagedSession* ms = SessionManager::getSession(*session_str);
    
    if (!ms || !ms->instance) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }
    
    OFSInstance* fs = ms->instance;
    
    TreeNode* node = fs->file_tree->findNode(path);
    if (!node || !node->isFile) {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }
    
    if (fs->config.require_auth && (node->permissions & 0444) == 0) {
        if (strcmp(node->owner.c_str(), ms->info.user.username) != 0 && 
            ms->info.user.role != UserRole::ADMIN) {
            return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
        }
    }
    
    *size = node->size;
    if (*size == 0) {
        *buffer = new char[1];
        (*buffer)[0] = '\0';
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    *buffer = new char[*size + 1];
    (*buffer)[*size] = '\0';
    
    uint64_t content_offset = calculateContentOffset(fs->header, fs->config.max_files);
    
    uint32_t current_block = node->startBlockIndex;
    size_t read_so_far = 0;
    uint32_t usable_block_size = fs->header.block_size - 4;
    
    while (current_block != 0 && read_so_far < *size) {
        uint64_t block_offset = content_offset + (current_block * fs->header.block_size);
        fseek(fs->omni_file, block_offset, SEEK_SET);
        
        uint32_t next_block;
        fread(&next_block, sizeof(uint32_t), 1, fs->omni_file);
        
        size_t to_read = min(*size - read_so_far, (size_t)usable_block_size);
        size_t bytes_read = fread(*buffer + read_so_far, 1, to_read, fs->omni_file);
        read_so_far += bytes_read;
        
        current_block = next_block;
        
        if (bytes_read < to_read) {
            break;
        }
    }
    
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

extern "C" int file_delete(void* session, const char* path) {
    string* session_str = (string*)session;
    ManagedSession* ms = SessionManager::getSession(*session_str);
    
    if (!ms || !ms->instance) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }
    
    OFSInstance* fs = ms->instance;
    
    TreeNode* node = fs->file_tree->findNode(path);
    if (!node || !node->isFile) {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }
    
    if (fs->config.require_auth && strcmp(node->owner.c_str(), ms->info.user.username) != 0 && 
        ms->info.user.role != UserRole::ADMIN) {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }
    
    vector<uint32_t> blocks_to_free;
    uint32_t current_block = node->startBlockIndex;
    
    uint64_t content_offset = calculateContentOffset(fs->header, fs->config.max_files);
    
    while (current_block != 0) {
        blocks_to_free.push_back(current_block);
        
        uint64_t block_offset = content_offset + (current_block * fs->header.block_size);
        fseek(fs->omni_file, block_offset, SEEK_SET);
        
        uint32_t next_block;
        if (fread(&next_block, sizeof(uint32_t), 1, fs->omni_file) != 1) {
            break;
        }
        current_block = next_block;
    }
    
    if (!blocks_to_free.empty()) {
        fs->free_manager->freeBlockSegments(blocks_to_free);
    }
    
    uint64_t file_entry_offset = fs->header.user_table_offset + 
                                (fs->header.max_users * sizeof(UserInfo)) +
                                (node->entryIndex * sizeof(FileEntry));
    
    FileEntry entry;
    fseek(fs->omni_file, file_entry_offset, SEEK_SET);
    fread(&entry, sizeof(FileEntry), 1, fs->omni_file);
    
    entry.markInvalid();
    
    fseek(fs->omni_file, file_entry_offset, SEEK_SET);
    fwrite(&entry, sizeof(FileEntry), 1, fs->omni_file);
    fflush(fs->omni_file);
    
    if (fs->file_tree->deleteNode(path)) {
        fs->total_files--;
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
}

extern "C" int file_exists(void* session, const char* path) {
    string* session_str = (string*)session;
    ManagedSession* ms = SessionManager::getSession(*session_str);
    
    if (!ms || !ms->instance) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }
    
    OFSInstance* fs = ms->instance;
    
    if (fs->file_tree->isFile(path)) {
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
}

extern "C" int file_rename(void* session, const char* old_path, const char* new_path) {
    string* session_str = (string*)session;
    ManagedSession* ms = SessionManager::getSession(*session_str);
    
    if (!ms || !ms->instance) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }
    
    OFSInstance* fs = ms->instance;
    
    TreeNode* node = fs->file_tree->findNode(old_path);
    if (!node || !node->isFile) {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }
    
    if (fs->config.require_auth && strcmp(node->owner.c_str(), ms->info.user.username) != 0 && 
        ms->info.user.role != UserRole::ADMIN) {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }
    
    if (fs->file_tree->exists(new_path)) {
        return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);
    }
    
    uint32_t new_parent_idx = getParentIndexFromPath(fs, string(new_path));
    if (new_parent_idx == 0 && string(new_path) != "/") {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
    }
    
    uint64_t file_entry_offset = fs->header.user_table_offset + 
                                (fs->header.max_users * sizeof(UserInfo)) +
                                (node->entryIndex * sizeof(FileEntry));
    
    FileEntry file_entry;
    fseek(fs->omni_file, file_entry_offset, SEEK_SET);
    fread(&file_entry, sizeof(FileEntry), 1, fs->omni_file);
    
    string new_name = extractFilename(string(new_path));
    if (new_name.length() > fs->config.max_filename_length) {
        new_name = new_name.substr(0, fs->config.max_filename_length);
    }
    
    strncpy(file_entry.name, new_name.c_str(), sizeof(file_entry.name) - 1);
    file_entry.name[sizeof(file_entry.name) - 1] = '\0';
    file_entry.parent_index = new_parent_idx;
    file_entry.modified_time = time(nullptr);
    
    fseek(fs->omni_file, file_entry_offset, SEEK_SET);
    fwrite(&file_entry, sizeof(FileEntry), 1, fs->omni_file);
    fflush(fs->omni_file);
    
    if (fs->file_tree->rename(old_path, new_path)) {
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
}

extern "C" int file_edit(void* session, const char* path, const char* data, size_t size, uint32_t index) {
    string* session_str = (string*)session;
    ManagedSession* ms = SessionManager::getSession(*session_str);
    
    if (!ms || !ms->instance) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }
    
    OFSInstance* fs = ms->instance;
    
    TreeNode* node = fs->file_tree->findNode(path);
    if (!node || !node->isFile) {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }
    
    if (fs->config.require_auth && strcmp(node->owner.c_str(), ms->info.user.username) != 0 && 
        ms->info.user.role != UserRole::ADMIN) {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }
    
    if (index > node->size) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
    }
    
    uint32_t usable_block_size = fs->header.block_size - 4;
    uint64_t content_offset = calculateContentOffset(fs->header, fs->config.max_files);
    
    uint64_t new_size = index + size;
    bool needs_expansion = (new_size > node->size);
    
    if (needs_expansion) {
        uint32_t current_blocks = (node->size + usable_block_size - 1) / usable_block_size;
        uint32_t needed_blocks = (new_size + usable_block_size - 1) / usable_block_size;
        uint32_t additional_blocks = needed_blocks - current_blocks;
        
        if (additional_blocks > 0) {
            vector<uint32_t> current_block_chain;
            uint32_t current_block = node->startBlockIndex;
            
            while (current_block != 0) {
                current_block_chain.push_back(current_block);
                uint64_t block_offset = content_offset + (current_block * fs->header.block_size);
                fseek(fs->omni_file, block_offset, SEEK_SET);
                uint32_t next_block;
                fread(&next_block, sizeof(uint32_t), 1, fs->omni_file);
                current_block = next_block;
            }
            
            vector<uint32_t> new_blocks;
            for (uint32_t i = 0; i < additional_blocks; i++) {
                vector<uint32_t> single_block = fs->free_manager->allocateBlocks(1);
                if (single_block.empty()) {
                    if (!new_blocks.empty()) {
                        fs->free_manager->freeBlockSegments(new_blocks);
                    }
                    return static_cast<int>(OFSErrorCodes::ERROR_NO_SPACE);
                }
                new_blocks.push_back(single_block[0]);
            }
            
            if (!current_block_chain.empty()) {
                uint32_t last_existing_block = current_block_chain.back();
                uint64_t last_block_offset = content_offset + (last_existing_block * fs->header.block_size);
                fseek(fs->omni_file, last_block_offset, SEEK_SET);
                fwrite(&new_blocks[0], sizeof(uint32_t), 1, fs->omni_file);
            }
            
            for (size_t i = 0; i < new_blocks.size(); i++) {
                uint64_t new_block_offset = content_offset + (new_blocks[i] * fs->header.block_size);
                fseek(fs->omni_file, new_block_offset, SEEK_SET);
                
                uint32_t next_ptr = (i < new_blocks.size() - 1) ? new_blocks[i + 1] : 0;
                fwrite(&next_ptr, sizeof(uint32_t), 1, fs->omni_file);
                
                vector<char> zeros(usable_block_size, 0);
                fwrite(zeros.data(), 1, usable_block_size, fs->omni_file);
            }
        }
        
        node->size = new_size;
    }
    
    uint32_t block_index = index / usable_block_size;
    uint32_t offset_in_block = index % usable_block_size;
    
    uint32_t current_block = node->startBlockIndex;
    for (uint32_t i = 0; i < block_index && current_block != 0; i++) {
        uint64_t block_offset = content_offset + (current_block * fs->header.block_size);
        fseek(fs->omni_file, block_offset, SEEK_SET);
        uint32_t next_block;
        fread(&next_block, sizeof(uint32_t), 1, fs->omni_file);
        current_block = next_block;
    }
    
    if (current_block == 0) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
    }
    
    size_t written = 0;
    while (written < size && current_block != 0) {
        uint64_t block_offset = content_offset + (current_block * fs->header.block_size);
        fseek(fs->omni_file, block_offset + 4 + offset_in_block, SEEK_SET);
        
        size_t to_write = min(size - written, 
                             (size_t)(usable_block_size - offset_in_block));
        
        if (to_write > 0) {
            fwrite(data + written, 1, to_write, fs->omni_file);
            written += to_write;
        }
        
        offset_in_block = 0;
        
        fseek(fs->omni_file, block_offset, SEEK_SET);
        uint32_t next_block;
        fread(&next_block, sizeof(uint32_t), 1, fs->omni_file);
        current_block = next_block;
    }
    
    if (needs_expansion) {
        uint64_t file_entry_offset = fs->header.user_table_offset + 
                                    (fs->header.max_users * sizeof(UserInfo)) +
                                    (node->entryIndex * sizeof(FileEntry));
        
        FileEntry file_entry;
        fseek(fs->omni_file, file_entry_offset, SEEK_SET);
        fread(&file_entry, sizeof(FileEntry), 1, fs->omni_file);
        
        file_entry.size = node->size;
        file_entry.modified_time = time(nullptr);
        
        fseek(fs->omni_file, file_entry_offset, SEEK_SET);
        fwrite(&file_entry, sizeof(FileEntry), 1, fs->omni_file);
        fflush(fs->omni_file);
    }
    
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

extern "C" int file_truncate(void* session, const char* path) {
    string* session_str = (string*)session;
    ManagedSession* ms = SessionManager::getSession(*session_str);
    
    if (!ms || !ms->instance) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }
    
    OFSInstance* fs = ms->instance;
    
    TreeNode* node = fs->file_tree->findNode(path);
    if (!node || !node->isFile) {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }
    
    if (fs->config.require_auth && strcmp(node->owner.c_str(), ms->info.user.username) != 0 && 
        ms->info.user.role != UserRole::ADMIN) {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }
    
    const char* text = "siruamr";
    size_t text_len = strlen(text);
    
    uint64_t content_offset = calculateContentOffset(fs->header, fs->config.max_files);
    
    uint32_t current_block = node->startBlockIndex;
    size_t written = 0;
    size_t total_to_write = node->size;
    uint32_t usable_block_size = fs->header.block_size - 4;
    
    while (current_block != 0 && written < total_to_write) {
        uint64_t block_offset = content_offset + (current_block * fs->header.block_size);
        fseek(fs->omni_file, block_offset, SEEK_SET);
        
        uint32_t next_block;
        fread(&next_block, sizeof(uint32_t), 1, fs->omni_file);
        
        char* block_data = new char[usable_block_size];
        size_t bytes_to_write = min((size_t)usable_block_size, (size_t)(total_to_write - written));
        
        for (size_t i = 0; i < bytes_to_write; i++) {
            block_data[i] = text[written % text_len];
            written++;
        }
        
        fseek(fs->omni_file, block_offset + 4, SEEK_SET);
        fwrite(block_data, 1, bytes_to_write, fs->omni_file);
        
        delete[] block_data;
        current_block = next_block;
    }
    
    fflush(fs->omni_file);
    
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}