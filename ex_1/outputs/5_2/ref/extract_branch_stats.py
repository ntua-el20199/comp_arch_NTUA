#!/usr/bin/env python3

import sys

def extract_stats(filename):
    stats = {}
    stats_order = [
        'Total-Branches',
        'Conditional-Taken-Branches',
        'Conditional-NotTaken-Branches',
        'Unconditional-Branches',
        'Calls',
        'Returns'
    ]
    
    with open(filename, 'r') as f:
        for line in f:
            for stat in stats_order:
                if stat in line:
                    stats[stat] = int(line.split(':')[1].strip())
    
    # Append all stats in LaTeX table format
    with open(filename, 'a') as f:
        f.write("\n# LaTeX formatted rows:\n")
        for stat in stats_order:
            f.write(f"{stats[stat]} & ")
        f.write("\n")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: ./extract_branch_stats.py <filename>")
        sys.exit(1)
    
    extract_stats(sys.argv[1])