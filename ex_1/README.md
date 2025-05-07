# Branch Predictor Simulation Project

This repository contains the implementation and evaluation of various branch predictors for processor architecture. The goal is to compare the performance of different branch prediction strategies and understand the impact of key parameters such as entries, associativity, and history length on prediction accuracy.

## Simulations

We have performed simulations to compare the performance of the above predictors using both **reference (ref)** and **training (train)** input sets. The metrics used for comparison include the **Misses Per Thousand Instructions (MPKI)** and the **hardware cost** (estimated in terms of entries and associativity).