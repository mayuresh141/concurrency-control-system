#ifndef TRANSACTION_MANAGER_H
#define TRANSACTION_MANAGER_H

#include "transaction.h"
#include "storage_engine.h"
#include "occ_record.h"
#include "protocol.h"
#include <vector>
#include <mutex>

class TransactionManager {

private:
    StorageEngine* storage;

    std::vector<OCCRecord> committed_txns;

    std::mutex validation_mutex;
    Protocol protocol;

public:
    TransactionManager(StorageEngine* storage, Protocol protocol);

    bool commit(Transaction& txn);
};

#endif