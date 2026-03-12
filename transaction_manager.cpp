#include "transaction_manager.h"

TransactionManager::TransactionManager(StorageEngine* storage, Protocol protocol) {
    this->storage = storage;
    this->protocol = protocol;
}

bool TransactionManager::commit(Transaction& txn) {

    // OCC VALIDATION
    if (protocol == Protocol::OCC) {

        std::lock_guard<std::mutex> lock(validation_mutex);

        int start = committed_txns.size() > 50 ? committed_txns.size() - 50 : 0;

        for (size_t i = start; i < committed_txns.size(); i++) {

            auto& committed = committed_txns[i];

            for (auto& key : txn.read_set) {

                if (committed.write_set.count(key)) {
                    return false;
                }
            }
        }

        // write phase
        for (auto& pair : txn.write_buffer) {
            storage->write(pair.first, pair.second);
        }

        OCCRecord record;
        record.txn_id = txn.txn_id;
        record.write_set = txn.write_set;

        committed_txns.push_back(record);
    }

    // 2PL COMMIT (no validation)
    else {

        for (auto& pair : txn.write_buffer) {
            storage->write(pair.first, pair.second);
        }
    }

    return true;
}