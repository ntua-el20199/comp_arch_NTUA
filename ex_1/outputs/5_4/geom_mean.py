#!/usr/bin/env python

import sys
import matplotlib.pyplot as plt
import numpy as np

if __name__ == "__main__":
    # Get arguments, skipping the script name
    args = sys.argv[1:]

    if not args or len(args) % 2 != 0:
        print("Usage: python geom_mean.py <label1> <value1> <label2> <value2> ...")
        sys.exit(1)

    labels = []
    values = []
    try:
        # Process arguments in pairs (label, value)
        for i in range(0, len(args), 2):
            labels.append(args[i])
            values.append(float(args[i+1]))
    except ValueError:
        print("Error: Values must be numeric.")
        sys.exit(1)
    except IndexError:
         print("Error: Mismatched number of labels and values.")
         sys.exit(1)


    # Create the bar chart
    plt.figure(figsize=(10, 6)) # Adjust figure size as needed
    plt.bar(labels, values)

    # Add labels and title
    plt.xlabel("Prediction Model")
    plt.ylabel("Mean MPKI")
    plt.title("")
    plt.xticks(rotation=45, ha='right', fontsize=12) # Rotate labels if they overlap
    plt.tight_layout() # Adjust layout to prevent labels overlapping

    # Save the plot to a file instead of displaying it
    plt.savefig('geom_mean.png')
    # plt.show() # Remove or comment out this line