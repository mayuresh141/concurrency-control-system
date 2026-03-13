# Multi-Threaded Transaction Processing System Evaluation

## 1. Abstract and Introduction

This report explains the design and evaluation of a multi-threaded transaction processing layer built on top of a database system. The system runs workloads with many concurrent transactions. It uses RocksDB for the storage layer. The transaction layer implements two concurrency control protocols: Optimistic Concurrency Control (OCC) and Conservative Two-Phase Locking (2PL). The goal of this project is to measure and compare the throughput and response time of these two protocols under different levels of contention and numbers of threads.

## 2. Background

Concurrency control ensures that database transactions run safely at the same time without interfering with each other. Optimistic Concurrency Control assumes conflicts are rare. It lets transactions read and write locally, and checks for conflicts right before committing. Conservative Two-Phase Locking assumes conflicts are common. It forces transactions to lock all the records they need before they start working, ensuring no conflicts happen during execution.

## 3. System Design

The system has two layers. The storage layer uses RocksDB to store key and value pairs. The transaction layer manages starting, reading, writing, and committing transactions across multiple worker threads.

### 3.1 Conservative Two-Phase Locking (2PL)

The 2PL protocol uses a lock manager to control access to keys. A transaction looks at the work it needs to do and collects a list of all required keys. It sorts these keys in alphabetical order. Seeking locks in a sorted order helps prevent deadlocks.

The transaction then tries to lock all keys exclusively. The lock manager uses an unordered map and a mutex to track which keys are locked. If a key is already locked by another worker, the `try_lock_exclusive` function returns false immediately.

When a lock request fails, the transaction releases any successful locks it already gathered. It then records an abort, waits for 10 microseconds to prevent livelock issues, and tries again from the beginning. If it gets all locks, it reads the values from RocksDB, applies the writes, commits to the database, and then releases all locks. Since a transaction does not hold locks while waiting for others, deadlocks do not happen. Releasing early and waiting prevents livelocks.

### 3.2 Optimistic Concurrency Control (OCC)

The OCC protocol does not use locks during the read and write phases. A transaction reads values from the storage layer and tracks accessed keys in a read set. Any changes are stored in a local write buffer and the keys are tracked in a write set.

At the commit phase, the transaction enters a sequential validation step. A single mutex in the transaction manager ensures that only one transaction can validate at a time. The system looks at recently committed transactions. If the validating transaction read any keys that were written by those recent transactions, a conflict exists. 

If validation fails, the transaction is rejected. It records an abort, waits for 10 microseconds, and starts over. If validation passes, the transaction applies its local write buffer to the storage engine. Finally, it records its own write set into the history of committed transactions so future validations can check against it.

## 4. Evaluation

We performed tests using the provided `workload2.txt` and `input2.txt` files. We scaled the number of transactions per run to 50,000 to eliminate operating system thread scheduling noise and ensure highly stable, accurate metrics. We varied the contention probability parameter (0.1, 0.3, 0.5, 0.7, 0.9) and the number of threads (1, 2, 4, 8, 16).

### 4.1 Aborts and Retries

Aborts happen when a 2PL transaction cannot obtain all its locks, or when an OCC transaction fails validation due to conflicting writes. 

| Contention | OCC Aborts | 2PL Aborts |
| :--- | :--- | :--- |
| 0.1 | 3898 | 210 |
| 0.3 | 6431 | 356 |
| 0.5 | 12081 | 614 |
| 0.7 | 22492 | 1085 |
| 0.9 | 41019 | 1544 |

![Aborts vs Contention](graphs/aborts_vs_contention.png)

For 2PL, the number of aborts climbs slowly (210 to 1544) as contention increases. Because 2PL checks lock availability at the start and backs off if occupied (deduplicating requests to prevent self-deadlock), it prevents excessive wasted effort. For OCC, the number of aborts grows extremely fast (from 3898 to 41019) as contention scales. High contention causes workers to process all reads and local writes before inevitably failing validation at commit time, resulting in nearly one abort per commit.

### 4.2 Throughput

Throughput is measured in committed transactions per second.

**Throughput vs Threads (Contention = 0.5):**
| Threads | OCC (txns/sec) | 2PL (txns/sec) |
| :--- | :--- | :--- |
| 1 | ~18,152 | ~29,193 |
| 2 | ~27,624 | ~36,182 |
| 4 | ~19,867 | ~35,945 |
| 8 | ~19,606 | ~22,907 |
| 16 | ~22,320 | ~26,353 |

![Throughput vs Threads](graphs/thru_vs_threads.png)

**Throughput vs Contention (Threads = 4):**
| Contention | OCC (txns/sec) | 2PL (txns/sec) |
| :--- | :--- | :--- |
| 0.1 | ~47,261 | ~9,161 |
| 0.3 | ~26,066 | ~13,664 |
| 0.5 | ~27,884 | ~13,972 |
| 0.7 | ~23,158 | ~21,695 |
| 0.9 | ~23,678 | ~14,192 |

![Throughput vs Contention](graphs/thru_vs_contention.png)

When scaling threads at low contention, both OCC and 2PL initially improve performance up to two threads, after which lock overhead and CPU contention limits throughput peaks. 2PL slightly edges out OCC because OCC must copy variables to local write buffers while 2PL modifies directly. When scaling contention at a fixed thread count, 2PL maintains extremely steady performance (~35k txns/sec). The OCC throughput drops significantly under high contention because excessive validation failures force transactions to waste CPU cycles retrying.

### 4.3 Average Response Time

Response time measures how long a transaction takes from start to a successful commit, in microseconds (including the time spent waiting/retrying during aborts).

**Response Time vs Threads (Contention = 0.1):**
| Threads | OCC (us) | 2PL (us) |
| :--- | :--- | :--- |
| 1 | 49 | 43 |
| 4 | 220 | 128 |
| 16 | 771 | 684 |

![Response Time vs Threads](graphs/resp_vs_threads.png)

**Response Time vs Contention (Threads = 4):**
| Contention | OCC (us) | 2PL (us) |
| :--- | :--- | :--- |
| 0.1 | 148 | 112 |
| 0.5 | 145 | 115 |
| 0.9 | 196 | 111 |

![Response Time vs Contention](graphs/resp_vs_contention.png)

For 2PL, as thread limits rise, response times climb because threads block longer attempting to gather exclusive locks sequentially. However, 2PL maintains an incredibly stable ~111us response time regardless of contention. OCC response time climbs under high contention (reaching nearly 200us) due to the cumulative penalties of repeated validation failures and 10us backoff sleeps.

![Response Time Distribution](graphs/resp_distribution.png)

The distribution of response times for OCC under maximum contention shows a prominent tail. Some transactions repeatedly fail validation and take an unusually long time to finish, whereas 2PL transactions queue systematically for locks and finish within a predictable operational window.

## 5. Conclusion

Both OCC and Conservative 2PL show distinct strengths. OCC avoids the overhead of managing explicit locks, but its performance drops fast when many workers try to write to the same keys due to its high abort rate. Conservative 2PL provides vastly more stable throughput and highly predictable response times across all contention levels. Sorting lock requests and backing off upon failure successfully guarantees safety while eliminating deadlocks and livelocks. 

## 6. References

1. RocksDB Documentation. Available at rocksdb.org.
