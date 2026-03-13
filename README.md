# Concurrency Control System

## Required Third-Party Dependencies

This project requires the following libraries to be installed:
*   RocksDB
*   Snappy
*   LZ4
*   Zstandard (zstd)
*   Bzip2
*   Zlib

You also need a C++17 compatible compiler and `make`. If you are using macOS with Homebrew, you can install the dependencies like this:
`brew install rocksdb snappy lz4 zstd bzip2 zlib`

## Build and Compile Instructions

To build the project, open your terminal, navigate to the project directory, and run the following command to clean previous builds and compile the code:

`make clean && make`

This will generate an executable named `app`.

## How to Run Each Workload

To run a workload, you need an initial dataset and a workload file. The `workload2` directory contains sample files. Run the executable with the following format:

`./app <protocol> <contention_prob> <num_threads> <total_txns> <input_file> <workload_file> [hotset_size]`

*   `<protocol>`: Use `2pl` for Two-Phase Locking or `occ` for Optimistic Concurrency Control.
*   `<total_txns>`: The total number of transactions to execute (e.g., 1000).
*   `<input_file>`: The path to the initial dataset file (e.g., `workload2/input2.txt`).
*   `<workload_file>`: The path to the workload template file (e.g., `workload2/workload2.txt`).

Example run:
`./app 2pl 0.1 4 1000 workload2/input2.txt workload2/workload2.txt`

## How to Set Number of Threads and Contention Level

You set the number of threads and contention level using the command line arguments when running the program:

*   **Number of Threads**: This is the third argument (`<num_threads>`). Change this number to control parallelism. For example, `4` means four worker threads.
*   **Contention Level**: This is controlled by the second argument (`<contention_prob>`), which is the probability of accessing keys from the hotset. A value of `0.1` means a 10 percent chance. A higher value increases contention.
*   **Hotset Size** (Optional): This is an optional seventh argument (`[hotset_size]`). It defines the number of keys in the hotset. If you do not provide it, the system defaults to 10 percent of the total keyspace. A smaller hotset size increases contention.
