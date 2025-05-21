import os
import re
import math
import matplotlib.pyplot as plt
from scipy.stats import gmean # For geometric mean

# --- Configuration based on your assignment ---
L1_HIT_TIME = 1
L2_HIT_TIME = 15
MEMORY_ACCESS_TIME = 250 # L2 Miss Penalty

# --- Function to parse a single .out file ---
def parse_output_file(filepath):
    """
    Parses a single .out file and extracts relevant metrics.
    Returns a dictionary with the metrics or None if parsing fails.
    """
    data = {'filepath': filepath} # Store filepath for debugging
    try:
        with open(filepath, 'r') as f:
            content = f.read()

            # --- Extracting General Stats ---
            ipc_match = re.search(r"IPC:\s*([0-9.]+)", content)
            if ipc_match: data['ipc'] = float(ipc_match.group(1))
            else: data['ipc'] = float('nan') # Handle missing IPC

            total_instructions_match = re.search(r"Total Instructions:\s*(\d+)", content)
            if total_instructions_match: data['total_instructions'] = int(total_instructions_match.group(1))
            else: data['total_instructions'] = 0 # Important for MPKI, handle if missing

            # --- Extracting L1 Cache Stats ---
            l1_total_misses_match = re.search(r"L1-Total-Misses:\s*(\d+)", content)
            if l1_total_misses_match: data['l1_total_misses'] = int(l1_total_misses_match.group(1))
            else: data['l1_total_misses'] = 0

            l1_total_accesses_match = re.search(r"L1-Total-Accesses:\s*(\d+)", content)
            if l1_total_accesses_match: data['l1_total_accesses'] = int(l1_total_accesses_match.group(1))
            else: data['l1_total_accesses'] = 0

            # --- Extracting L2 Cache Stats ---
            l2_total_misses_match = re.search(r"L2-Total-Misses:\s*(\d+)", content)
            if l2_total_misses_match: data['l2_total_misses'] = int(l2_total_misses_match.group(1))
            else: data['l2_total_misses'] = 0

            l2_total_accesses_match = re.search(r"L2-Total-Accesses:\s*(\d+)", content) # These are L1 misses
            if l2_total_accesses_match: data['l2_total_accesses'] = int(l2_total_accesses_match.group(1))
            else: data['l2_total_accesses'] = 0


            # --- Extract L2 Cache Configuration for Labels ---
            l2_size_kb_match = re.search(r"L2-Data Cache:\s*Size\(KB\):\s*(\d+)", content)
            # Using re.DOTALL because associativity and block size might be on different lines or spaced out
            l2_assoc_match = re.search(r"L2-Data Cache:.*?Associativity:\s*(\d+)", content, re.DOTALL)
            l2_block_b_match = re.search(r"L2-Data Cache:.*?Block Size\(B\):\s*(\d+)", content, re.DOTALL)

            if l2_size_kb_match and l2_assoc_match and l2_block_b_match:
                data['l2_config_str'] = f"{l2_size_kb_match.group(1)}KB-{l2_assoc_match.group(1)}way-{l2_block_b_match.group(1)}B"
            else:
                # Fallback if config string isn't found, use filename or part of it
                data['l2_config_str'] = os.path.basename(filepath).replace(".out", "")


            # --- Calculate Metrics ---
            l1_miss_rate = 0
            if data['l1_total_accesses'] > 0:
                l1_miss_rate = data['l1_total_misses'] / data['l1_total_accesses']

            l2_miss_rate_local = 0 # Miss rate local to L2
            if data['l2_total_accesses'] > 0: # L2_total_accesses are L1 misses
                l2_miss_rate_local = data['l2_total_misses'] / data['l2_total_accesses']

            data['amat'] = L1_HIT_TIME + l1_miss_rate * (L2_HIT_TIME + l2_miss_rate_local * MEMORY_ACCESS_TIME)

            data['l2_mpki'] = 0
            if data['total_instructions'] > 0:
                data['l2_mpki'] = (data['l2_total_misses'] / data['total_instructions']) * 1000
            else: # If total_instructions is 0 or missing, MPKI is undefined or infinite
                data['l2_mpki'] = float('nan')


            # Validate essential data (IPC is already handled with nan)
            if data['total_instructions'] == 0:
                 print(f"Warning: Total Instructions is 0 or missing in {filepath}. MPKI will be NaN.")


            return data

    except Exception as e:
        print(f"Error parsing file {filepath}: {e}")
        return {'ipc': float('nan'), 'amat': float('nan'), 'l2_mpki': float('nan'),
                'l2_config_str': os.path.basename(filepath) + "_parse_error", 'filepath': filepath}

# --- Main Processing Logic ---
def process_benchmarks(base_dir="."):
    benchmark_dirs = [d for d in os.listdir(base_dir) if os.path.isdir(os.path.join(base_dir, d)) and not d.startswith('.')] # Basic filter
    if not benchmark_dirs:
        print(f"Error: No benchmark subdirectories found in {base_dir}.")
        print("Please ensure your script is in the parent directory of the benchmark folders,")
        print("or specify the correct 'base_dir'.")
        return

    print(f"Found benchmark directories: {benchmark_dirs}")

    first_benchmark_path = os.path.join(base_dir, benchmark_dirs[0])
    try:
        config_files_ordered = sorted([f for f in os.listdir(first_benchmark_path) if f.endswith(".out")])
        if not config_files_ordered:
            print(f"Error: No .out files found in the first benchmark directory: {first_benchmark_path}")
            return
        print(f"Determined {len(config_files_ordered)} configurations from {benchmark_dirs[0]}: {config_files_ordered[:5]}...") # Print first 5
    except FileNotFoundError:
        print(f"Error: Could not access files in the first benchmark directory: {first_benchmark_path}")
        return

    num_configurations = len(config_files_ordered)
    if num_configurations != 21: # As per user's initial description
        print(f"Warning: Expected 21 configurations based on initial description, but found {num_configurations} files in {benchmark_dirs[0]}. Proceeding with {num_configurations}.")


    ipc_data = [[] for _ in range(num_configurations)]
    amat_data = [[] for _ in range(num_configurations)]
    l2_mpki_data = [[] for _ in range(num_configurations)]
    config_labels = [""] * num_configurations

    for bench_idx, bench_dir_name in enumerate(benchmark_dirs):
        bench_path = os.path.join(base_dir, bench_dir_name)
        print(f"\nProcessing Benchmark: {bench_dir_name}")

        current_bench_files = sorted([f for f in os.listdir(bench_path) if f.endswith(".out")])
        # Simple check based on the number of files found in the first benchmark dir.
        # More robust matching might be needed if filenames aren't identical across benchmarks
        # but represent the same logical configuration.
        if len(current_bench_files) != num_configurations:
            print(f"  Warning: Benchmark {bench_dir_name} has {len(current_bench_files)} .out files, but expected {num_configurations} based on '{benchmark_dirs[0]}'. Data alignment might be affected if files don't correspond one-to-one with the order from the first benchmark.")


        for config_idx in range(num_configurations):
            # Use the filename from the 'config_files_ordered' list (derived from the first benchmark)
            # to ensure we are trying to pick the 'same' configuration file.
            # This assumes that if benchmark1 has config_A.out, config_B.out, config_C.out (sorted),
            # then benchmark2 also has config_A.out, config_B.out, config_C.out in that sorted order,
            # or at least files that correspond to those configurations when sorted.
            if config_idx >= len(config_files_ordered): # Should not happen if logic is correct
                break

            config_filename = config_files_ordered[config_idx]
            filepath = os.path.join(bench_path, config_filename)

            if not os.path.exists(filepath):
                print(f"  Warning: File {filepath} (expected for config {config_idx+1}) not found in {bench_dir_name}. Adding NaN for this data point.")
                ipc_data[config_idx].append(float('nan'))
                amat_data[config_idx].append(float('nan'))
                l2_mpki_data[config_idx].append(float('nan'))
                if not config_labels[config_idx]:
                    config_labels[config_idx] = f"Config_{config_idx+1}_DataMissing"
                continue

            parsed_metrics = parse_output_file(filepath)

            ipc_data[config_idx].append(parsed_metrics['ipc'])
            amat_data[config_idx].append(parsed_metrics['amat'])
            l2_mpki_data[config_idx].append(parsed_metrics['l2_mpki'])

            if not config_labels[config_idx] or config_labels[config_idx].endswith("_DataMissing"):
                config_labels[config_idx] = parsed_metrics['l2_config_str']


    geo_mean_ipc = []
    geo_mean_amat = []
    geo_mean_l2_mpki = []

    for i in range(num_configurations):
        valid_ipc = [x for x in ipc_data[i] if not math.isnan(x) and x > 0]
        valid_amat = [x for x in amat_data[i] if not math.isnan(x) and x > 0]
        valid_l2_mpki = [x for x in l2_mpki_data[i] if not math.isnan(x)] # MPKI can be 0

        if valid_ipc: geo_mean_ipc.append(gmean(valid_ipc))
        else: geo_mean_ipc.append(float('nan'))

        if valid_amat: geo_mean_amat.append(gmean(valid_amat))
        else: geo_mean_amat.append(float('nan'))

        if valid_l2_mpki:
            if all(v == 0 for v in valid_l2_mpki):
                geo_mean_l2_mpki.append(0.0)
            else:
                positive_l2_mpki = [x for x in valid_l2_mpki if x > 0]
                if positive_l2_mpki:
                    geo_mean_l2_mpki.append(gmean(positive_l2_mpki))
                elif any(v == 0 for v in valid_l2_mpki):
                    geo_mean_l2_mpki.append(0.0)
                else:
                    geo_mean_l2_mpki.append(float('nan'))
        else:
            geo_mean_l2_mpki.append(float('nan'))

    x_ticks_pos = range(num_configurations)
    plt.style.use('seaborn-v0_8-whitegrid')

    # --- Plotting (Changed to Bar Charts) ---

    # Plot IPC
    plt.figure(figsize=(15, 7))
    plt.bar(x_ticks_pos, geo_mean_ipc, color='dodgerblue', label='Geo. Mean IPC', width=0.6) # Changed to plt.bar()
    plt.title('Geometric Mean of IPC vs. L2 Cache Configuration', fontsize=16)
    plt.xlabel('L2 Cache Configuration', fontsize=14)
    plt.ylabel('Geometric Mean IPC', fontsize=14)
    plt.xticks(x_ticks_pos, config_labels, rotation=90, fontsize=9)
    plt.grid(True, axis='y', linestyle='--', linewidth=0.5) # Grid on y-axis can be helpful for bars
    plt.legend()
    plt.tight_layout()
    plt.savefig("ipc.png", dpi=300)
    print("\nSaved IPC bar chart to geometric_mean_ipc_barchart.png")
    plt.show()

    # Plot AMAT
    plt.figure(figsize=(15, 7))
    plt.bar(x_ticks_pos, geo_mean_amat, color='crimson', label='Geo. Mean AMAT', width=0.6) # Changed to plt.bar()
    plt.title('Geometric Mean of AMAT vs. L2 Cache Configuration', fontsize=16)
    plt.xlabel('L2 Cache Configuration', fontsize=14)
    plt.ylabel('Geometric Mean AMAT (cycles)', fontsize=14)
    plt.xticks(x_ticks_pos, config_labels, rotation=90, fontsize=9)
    plt.grid(True, axis='y', linestyle='--', linewidth=0.5)
    plt.legend()
    plt.tight_layout()
    plt.savefig("amat.png", dpi=300)
    print("Saved AMAT bar chart to geometric_mean_amat_barchart.png")
    plt.show()

    # Plot L2 MPKI
    plt.figure(figsize=(15, 7))
    plt.bar(x_ticks_pos, geo_mean_l2_mpki, color='forestgreen', label='Geo. Mean L2 MPKI', width=0.6) # Changed to plt.bar()
    plt.title('Geometric Mean of L2 MPKI vs. L2 Cache Configuration', fontsize=16)
    plt.xlabel('L2 Cache Configuration', fontsize=14)
    plt.ylabel('Geometric Mean L2 MPKI', fontsize=14)
    plt.xticks(x_ticks_pos, config_labels, rotation=90, fontsize=9)
    plt.grid(True, axis='y', linestyle='--', linewidth=0.5)
    plt.legend()
    plt.tight_layout()
    plt.savefig("mpki.png", dpi=300)
    print("Saved L2 MPKI bar chart to geometric_mean_l2_mpki_barchart.png")
    plt.show()

    print("\n--- Summary of Geometric Means ---")
    for i in range(num_configurations):
        print(f"Config: {config_labels[i] if i < len(config_labels) else f'Config_{i+1}_LabelError'}") # Safety check for config_labels length
        ipc_val = geo_mean_ipc[i] if i < len(geo_mean_ipc) else float('nan')
        amat_val = geo_mean_amat[i] if i < len(geo_mean_amat) else float('nan')
        mpki_val = geo_mean_l2_mpki[i] if i < len(geo_mean_l2_mpki) else float('nan')

        print(f"  IPC: {ipc_val:.4f if not math.isnan(ipc_val) else 'NaN'}")
        print(f"  AMAT: {amat_val:.2f if not math.isnan(amat_val) else 'NaN'} cycles")
        print(f"  L2 MPKI: {mpki_val:.3f if not math.isnan(mpki_val) else 'NaN'}")

if __name__ == "__main__":
    process_benchmarks()