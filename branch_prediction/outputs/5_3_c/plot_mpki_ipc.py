#!/usr/bin/env python

import sys
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

# Desired order
desired_order = ["FSM1_16K-2", "FSM2_16K-2", "FSM3_16K-2", "FSM4_16K-2", "FSM5_16K-2", "Nbit-32K-1", "Nbit-8K-4" ]

# Predictor prefix to look for
predictors_to_plot = [ "FSM", "Nbit" ]


# Data collection
mpki_data = {}

# Read input file
with open(sys.argv[1]) as fp:
    for line in fp:
        tokens = line.split()
        if line.startswith("Total Instructions:"):
            total_ins = int(tokens[2])
        else:
            for pred_prefix in predictors_to_plot:
                if line.strip().startswith(pred_prefix):
                    name = tokens[0].strip(':')
                    correct = int(tokens[1])
                    incorrect = int(tokens[2])
                    mpki = incorrect / (total_ins / 1000.0)
                    mpki_data[name] = mpki

# Organize data in the specified order
x_Axis = []
mpki_Axis = []

for name in desired_order:
    if name in mpki_data:
        x_Axis.append(name)
        mpki_Axis.append(mpki_data[name])

# Plotting
fig, ax1 = plt.subplots()
ax1.grid(True)

xAx = np.arange(len(x_Axis))
ax1.set_xticks(xAx)
ax1.set_xticklabels(x_Axis, rotation=45)
ax1.set_xlim(-0.5, len(x_Axis) - 0.5)
ax1.set_ylim(min(mpki_Axis) - 0.05, max(mpki_Axis) + 0.05)
ax1.set_ylabel("$MPKI$")
ax1.plot(xAx, mpki_Axis, label="mpki", color="red", marker='x')

plt.title("MPKI")
plt.tight_layout()
plt.savefig(sys.argv[1] + ".png",bbox_inches="tight")