#!/usr/bin/env python

import sys
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

# File input from command line
if len(sys.argv) != 2:
    print("Usage: ./plot.py <input_file>")
    sys.exit(1)

filename = sys.argv[1]

# Prepare data
labels = []
values = []
total_instructions = None

seen_keys = set()

with open(filename) as f:
    for line in f:
        if line.startswith("Total Instructions:"):
            total_instructions = int(line.split(":")[1].strip())
        elif ":" in line and any(keyword in line for keyword in [
            "Total-Branches", "Conditional-Taken-Branches", 
            "Conditional-NotTaken-Branches", "Unconditional-Branches", 
            "Calls", "Returns"
        ]):
            key, val = line.split(":")
            key = key.strip()
            if key in seen_keys:
                continue  # Skip duplicate keys
            seen_keys.add(key)
            labels.append(key)
            values.append(int(val.strip()))

if total_instructions is None:
    print("Total Instructions not found in file.")
    sys.exit(1)

# Normalize to MPKI (if desired, optional)
per_million = [(v / 1000000.0) for v in values]

# Plotting
fig, ax = plt.subplots()
x = range(len(labels))
ax.bar(x, per_million, color="skyblue")
ax.set_xticks(x)
ax.set_xticklabels(labels, rotation=45, ha="right")
ax.set_ylabel("Amount (millions)")
ax.set_title("Branch Statistics")
ax.grid(True, axis='y')

plt.tight_layout()
plt.savefig(sys.argv[1] + ".png",bbox_inches="tight")
