#ifndef OFS_INSTANCE_HPP
#define OFS_INSTANCE_HPP

#include "../include/odf_types.hpp"
#include "../include/config_parser.h"
#include "../data_structures/avl_tree.h"
#include "../data_structures/file_tree.h"
#include "../data_structures/free_space_manager.h"

struct OFSInstance {
    FILE* omni_file;
    OMNIHeader header;
    AVLTree<UserInfo> users;
    AVLTree<SessionInfo> sessions;
    FileTree* file_tree;
    FreeSpaceManager* free_manager;
    uint32_t total_files;
    uint32_t total_directories;
    
    // Store config for use
    FileSystemConfig config;
    
    OFSInstance() : omni_file(nullptr), file_tree(nullptr), free_manager(nullptr),
                   total_files(0), total_directories(1) {}
    
    ~OFSInstance() {
        if (omni_file) fclose(omni_file);
        if (file_tree) delete file_tree;
        if (free_manager) delete free_manager;
    }
};

#endif