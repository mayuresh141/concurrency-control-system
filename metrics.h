#ifndef METRICS_H
#define METRICS_H

#include <atomic>

class Metrics {
public:
    std::atomic<int> committed_txns;
    std::atomic<int> aborted_txns;
    std::atomic<uint64_t> total_response_time_us;

    Metrics() {
        committed_txns = 0;
        aborted_txns = 0;
        total_response_time_us = 0;
    }
};

#endif