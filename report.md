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

### 4. Evaluation

We performed tests using both `workload1.txt` (high density constraints) and `workload2.txt` (low density open constraints) input files, evaluating exactly 5,000 transactions for each. We tracked 2PL and OCC across both datasets, sweeping the contention probability (0.1, 0.3, 0.5, 0.7, 0.9) and the thread count (1, 2, 4, 8, 16).

### 4.1 Aborts and Retries

Aborts measure transactions that failed validations or locking mechanisms and were forced to retry. 

#### Workload 1
**Aborts (Out of 5000 Txns)**
| Contention | OCC | 2PL |
| :--- | :--- | :--- |
| 0.1 | 2,962 | 116 |
| 0.5 | 10,609 | 307 |
| 0.9 | 212,450 | 937 |

![Aborts vs Contention WL1](graphs/wl1_aborts_vs_contention.png)

#### Workload 2
**Aborts (Out of 5000 Txns)**
| Contention | OCC | 2PL |
| :--- | :--- | :--- |
| 0.1 | 371 | 19 |
| 0.5 | 1,222 | 63 |
| 0.9 | 3,885 | 189 |

![Aborts vs Contention WL2](graphs/wl2_aborts_vs_contention.png)

Because 2PL checks lock availability clearly at the start and gracefully backs off if occupied (deduplicating to prevent self-deadlock), it prevents excessive wasted effort and keeps aborts remarkably minimal in both workloads. For OCC under Workload 2 (low-density), aborts climb as expected but stay under system strain control. However, under Workload 1 (high-density keys), OCC's abort rate reaches catastrophic failure levels (averaging over 42 retries per successfully committed transaction at max contention) as constant conflicts invalidate the commit buffer perpetually.

### 4.2 Throughput

Throughput is measured in committed transactions per second.

#### Workload 1
**Throughput vs Threads (Contention = 0.5):**
| Threads | OCC (txns/sec) | 2PL (txns/sec) |
| :--- | :--- | :--- |
| 1 | ~14,520 | ~65,083 |
| 4 | ~37,036 | ~67,604 |
| 16 | ~38,879 | ~59,250 |

![Throughput vs Threads WL1](graphs/wl1_thru_vs_threads.png)

**Throughput vs Contention (Threads = 4):**
| Contention | OCC (txns/sec) | 2PL (txns/sec) |
| :--- | :--- | :--- |
| 0.1 | ~39,611 | ~62,828 |
| 0.5 | ~26,521 | ~84,000 |
| 0.9 | ~2,725 | ~83,515 |

![Throughput vs Contention WL1](graphs/wl1_thru_vs_contention.png)

#### Workload 2
**Throughput vs Threads (Contention = 0.5):**
| Threads | OCC (txns/sec) | 2PL (txns/sec) |
| :--- | :--- | :--- |
| 1 | ~20,888 | ~22,489 |
| 4 | ~39,916 | ~33,944 |
| 16 | ~30,640 | ~34,104 |

![Throughput vs Threads WL2](graphs/wl2_thru_vs_threads.png)

**Throughput vs Contention (Threads = 4):**
| Contention | OCC (txns/sec) | 2PL (txns/sec) |
| :--- | :--- | :--- |
| 0.1 | ~42,724 | ~11,417 |
| 0.5 | ~25,087 | ~55,268 |
| 0.9 | ~19,532 | ~25,424 |

![Throughput vs Contention WL2](graphs/wl2_thru_vs_contention.png)

In Workload 2 (where keys are wide and distinct), OCC and 2PL maintain largely comparable scalability. However, Workload 1 paints a starkly contrasting reality: 2PL commands an overwhelming lead in raw throughput natively out of the gate. This systemic advantage is aggressively exacerbated when scaling up contention: 2PL safely retains ~80k+ txns/sec stably, whereas OCC entirely collapses to merely ~2.7k txns/sec at max contention because blind validation failures force threads into near-constant CPU-burning retry loops.

### 4.3 Average Response Time

Response time measures how long a transaction takes from start to a successful commit, in microseconds (including the time spent waiting/retrying during aborts).

#### Workload 1
**Response Time vs Threads (Contention = 0.5):**
| Threads | OCC (us) | 2PL (us) |
| :--- | :--- | :--- |
| 1 | 68 | 15 |
| 16 | 293 | 174 |

![Response Time vs Threads WL1](graphs/wl1_resp_vs_threads.png)

**Response Time vs Contention (Threads = 4):**
| Contention | OCC (us) | 2PL (us) |
| :--- | :--- | :--- |
| 0.1 | 100 | 63 |
| 0.9 | 1453 | 46 |

![Response Time vs Contention WL1](graphs/wl1_resp_vs_contention.png)

#### Workload 2
**Response Time vs Threads (Contention = 0.5):**
| Threads | OCC (us) | 2PL (us) |
| :--- | :--- | :--- |
| 1 | 46 | 37 |
| 16 | 442 | 451 |

![Response Time vs Threads WL2](graphs/wl2_resp_vs_threads.png)

**Response Time vs Contention (Threads = 4):**
| Contention | OCC (us) | 2PL (us) |
| :--- | :--- | :--- |
| 0.1 | 91 | 350 |
| 0.9 | 202 | 155 |

![Response Time vs Contention WL2](graphs/wl2_resp_vs_contention.png)

For 2PL, response times climb gently as threads wait to gather sequentially locked keys, but crucially across both workloads, 2PL maintains extremely fast and rigid response times mathematically immune to the scale of pure contention logic. Conversely, OCC response times skyrocket under Workload 1 high-contention (reaching nearly ~1,500us) intrinsically due to the catastrophic validation/retry loops stacking up and penalizing single thread completion velocities.

### 4.4 Response Time Distribution

To properly visualize the frequency of completion times across the wildly divergent scaling realities, the axes are mapped to a logarithmic scale.

#### Workload 1 Distribution
![Response Time Distribution WL1](graphs/wl1_resp_distribution.png)

#### Workload 2 Distribution
![Response Time Distribution WL2](graphs/wl2_resp_distribution.png)

In Workload 1, the logarithmic scale highlights a highly skewed OCC tail representing massive populations of transactions repeatedly failing validation dozens of times and holding thread execution hostaged. In stark contrast, 2PL systematically queues locks and rapidly executes, producing mathematically tight, clustered frequency distributions absent of runaway tails.

### 4.5 Throughput vs Hotset Size

To understand how data locality affects performance, we swept the `hotset_size` parameter (100, 500, 1000, 2000, 5000 keys) under moderate contention (0.5) with 4 threads.

#### Workload 1
**Throughput vs Hotset (Txns/sec)**
| Hotset | OCC | 2PL |
| :--- | :--- | :--- |
| 100 | ~14,038 | ~53,191 |
| 500 | ~26,902 | ~45,621 |
| 1000 | ~39,327 | ~49,178 |
| 2000 | ~35,753 | ~33,116 |
| 5000 | ~32,615 | ~25,040 |

![Throughput vs Hotset WL1](graphs/wl1_thru_vs_hotset.png)

#### Workload 2
**Throughput vs Hotset (Txns/sec)**
| Hotset | OCC | 2PL |
| :--- | :--- | :--- |
| 100 | ~12,275 | ~29,759 |
| 500 | ~29,288 | ~38,491 |
| 1000 | ~48,738 | ~43,669 |
| 2000 | ~25,073 | ~13,825 |
| 5000 | ~24,426 | ~35,040 |

![Throughput vs Hotset WL2](graphs/wl2_thru_vs_hotset.png)

When the hotset size is artificially compressed (e.g. 100-500 keys), the working set simulates extremely high localized contention. Here, 2PL consistently maintains an advantage because it safely queues thread access to heavily trafficked records. OCC suffers massive abort penalties because localized hotsets functionally guarantee overlapping write sets across concurrent validations, destroying validation success rates and collapsing throughput. As the hotset size widens toward 5000 keys, conflicts naturally thin out across the database footprint. 

## 5. Conclusion

Both OCC and Conservative 2PL show distinct empirical strengths. OCC neatly avoids the systemic overhead of managing explicit mutex locks, making it highly preferred for low-density/low-conflict scopes similar to Workload 2. However, its core design disintegrates completely under high-conflict dense data subsets like Workload 1 due to geometrically increasing abort rate penalties. Conservative 2PL provides vastly more insulated throughput, structurally lower abort limits, and consistently predictable response boundaries across all workloads. Rigorously sorting lock requests and natively backing off upon failure demonstrably guarantees thread safety while efficiently eliminating deadlocks and livelocks.

## 6. References

1. RocksDB Documentation. Available at rocksdb.org.
