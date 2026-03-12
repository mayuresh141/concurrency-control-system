#ifndef LOCK_MANAGER_H
#define LOCK_MANAGER_H

#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <string>

struct Lock {
    std::mutex mtx;
    std::condition_variable cv;
    int shared_count = 0;
    bool exclusive = false;
};

class LockManager {
private:
    std::unordered_map<std::string, Lock> lock_table;
    std::mutex table_mutex;

    // Helper: get lock for a key, creating it if needed (thread-safe)
    Lock* get_lock(const std::string& key);

public:
    void lock_shared(const std::string& key);
    void unlock_shared(const std::string& key);

    void lock_exclusive(const std::string& key);
    void unlock_exclusive(const std::string& key);
    bool try_lock_exclusive(const std::string& key);
};

#endif