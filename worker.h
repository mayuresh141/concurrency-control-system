#ifndef WORKER_H
#define WORKER_H

#include "storage_engine.h"
#include "metrics.h"
#include "lock_manager.h"
#include "protocol.h"
#include <vector>
#include <string>

// Struct to represent a transaction template
struct Template {
    std::vector<std::string> placeholders; // e.g., ["FROM_KEY", "TO_KEY"]
    // You can extend this later to store the actual operations if needed
};

class Worker {

private:
    int worker_id;
    StorageEngine* storage;
    Metrics* metrics;
    LockManager* lock_manager;

    int txn_count;
    std::vector<std::string> keys;
    Protocol protocol;

    // Hotset / contention parameters
    double contention_prob;
    int hotset_size;

    // Transaction templates
    std::vector<Template> templates;

public:
    Worker(int id,
           StorageEngine* storage,
           Metrics* metrics,
           LockManager* lock_manager,
           int txn_count,
           const std::vector<std::string>& keys,
           Protocol protocol,
           double contention_prob,
           int hotset_size,
           const std::vector<Template>& templates);

    void run();
};

#endif