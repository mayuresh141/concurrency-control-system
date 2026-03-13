import subprocess
import re
import matplotlib.pyplot as plt
import numpy as np
import os
import glob

# Parameters for benchmarking
PROTOCOLS = ["occ", "2pl"]
THREADS = [1, 2, 4, 8]
CONTENTIONS = [0.1, 0.5, 0.9]
TOTAL_TXNS = 5000

def run_experiment(protocol, contention, threads):
    cmd = [
        "./app",
        protocol,
        str(contention),
        str(threads),
        str(TOTAL_TXNS),
        "workload2/input2.txt",
        "workload2/workload2.txt"
    ]
    
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    
    aborts = 0
    throughput = 0.0
    avg_resp = 0.0
    
    match_aborts = re.search(r"Aborted Transactions: (\d+)", result.stdout)
    if match_aborts:
        aborts = int(match_aborts.group(1))
        
    match_thru = re.search(r"Throughput: ([\d\.]+) txns/sec", result.stdout)
    if match_thru:
        throughput = float(match_thru.group(1))
        
    match_resp = re.search(r"Avg_Response_Time_us: ([\d\.]+)", result.stdout)
    if match_resp:
        avg_resp = float(match_resp.group(1))
        
    return aborts, throughput, avg_resp

def collect_distribution(protocol):
    # read all dist_{protocol}_*.csv files
    files = glob.glob(f"dist_{protocol}_*.csv")
    times = []
    for f in files:
        with open(f, 'r') as file:
            for line in file:
                val = line.strip()
                if val:
                    times.append(int(val))
    return times

def clean_distributions():
    for f in glob.glob("dist_*.csv"):
        os.remove(f)

def run_all():
    results = {
        "aborts_vs_contention": {"occ": [], "2pl": []},
        "thru_vs_threads": {"occ": [], "2pl": []},
        "thru_vs_contention": {"occ": [], "2pl": []},
        "resp_vs_threads": {"occ": [], "2pl": []},
        "resp_vs_contention": {"occ": [], "2pl": []},
        "distributions": {"occ": [], "2pl": []}
    }
    
    # Experiment 1: Aborts vs Contention (Threads = 4)
    print("--- Exp 1: Aborts vs Contention (Threads=4) ---")
    for protocol in PROTOCOLS:
        for c in CONTENTIONS:
            a, t, r = run_experiment(protocol, c, 4)
            results["aborts_vs_contention"][protocol].append(a)
            results["thru_vs_contention"][protocol].append(t)
            results["resp_vs_contention"][protocol].append(r)
            if c == 0.9:
                results["distributions"][protocol] = collect_distribution(protocol)
            clean_distributions()
            
    # Experiment 2: Throughput/Resp vs Threads (Contention = 0.1)
    print("--- Exp 2: Performance vs Threads (Contention=0.1) ---")
    for protocol in PROTOCOLS:
        for t in THREADS:
            a, thru, r = run_experiment(protocol, 0.1, t)
            results["thru_vs_threads"][protocol].append(thru)
            results["resp_vs_threads"][protocol].append(r)
            clean_distributions()
            
    return results

def plot_results(results):
    os.makedirs("graphs", exist_ok=True)
    
    # 1. Aborts vs Contention
    plt.figure(figsize=(8,5))
    plt.plot(CONTENTIONS, results["aborts_vs_contention"]["occ"], marker='o', label="OCC")
    plt.plot(CONTENTIONS, results["aborts_vs_contention"]["2pl"], marker='s', label="2PL")
    plt.xlabel("Contention Probability")
    plt.ylabel("Number of Aborts (Out of 5000 Txns)")
    plt.title("Aborts vs Contention (Threads=4)")
    plt.legend()
    plt.grid(True)
    plt.savefig("graphs/aborts_vs_contention.png")
    plt.close()
    
    # 2. Throughput vs Threads
    plt.figure(figsize=(8,5))
    plt.plot(THREADS, results["thru_vs_threads"]["occ"], marker='o', label="OCC")
    plt.plot(THREADS, results["thru_vs_threads"]["2pl"], marker='s', label="2PL")
    plt.xlabel("Number of Threads")
    plt.ylabel("Throughput (Txns/Sec)")
    plt.title("Throughput vs Threads (Contention=0.1)")
    plt.legend()
    plt.grid(True)
    plt.savefig("graphs/thru_vs_threads.png")
    plt.close()
    
    # 3. Throughput vs Contention
    plt.figure(figsize=(8,5))
    plt.plot(CONTENTIONS, results["thru_vs_contention"]["occ"], marker='o', label="OCC")
    plt.plot(CONTENTIONS, results["thru_vs_contention"]["2pl"], marker='s', label="2PL")
    plt.xlabel("Contention Probability")
    plt.ylabel("Throughput (Txns/Sec)")
    plt.title("Throughput vs Contention (Threads=4)")
    plt.legend()
    plt.grid(True)
    plt.savefig("graphs/thru_vs_contention.png")
    plt.close()
    
    # 4. Response Time vs Threads
    plt.figure(figsize=(8,5))
    plt.plot(THREADS, results["resp_vs_threads"]["occ"], marker='o', label="OCC")
    plt.plot(THREADS, results["resp_vs_threads"]["2pl"], marker='s', label="2PL")
    plt.xlabel("Number of Threads")
    plt.ylabel("Average Response Time (us)")
    plt.title("Response Time vs Threads (Contention=0.1)")
    plt.legend()
    plt.grid(True)
    plt.savefig("graphs/resp_vs_threads.png")
    plt.close()
    
    # 5. Response Time vs Contention
    plt.figure(figsize=(8,5))
    plt.plot(CONTENTIONS, results["resp_vs_contention"]["occ"], marker='o', label="OCC")
    plt.plot(CONTENTIONS, results["resp_vs_contention"]["2pl"], marker='s', label="2PL")
    plt.xlabel("Contention Probability")
    plt.ylabel("Average Response Time (us)")
    plt.title("Response Time vs Contention (Threads=4)")
    plt.legend()
    plt.grid(True)
    plt.savefig("graphs/resp_vs_contention.png")
    plt.close()
    
    # 6. Response Time Distribution
    plt.figure(figsize=(8,5))
    plt.hist(results["distributions"]["occ"], bins=50, alpha=0.5, label="OCC")
    plt.hist(results["distributions"]["2pl"], bins=50, alpha=0.5, label="2PL")
    plt.xlabel("Response Time (us)")
    plt.ylabel("Frequency")
    plt.title("Response Time Distribution (Contention=0.9, Threads=4)")
    plt.legend()
    plt.grid(True)
    plt.savefig("graphs/resp_distribution.png")
    plt.close()
    
    print("Graphs generated in the 'graphs' directory.")

if __name__ == "__main__":
    r = run_all()
    plot_results(r)
    
    print("\n--- Summary Data ---")
    for k, v in r.items():
        if k != "distributions":
            print(f"{k}: {v}")
