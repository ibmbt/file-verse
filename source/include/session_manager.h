#ifndef SESSION_MANAGER_HPP
#define SESSION_MANAGER_HPP

#include "../include/odf_types.hpp"
#include "../include/config_parser.h"
#include "ofs_instance.h"
#include <string>
#include <ctime>
#include <iostream>

using namespace std;

struct ManagedSession {
    string session_id;
    SessionInfo info;
    OFSInstance* instance;
    bool is_active;
    
    ManagedSession() : instance(nullptr), is_active(false) {}
    ManagedSession(const string& id, const SessionInfo& si, OFSInstance* inst)
        : session_id(id), info(si), instance(inst), is_active(true) {}
};

class SessionManager {
private:
    static inline ManagedSession* sessions = nullptr;
    static inline OFSInstance* global_instance = nullptr;
    static inline int session_count = 0;
    static inline int max_sessions = 0;
    
    static int findSessionIndex(const string& session_id) {
        if (!sessions) return -1;
        
        for (int i = 0; i < max_sessions; i++) {
            if (sessions[i].is_active && sessions[i].session_id == session_id) {
                return i;
            }
        }
        return -1;
    }
    
    static int findFreeSlot() {
        if (!sessions) return -1;
        
        for (int i = 0; i < max_sessions; i++) {
            if (!sessions[i].is_active) {
                return i;
            }
        }
        return -1;
    }
    
public:
    static void initialize(const FileSystemConfig& config) {
        if (sessions) {
            delete[] sessions;
        }
        
        max_sessions = config.max_connections;
        sessions = new ManagedSession[max_sessions];
        session_count = 0;
    }
    
    static void setInstance(OFSInstance* inst) {
        global_instance = inst;
    }
    
    static OFSInstance* getInstance() {
        return global_instance;
    }
    
    static string createSession(const UserInfo& user, OFSInstance* inst) {
        if (!sessions) {
            return "";
        }
        
        for (int i = 0; i < max_sessions; i++) {
            if (sessions[i].is_active && 
                string(sessions[i].info.user.username) == string(user.username)) {
                return sessions[i].session_id;
            }
        }
        
        string session_id = string(user.username) + "_" + to_string(time(nullptr));
        SessionInfo info(session_id, user, time(nullptr));
        
        int slot = findFreeSlot();
        if (slot != -1) {
            sessions[slot] = ManagedSession(session_id, info, inst);
            session_count++;
            
            inst->sessions.insert(session_id, info);
            
            return session_id;
        }
        
        return "";
    }
    
    static ManagedSession* getSession(const string& session_id) {
        int index = findSessionIndex(session_id);
        if (index != -1) {
            sessions[index].info.last_activity = time(nullptr);
            return &sessions[index];
        }
        return nullptr;
    }
    
    static bool removeSession(const string& session_id) {
        int index = findSessionIndex(session_id);
        if (index != -1) {
            if (sessions[index].instance) {
                sessions[index].instance->sessions.remove(session_id);
            }
            
            sessions[index].is_active = false;
            session_count--;
            
            return true;
        }
        return false;
    }
    
    static void clearAll() {
        if (!sessions) return;
        
        for (int i = 0; i < max_sessions; i++) {
            sessions[i].is_active = false;
        }
        session_count = 0;
    }
    
    static int getSessionCount() {
        return session_count;
    }
    
    static int getMaxSessions() {
        return max_sessions;
    }
    
    static void printActiveSessions() {
        if (!sessions) {
            return;
        }
        
        for (int i = 0; i < max_sessions; i++) {
            if (sessions[i].is_active) {
                cout << "  " << sessions[i].session_id 
                     << " - User: " << sessions[i].info.user.username
                     << " - Last Activity: " << sessions[i].info.last_activity << endl;
            }
        }
    }
    
    static void cleanup() {
        if (sessions) {
            delete[] sessions;
            sessions = nullptr;
        }
        session_count = 0;
        max_sessions = 0;
    }
};

#endif