#include "../include/odf_types.hpp"
#include "../include/ofs_instance.h"
#include "../include/session_manager.h"
#include <iostream>
#include <cstring>

// get metadata for the file
extern "C" int get_metadata(void* session, const char* path, FileMetadata* meta) {
    string* session_str = (string*)session;
    ManagedSession* ms = SessionManager::getSession(*session_str);
    
    if (!ms || !ms->instance) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }
    
    OFSInstance* fs = ms->instance;
    
    // finds the node
    TreeNode* node = fs->file_tree->findNode(path);
    if (!node) {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }
    
    strncpy(meta->path, path, sizeof(meta->path) - 1);
    meta->path[sizeof(meta->path) - 1] = '\0';
    
    // Fill entry info
    strncpy(meta->entry.name, node->name.c_str(), sizeof(meta->entry.name) - 1);
    meta->entry.name[sizeof(meta->entry.name) - 1] = '\0';
    
    // set type to FILE/DIRECTORY
    if (node->isFile) {
        meta->entry.type = static_cast<uint8_t>(EntryType::FILE);
    } 
    else {
        meta->entry.type = static_cast<uint8_t>(EntryType::DIRECTORY);
    }
    meta->entry.size = node->size;
    meta->entry.permissions = node->permissions;
    meta->entry.inode = node->entryIndex;
    
    strncpy(meta->entry.owner, node->owner.c_str(), sizeof(meta->entry.owner) - 1);
    meta->entry.owner[sizeof(meta->entry.owner) - 1] = '\0';
    
    meta->entry.created_time = node->created_time;
    meta->entry.modified_time = node->modified_time;
    
    // calculate blocks used
    uint32_t usable_block_size = fs->header.block_size - 4;
    if (node->isFile && node->size > 0) {
        meta->blocks_used = (node->size + usable_block_size - 1) / usable_block_size;
        meta->actual_size = meta->blocks_used * fs->header.block_size;
    } else {
        meta->blocks_used = 0;
        meta->actual_size = 0;
    }
    
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

// set permissoins
extern "C" int set_permissions(void* session, const char* path, uint32_t permissions) {
    string* session_str = (string*)session;
    ManagedSession* ms = SessionManager::getSession(*session_str);
    
    if (!ms || !ms->instance) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }
    
    OFSInstance* fs = ms->instance;
    
    // find node
    TreeNode* node = fs->file_tree->findNode(path);
    if (!node) {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }
    
    // check for permissions
    if (strcmp(node->owner.c_str(), ms->info.user.username) != 0 && 
        ms->info.user.role != UserRole::ADMIN) {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }
    
    // update permissions
    node->permissions = permissions;
    
    // Update FileEntry on disk
    uint64_t file_entry_offset = fs->header.user_table_offset + 
                                (fs->header.max_users * sizeof(UserInfo)) +
                                (node->entryIndex * sizeof(FileEntry));
    
    FileEntry file_entry;
    fseek(fs->omni_file, file_entry_offset, SEEK_SET);
    fread(&file_entry, sizeof(FileEntry), 1, fs->omni_file);
    
    file_entry.permissions = permissions;
    file_entry.modified_time = time(nullptr);
    
    fseek(fs->omni_file, file_entry_offset, SEEK_SET);
    fwrite(&file_entry, sizeof(FileEntry), 1, fs->omni_file);
    fflush(fs->omni_file);
    
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

// gets the stats for session
extern "C" int get_stats(void* session, FSStats* stats) {
    string* session_str = (string*)session;
    ManagedSession* ms = SessionManager::getSession(*session_str);
    
    if (!ms || !ms->instance) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }
    
    OFSInstance* fs = ms->instance;
    
    // calculate stats
    stats->total_size = fs->header.total_size;
    
    uint64_t used_blocks = fs->free_manager->getUsedBlocks();
    stats->used_space = used_blocks * fs->header.block_size;
    stats->free_space = fs->free_manager->getFreeBlocks() * fs->header.block_size;
    
    stats->total_files = fs->total_files;
    stats->total_directories = fs->total_directories;
    stats->total_users = fs->users.size();
    stats->active_sessions = SessionManager::getSessionCount();
    stats->fragmentation = fs->free_manager->getFragmentation();
    
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

extern "C" void free_buffer(void* buffer) {
    if (buffer) {
        delete[] (char*)buffer;
    }
}

extern "C" const char* get_error_message(int error_code) {
    switch (static_cast<OFSErrorCodes>(error_code)) {
        case OFSErrorCodes::SUCCESS:
            return "Operation completed successfully";
        case OFSErrorCodes::ERROR_NOT_FOUND:
            return "File, directory, or user not found";
        case OFSErrorCodes::ERROR_PERMISSION_DENIED:
            return "Permission denied - insufficient privileges";
        case OFSErrorCodes::ERROR_IO_ERROR:
            return "Input/output error occurred";
        case OFSErrorCodes::ERROR_INVALID_PATH:
            return "Invalid path format";
        case OFSErrorCodes::ERROR_FILE_EXISTS:
            return "File or directory already exists";
        case OFSErrorCodes::ERROR_NO_SPACE:
            return "Insufficient space in file system";
        case OFSErrorCodes::ERROR_INVALID_CONFIG:
            return "Invalid configuration file";
        case OFSErrorCodes::ERROR_NOT_IMPLEMENTED:
            return "Feature not yet implemented";
        case OFSErrorCodes::ERROR_INVALID_SESSION:
            return "Invalid or expired session";
        case OFSErrorCodes::ERROR_DIRECTORY_NOT_EMPTY:
            return "Directory is not empty";
        case OFSErrorCodes::ERROR_INVALID_OPERATION:
            return "Invalid operation";
        default:
            return "Unknown error";
    }
}