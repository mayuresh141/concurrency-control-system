#include "worker.h"
#include "transaction_manager.h"
#include <random>
#include <thread>
#include <chrono>
#include <iostream>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <fstream>

// helper: extract balance from JSON-like value
int extract_balance(const std::string& value) {
    size_t pos = value.find("balance:");
    if (pos == std::string::npos) return 0;

    pos += 8;
    while (pos < value.size() && (value[pos] == ' ' || value[pos] == '"'))
        pos++;

    size_t end = value.find_first_of("}", pos);
    return std::stoi(value.substr(pos, end - pos));
}

// helper: update balance in value string
std::string update_balance(const std::string& value, int new_balance) {
    size_t pos = value.find("balance:");
    if (pos == std::string::npos) return value;

    size_t start = pos + 8;
    while (start < value.size() && value[start] == ' ')
        start++;

    size_t end = value.find_first_of("}", start);

    return value.substr(0, start) + std::to_string(new_balance) + value.substr(end);
}

// --- Worker constructor ---
Worker::Worker(int id,
               StorageEngine* storage,
               Metrics* metrics,
               LockManager* lock_manager,
               int txn_count,
               const std::vector<std::string>& keys,
               Protocol protocol,
               double contention_prob,
               int hotset_size,
               const std::vector<Template>& templates)
{
    this->worker_id = id;
    this->storage = storage;
    this->metrics = metrics;
    this->lock_manager = lock_manager;
    this->txn_count = txn_count;
    this->keys = keys;
    this->protocol = protocol;
    this->contention_prob = contention_prob;
    this->hotset_size = hotset_size;
    this->templates = templates;
}

void Worker::run() {
    TransactionManager txn_manager(storage, protocol);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> prob_dist(0.0, 1.0);
    std::uniform_int_distribution<> full_dist(0, keys.size() - 1);
    std::uniform_int_distribution<> hot_dist(0, hotset_size - 1);
    std::uniform_int_distribution<> tmpl_dist(0, templates.size() - 1);

    std::cout << "Worker " << worker_id << " started\n";

    int committed_count = 0;
    std::vector<uint64_t> response_times;
    response_times.reserve(txn_count);
    while (committed_count < txn_count) {

        auto txn_start = std::chrono::high_resolution_clock::now();
        bool committed = false;

        while (!committed) {
            Transaction txn(committed_count);

            // --- Pick a random template ---
            Template t = templates[tmpl_dist(gen)];

            // --- Select actual keys for placeholders ---
            std::unordered_map<std::string, std::string> actual_keys;
            for (auto& ph : t.placeholders) {
                double r = prob_dist(gen);
                if (r < contention_prob)
                    actual_keys[ph] = keys[hot_dist(gen)];
                else
                    actual_keys[ph] = keys[full_dist(gen)];
            }

            // --- 2PL: acquire all locks in sorted order ---
            std::vector<std::string> lock_keys;
            for (auto& p : actual_keys) lock_keys.push_back(p.second);
            std::sort(lock_keys.begin(), lock_keys.end());

            bool acquired_all = true;
            if (protocol == Protocol::C2PL) {
                for (auto& k : lock_keys) {
                    if (!lock_manager->try_lock_exclusive(k)) {
                        acquired_all = false;
                        break;
                    }
                }

                if (!acquired_all) {
                    // release any locks we acquired
                    for (auto& k : lock_keys) lock_manager->unlock_exclusive(k);
                    metrics->aborted_txns++;
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                    continue; // retry transaction
                }
            }

            // --- READ & MODIFY (simple "transfer" example for demo) ---
            std::vector<std::pair<std::string, int>> balances;
            for (auto& k : lock_keys) {
                std::string val = txn.read(k, storage);
                int bal = extract_balance(val);
                balances.push_back({k, bal});
            }

            // example logic: decrement first key, increment second key
            if (balances.size() >= 2) {
                balances[0].second -= 1;
                balances[1].second += 1;
            }

            for (auto& b : balances) {
                std::string old_val;
                txn.read(b.first, storage); // ensure in read_set
                std::string new_val = update_balance(txn.read(b.first, storage), b.second);
                txn.write(b.first, new_val);
            }

            // --- COMMIT ---
            committed = txn_manager.commit(txn);

            // --- UNLOCK ---
            if (protocol == Protocol::C2PL) {
                for (auto it = lock_keys.rbegin(); it != lock_keys.rend(); ++it)
                    lock_manager->unlock_exclusive(*it);
            }

            if (!committed) {
                metrics->aborted_txns++;
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }

        auto txn_end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(txn_end - txn_start).count();
        
        metrics->total_response_time_us += duration;
        metrics->committed_txns++;
        committed_count++;
        response_times.push_back(duration);
    }

    std::string proto_name = (protocol == Protocol::OCC) ? "occ" : "2pl";
    std::ofstream out("dist_" + proto_name + "_" + std::to_string(worker_id) + ".csv");
    for (auto rt : response_times) out << rt << "\n";
    out.close();

    std::cout << "Worker " << worker_id << " finished\n";
}