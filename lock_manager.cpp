#include "lock_manager.h"

// Ensure lock exists for a key (thread-safe)
Lock* LockManager::get_lock(const std::string& key) {
    std::lock_guard<std::mutex> guard(table_mutex);
    return &lock_table[key]; // safe, creates Lock if not exists
}

void LockManager::lock_shared(const std::string& key) {
    Lock* lock = get_lock(key);

    std::unique_lock<std::mutex> l(lock->mtx);

    lock->cv.wait(l, [&] { return !lock->exclusive; });

    lock->shared_count++;
}

void LockManager::lock_exclusive(const std::string& key) {
    Lock* lock = get_lock(key);

    std::unique_lock<std::mutex> l(lock->mtx);

    lock->cv.wait(l, [&] { return !lock->exclusive && lock->shared_count == 0; });

    lock->exclusive = true;
}

void LockManager::unlock_shared(const std::string& key) {
    Lock* lock = get_lock(key);

    std::unique_lock<std::mutex> l(lock->mtx);

    lock->shared_count--;

    if (lock->shared_count == 0)
        lock->cv.notify_all();
}

void LockManager::unlock_exclusive(const std::string& key) {
    Lock* lock = get_lock(key);

    std::unique_lock<std::mutex> l(lock->mtx);

    lock->exclusive = false;

    lock->cv.notify_all();
}

// --- Non-blocking try-lock ---
bool LockManager::try_lock_exclusive(const std::string& key) {
    Lock* lk = get_lock(key);
    std::unique_lock<std::mutex> ul(lk->mtx, std::defer_lock);

    // try to acquire mutex
    if (!ul.try_lock())
        return false;

    // can only get exclusive if no shared or exclusive locks
    if (lk->exclusive || lk->shared_count > 0)
        return false;

    lk->exclusive = true;
    return true;
}