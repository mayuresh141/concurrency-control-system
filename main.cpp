#include "storage_engine.h"
#include "worker.h"
#include "metrics.h"
#include "lock_manager.h"
#include "protocol.h"

#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <string>
#include <fstream>
#include <sstream>
#include <random>


void load_input(const std::string& filename,
                StorageEngine& storage,
                std::vector<std::string>& keys)
{
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        if (line.rfind("KEY:", 0) == 0) {
            size_t key_start = line.find("KEY:") + 4;
            size_t comma = line.find(",");
            std::string key = line.substr(key_start, comma - key_start);
            key.erase(0, key.find_first_not_of(" "));
            key.erase(key.find_last_not_of(" ") + 1);

            size_t value_start = line.find("VALUE:") + 6;
            std::string value = line.substr(value_start);

            storage.insert(key, value);
            keys.push_back(key);
        }
    }
}

std::vector<Template> load_workload(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    std::vector<Template> templates;

    Template temp;
    bool in_transaction = false;

    while (std::getline(file, line)) {
        if (line.rfind("TRANSACTION", 0) == 0) {
            temp.placeholders.clear();
            size_t start = line.find("(");
            size_t end = line.find(")");
            if (start != std::string::npos && end != std::string::npos) {
                std::string inner = line.substr(start + 1, end - start - 1);
                size_t pos = inner.find("INPUTS:");
                if (pos != std::string::npos) {
                    std::string keys_str = inner.substr(pos + 7);
                    std::stringstream ss(keys_str);
                    std::string key;
                    while (std::getline(ss, key, ',')) {
                        key.erase(0, key.find_first_not_of(" "));
                        key.erase(key.find_last_not_of(" ") + 1);
                        temp.placeholders.push_back(key);
                    }
                }
            }
            in_transaction = true;
        }
        else if (line.rfind("COMMIT", 0) == 0 && in_transaction) {
            templates.push_back(temp);
            in_transaction = false;
        }
    }
    return templates;
}

int main(int argc, char* argv[]) {

    if (argc < 5) {
        std::cerr << "Usage: ./app <protocol> <contention_prob> <num_threads> <total_txns> <input_file> <workload_file> [hotset_size]\n";
        return 1;
    }
    Protocol protocol = (std::string(argv[1]) == "2pl") ? Protocol::C2PL : Protocol::OCC;
    double contention_prob = std::stod(argv[2]);
    int num_threads = std::stoi(argv[3]);
    int total_txns = std::stoi(argv[4]);
    int hotset_size = (argc > 7) ? std::stoi(argv[7]) : -1;    
    std::string input_file = argv[5];
    std::string workload_file = argv[6];

    std::cout << "Starting transaction processing system...\n";

    StorageEngine storage("mydb");
    Metrics metrics;
    LockManager lock_manager;

    std::vector<std::string> keys;
    load_input(input_file, storage, keys);

    auto templates = load_workload(workload_file);
    std::cout << "Loaded " << templates.size() << " transaction templates\n";

    int remainder = total_txns % num_threads;
    int base_txns = total_txns / num_threads;
    hotset_size = std::max(1, static_cast<int>(keys.size() * 0.1));
    std::vector<std::thread> threads;
    std::vector<Worker> workers;
    for (int i = 0; i < num_threads; i++) {
        int txns_per_thread = base_txns;
        if (i < remainder) ++txns_per_thread;
        workers.emplace_back(i, &storage, &metrics, &lock_manager,
                             txns_per_thread, keys, protocol,
                             contention_prob, hotset_size, templates);
    }

    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "Launching workers...\n";

    for (int i = 0; i < num_threads; i++)
        threads.emplace_back(&Worker::run, &workers[i]);

    for (auto& t : threads) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    double seconds = std::chrono::duration<double>(end - start).count();

    std::cout << "\nExecution finished\n";
    std::cout << "Committed Transactions: " << metrics.committed_txns.load() << "\n";
    std::cout << "Aborted Transactions: " << metrics.aborted_txns.load() << "\n";
    std::cout << "Throughput: " << metrics.committed_txns.load() / seconds << " txns/sec\n";
    
    double avg_resp = 0;
    if (metrics.committed_txns.load() > 0) {
        avg_resp = (double)metrics.total_response_time_us.load() / metrics.committed_txns.load();
    }
    std::cout << "Avg_Response_Time_us: " << avg_resp << "\n";

    return 0;
}