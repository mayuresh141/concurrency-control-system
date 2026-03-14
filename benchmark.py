import subprocess
import re
import matplotlib.pyplot as plt
import os
import glob

PROTOCOLS = ["occ", "2pl"]
WORKLOADS = ["workload1", "workload2"]
THREADS = [1, 2, 4, 8, 16]
CONTENTIONS = [0.1, 0.3, 0.5, 0.7, 0.9]
TOTAL_TXNS = 5000

def run_experiment(protocol, contention, threads, workload):
    cmd = [
        "./app",
        protocol,
        str(contention),
        str(threads),
        str(TOTAL_TXNS),
        f"{workload}/input{workload[-1]}.txt",
        f"{workload}/{workload}.txt"
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

def collect_distribution(protocol, workload):
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
        try:
            os.remove(f)
        except Exception:
            pass

def run_all():
    results = {
        "aborts_vs_contention": {p: {w: [] for w in WORKLOADS} for p in PROTOCOLS},
        "thru_vs_threads": {p: {w: [] for w in WORKLOADS} for p in PROTOCOLS},
        "thru_vs_contention": {p: {w: [] for w in WORKLOADS} for p in PROTOCOLS},
        "resp_vs_threads": {p: {w: [] for w in WORKLOADS} for p in PROTOCOLS},
        "resp_vs_contention": {p: {w: [] for w in WORKLOADS} for p in PROTOCOLS},
        "distributions": {p: {w: [] for w in WORKLOADS} for p in PROTOCOLS}
    }
    
    print("--- Exp 1: Aborts vs Contention (Threads=4) ---")
    for workload in WORKLOADS:
        for protocol in PROTOCOLS:
            for c in CONTENTIONS:
                a, t, r = run_experiment(protocol, c, 4, workload)
                results["aborts_vs_contention"][protocol][workload].append(a)
                results["thru_vs_contention"][protocol][workload].append(t)
                results["resp_vs_contention"][protocol][workload].append(r)
                if c == 0.9:
                    results["distributions"][protocol][workload] = collect_distribution(protocol, workload)
                clean_distributions()
            
    print("--- Exp 2: Performance vs Threads (Contention=0.5) ---")
    for workload in WORKLOADS:
        for protocol in PROTOCOLS:
            for t in THREADS:
                a, thru, r = run_experiment(protocol, 0.5, t, workload)
                results["thru_vs_threads"][protocol][workload].append(thru)
                results["resp_vs_threads"][protocol][workload].append(r)
                clean_distributions()
            
    return results

def plot_results(results):
    os.makedirs("graphs", exist_ok=True)
    
    for wl in WORKLOADS:
        prefix = f"wl{wl[-1]}"
        
        def apply_plot(ax, metric_dict, x_vals, ylabel, title, filename):
            plt.figure(figsize=(8,5))
            plt.plot(x_vals, metric_dict["occ"][wl], marker='o', label="OCC", color='#1f77b4')
            plt.plot(x_vals, metric_dict["2pl"][wl], marker='s', label="2PL", color='#d62728')
            if x_vals == CONTENTIONS:
                plt.xlabel("Contention Probability")
            else:
                plt.xlabel("Number of Threads")
            plt.ylabel(ylabel)
            plt.title(title)
            plt.legend()
            plt.grid(True)
            plt.savefig(f"graphs/{prefix}_{filename}.png")
            plt.close()

        # 1. Aborts vs Contention
        apply_plot(plt, results["aborts_vs_contention"], CONTENTIONS, "Number of Aborts (Out of 5000 Txns)", f"Aborts vs Contention ({wl.capitalize()}, Threads=4)", "aborts_vs_contention")
        
        # 2. Throughput vs Threads
        apply_plot(plt, results["thru_vs_threads"], THREADS, "Throughput (Txns/Sec)", f"Throughput vs Threads ({wl.capitalize()}, Contention=0.5)", "thru_vs_threads")
        
        # 3. Throughput vs Contention
        apply_plot(plt, results["thru_vs_contention"], CONTENTIONS, "Throughput (Txns/Sec)", f"Throughput vs Contention ({wl.capitalize()}, Threads=4)", "thru_vs_contention")
        
        # 4. Response Time vs Threads
        apply_plot(plt, results["resp_vs_threads"], THREADS, "Average Response Time (us)", f"Response Time vs Threads ({wl.capitalize()}, Contention=0.5)", "resp_vs_threads")
        
        # 5. Response Time vs Contention
        apply_plot(plt, results["resp_vs_contention"], CONTENTIONS, "Average Response Time (us)", f"Response Time vs Contention ({wl.capitalize()}, Threads=4)", "resp_vs_contention")
        
        # 6. Response Time Distribution
        plt.figure(figsize=(8,5))
        plt.hist(results["distributions"]["occ"][wl], bins=50, alpha=0.5, label="OCC", color='#1f77b4', log=True)
        plt.hist(results["distributions"]["2pl"][wl], bins=50, alpha=0.5, label="2PL", color='#d62728', log=True)
        plt.xlabel("Response Time (us)")
        plt.ylabel("Frequency (Log Scale)")
        plt.title(f"Response Time Distribution ({wl.capitalize()}, Contention=0.9, Threads=4)")
        plt.legend()
        plt.grid(True)
        plt.savefig(f"graphs/{prefix}_resp_distribution.png")
        plt.close()

    print("Graphs generated in the 'graphs' directory.")

if __name__ == "__main__":
    r = run_all()
    plot_results(r)
    
    print("\n--- Summary Data ---")
    import pprint
    pprint.pprint({k: v for k, v in r.items() if k != "distributions"})
