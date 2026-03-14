#include "lock_manager.h"

Lock* LockManager::get_lock(const std::string& key) {
    std::lock_guard<std::mutex> guard(table_mutex);
    return &lock_table[key]; 
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


bool LockManager::try_lock_exclusive(const std::string& key) {
    Lock* lk = get_lock(key);
    std::unique_lock<std::mutex> ul(lk->mtx, std::defer_lock);

    if (!ul.try_lock())
        return false;

    if (lk->exclusive || lk->shared_count > 0)
        return false;

    lk->exclusive = true;
    return true;
}
