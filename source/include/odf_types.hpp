#ifndef ODF_TYPES_HPP
#define ODF_TYPES_HPP

#include <cstdint>
#include <string>
#include <cstring>  // For std::strncpy and std::memset

// ============================================================================
// ENUMERATIONS - DO NOT MODIFY THESE VALUES
// ============================================================================

/**
 * User role types
 * These values MUST remain consistent across all implementations
 */
enum class UserRole : uint32_t {
    NORMAL = 0,    // Regular user with standard permissions
    ADMIN = 1      // Administrator with full permissions
};

/**
 * Standard error codes
 * All functions return these codes to indicate operation status
 * DO NOT MODIFY THESE VALUES
 */
enum class OFSErrorCodes : int32_t {
    SUCCESS = 0,                      // Operation completed successfully
    ERROR_NOT_FOUND = -1,            // File/directory/user not found
    ERROR_PERMISSION_DENIED = -2,    // User lacks required permissions
    ERROR_IO_ERROR = -3,             // File I/O operation failed
    ERROR_INVALID_PATH = -4,         // Path format is invalid
    ERROR_FILE_EXISTS = -5,          // File/directory already exists
    ERROR_NO_SPACE = -6,             // Insufficient space in file system
    ERROR_INVALID_CONFIG = -7,       // Configuration file is invalid
    ERROR_NOT_IMPLEMENTED = -8,      // Feature not yet implemented
    ERROR_INVALID_SESSION = -9,      // Session is invalid or expired
    ERROR_DIRECTORY_NOT_EMPTY = -10, // Cannot delete non-empty directory
    ERROR_INVALID_OPERATION = -11    // Operation not allowed
};

/**
 * File entry types
 */
enum class EntryType : uint8_t {
    FILE = 0,         // Regular file
    DIRECTORY = 1     // Directory
};

/**
 * File permission flags (UNIX-style)
 */
enum class FilePermissions : uint32_t {
    OWNER_READ = 0400,      // Owner can read
    OWNER_WRITE = 0200,     // Owner can write
    OWNER_EXECUTE = 0100,   // Owner can execute
    GROUP_READ = 0040,      // Group can read
    GROUP_WRITE = 0020,     // Group can write
    GROUP_EXECUTE = 0010,   // Group can execute
    OTHERS_READ = 0004,     // Others can read
    OTHERS_WRITE = 0002,    // Others can write
    OTHERS_EXECUTE = 0001   // Others can execute
};

// ============================================================================
// DATA STRUCTURES - MAINTAIN EXACT SIZES AND FIELD ORDER
// ============================================================================

/**
 * OMNI File Header (512 bytes total)
 * Located at the beginning of every .omni file
 * 
 * CRITICAL: Do not modify field order or sizes - this must be consistent
 * across all implementations for file compatibility
 */
struct OMNIHeader {
    char magic[8];              // Magic number: "OMNIFS01" (8 bytes)
    uint32_t format_version;    // Format version: 0x00010000 for v1.0 (4 bytes)
    uint64_t total_size;        // Total file system size in bytes (8 bytes)
    uint64_t header_size;       // Size of this header (8 bytes)
    uint64_t block_size;        // Block size in bytes (8 bytes)
    
    char student_id[32];        // Student ID who created this (32 bytes)
    char submission_date[16];   // Creation date YYYY-MM-DD (16 bytes)
    
    char config_hash[64];       // SHA-256 hash of config file (64 bytes)
    uint64_t config_timestamp;  // Config file timestamp (8 bytes)
    
    uint32_t user_table_offset; // Byte offset to user table (4 bytes)
    uint32_t max_users;         // Maximum number of users (4 bytes)
    
    // Reserved for Phase 2: Delta Vault 
    uint32_t file_state_storage_offset;  // Offset to file_state_storage area (4 bytes)
    uint32_t change_log_offset;       // Offset to change log (4 bytes)
    
    uint8_t reserved[328];      // Reserved for future use (328 bytes)

    // Default constructor
    OMNIHeader() = default;
    
    // Constructor with initialization
    OMNIHeader(uint32_t version, uint64_t size, uint64_t header_sz, uint64_t block_sz)
        : format_version(version), total_size(size), header_size(header_sz), block_size(block_sz) {
        std::memset(magic, 0, sizeof(magic));
        std::memset(student_id, 0, sizeof(student_id));
        std::memset(submission_date, 0, sizeof(submission_date));
        std::memset(config_hash, 0, sizeof(config_hash));
        std::memset(reserved, 0, sizeof(reserved));
    }
};  // Total: 512 bytes

/**
 * User Information Structure
 * Stored in user table within .omni file
 */
struct UserInfo {
    char username[32];          // Username (null-terminated)
    char password_hash[64];     // Password hash (SHA-256)
    UserRole role;              // User role (4 bytes)
    uint64_t created_time;      // Account creation timestamp (Unix epoch)
    uint64_t last_login;        // Last login timestamp (Unix epoch)
    uint8_t is_active;          // 1 if active, 0 if deleted
    uint8_t reserved[23];       // Reserved for future use

    // Default constructor
    UserInfo() = default;
    
    // Constructor
    UserInfo(const std::string& user, const std::string& hash, UserRole r, uint64_t created)
        : role(r), created_time(created), last_login(0), is_active(1) {
        std::strncpy(username, user.c_str(), sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
        std::strncpy(password_hash, hash.c_str(), sizeof(password_hash) - 1);
        password_hash[sizeof(password_hash) - 1] = '\0';
        std::memset(reserved, 0, sizeof(reserved));
    }
};  // Total: 128 bytes

/**
 * File/Directory Entry Structure
 * Used for directory listings and file metadata
 */
struct FileEntry {
    uint8_t valid;              // 1 for valid/in-use, 0 for invalid/deleted
    char name[256];             // File/directory name (null-terminated)
    uint8_t type;               // 0=file, 1=directory (EntryType)
    uint64_t size;              // Size in bytes (0 for directories)
    uint32_t permissions;       // UNIX-style permissions (e.g., 0644)
    uint64_t created_time;      // Creation timestamp (Unix epoch)
    uint64_t modified_time;     // Last modification timestamp (Unix epoch)
    char owner[32];             // Username of owner
    uint32_t inode;             // Internal file identifier (start block for files)
    uint32_t parent_index;      // Entry index of parent directory (1 for root's children)
    uint8_t reserved[42];       // Reserved for future use (47 - 4 - 1 = 42)

    // Default constructor
    FileEntry() : valid(0), type(0), size(0), permissions(0), created_time(0), 
                  modified_time(0), inode(0), parent_index(0) {
        std::memset(name, 0, sizeof(name));
        std::memset(owner, 0, sizeof(owner));
        std::memset(reserved, 0, sizeof(reserved));
    }
    
    // Constructor
    FileEntry(const std::string& filename, EntryType entry_type, uint64_t file_size, 
              uint32_t perms, const std::string& file_owner, uint32_t file_inode,
              uint32_t parent_idx)
        : valid(1), type(static_cast<uint8_t>(entry_type)), size(file_size), 
          permissions(perms), created_time(0), modified_time(0), 
          inode(file_inode), parent_index(parent_idx) {
        std::strncpy(name, filename.c_str(), sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        std::strncpy(owner, file_owner.c_str(), sizeof(owner) - 1);
        owner[sizeof(owner) - 1] = '\0';
        std::memset(reserved, 0, sizeof(reserved));
    }
    
    // Getter for type as enum
    EntryType getType() const { return static_cast<EntryType>(type); }
    
    // Setter for type as enum
    void setType(EntryType entry_type) { type = static_cast<uint8_t>(entry_type); }
    
    // Check if entry is valid
    bool isValid() const { return valid == 1; }
    
    // Mark entry as valid
    void markValid() { valid = 1; }
    
    // Mark entry as invalid/deleted
    void markInvalid() { valid = 0; }
};  // Total: 416 bytes

/**
 * File Metadata (Extended information)
 * Returned by get_metadata function
 */
struct FileMetadata {
    char path[512];             // Full path
    FileEntry entry;            // Basic entry information
    uint64_t blocks_used;       // Number of blocks used
    uint64_t actual_size;       // Actual size on disk (may differ from logical size)
    uint8_t reserved[64];       // Reserved

    // Default constructor
    FileMetadata() = default;
    
    // Constructor
    FileMetadata(const std::string& file_path, const FileEntry& file_entry)
        : entry(file_entry), blocks_used(0), actual_size(0) {
        std::strncpy(path, file_path.c_str(), sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        std::memset(reserved, 0, sizeof(reserved));
    }
};

/**
 * Session Information
 * Returned by get_session_info function
 */
struct SessionInfo {
    char session_id[64];        // Unique session identifier
    UserInfo user;              // User information
    uint64_t login_time;        // When session was created
    uint64_t last_activity;     // Last activity timestamp
    uint32_t operations_count;  // Number of operations performed
    uint8_t reserved[32];       // Reserved

    // Default constructor
    SessionInfo() = default;
    
    // Constructor
    SessionInfo(const std::string& id, const UserInfo& user_info, uint64_t login)
        : user(user_info), login_time(login), last_activity(login), operations_count(0) {
        std::strncpy(session_id, id.c_str(), sizeof(session_id) - 1);
        session_id[sizeof(session_id) - 1] = '\0';
        std::memset(reserved, 0, sizeof(reserved));
    }
};

/**
 * File System Statistics
 * Returned by get_stats function
 */
struct FSStats {
    uint64_t total_size;        // Total file system size
    uint64_t used_space;        // Space currently used
    uint64_t free_space;        // Available free space
    uint32_t total_files;       // Total number of files
    uint32_t total_directories; // Total number of directories
    uint32_t total_users;       // Total number of users
    uint32_t active_sessions;   // Currently active sessions
    double fragmentation;       // Fragmentation percentage (0.0 - 100.0)
    uint8_t reserved[64];       // Reserved

    // Default constructor
    FSStats() = default;
    
    // Constructor
    FSStats(uint64_t total, uint64_t used, uint64_t free)
        : total_size(total), used_space(used), free_space(free),
          total_files(0), total_directories(0), total_users(0),
          active_sessions(0), fragmentation(0.0) {
        std::memset(reserved, 0, sizeof(reserved));
    }
};

#endif // ODF_TYPES_HPP