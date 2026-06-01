# Built-In Algorithms

This document lists the built-in MAC scheduling algorithms available in ib-ORAN for both the dApp and xApp layers.

Each algorithm (if applicable) is available in two variants:

- **Standard** — applies the scheduling policy directly on raw KPMs.
- **Intent-Based (`iB`)** — extends the standard variant with intent-based, semantics-aware decision-making.

## dApp MAC Scheduling

Algorithms are grouped into four families:

| Family | Strategy |
|---|---|
| **Random / Round-Robin** | Baseline schedulers with no metric-driven selection |
| **Top-k** | Selects the top *k* UEs by a given metric; *k* is set via `--k` |
| **Greedy** | Iteratively selects the best UE by a given metric until *k* is reached |
| **DP** *(Dynamic Programming)* | Optimally selects *k* UEs by solving the selection problem exactly |

| Algorithm | Standard | Intent-Based | Description |
|---|---|---|---|
| **Random** | `"Random"` | `"iBRandom"` | Selects UEs uniformly at random |
| **Round-Robin** | `"RoundRobin"` | `"iBRoundRobin"` | Selects UEs in circular order |
| **Top-k: CQI** | `"TopKCqi"` | `"iBTopKCqi"` | Selects the *k* UEs with the highest Channel Quality Indicator |
| **Top-k: Buffer Size** | `"TopKBufferLevel"` | `"iBTopKBufferLevel"` | Selects the *k* UEs with the largest buffer occupancy |
| **Top-k: Proportional Fairness** | `"TopKPropFairness"` | `"iBTopKPropFairness"` | Selects the *k* UEs maximizing the proportional fairness metric |
| **Top-k: Criticality** | `"TopKCriticality"` | `"iBTopKCriticality"` | Selects the *k* UEs with the highest traffic criticality |
| **Top-k: Multi-Criteria** | `"TopKMultiCriteria"` | `"iBTopKMultiCriteria"` | Selects the *k* UEs ranked by a combination of multiple metrics |
| **Greedy: CQI** | `"GreedyCqi"` | `"iBGreedyCqi"` | Greedily selects up to *k* UEs by highest CQI |
| **Greedy: Buffer Size** | `"GreedyBufferLevel"` | `"iBGreedyBufferLevel"` | Greedily selects up to *k* UEs by largest buffer occupancy |
| **Greedy: Proportional Fairness** | `"GreedyPropFairness"` | `"iBGreedyPropFairness"` | Greedily selects up to *k* UEs by proportional fairness metric |
| **Greedy: Criticality** | `"GreedyCriticality"` | `"iBGreedyCriticality"` | Greedily selects up to *k* UEs by highest traffic criticality |
| **Greedy: Multi-Criteria** | `"GreedyMultiCriteria"` | `"iBGreedyMultiCriteria"` | Greedily selects up to *k* UEs by a combination of multiple metrics |
| **DP: CQI** | `"DpCqi"` | `"iBDpCqi"` | Optimally selects *k* UEs via dynamic programming based on CQI |
| **DP: Buffer Size** | `"DpBufferLevel"` | `"iBDpBufferLevel"` | Optimally selects *k* UEs via dynamic programming based on buffer occupancy |
| **DP: Proportional Fairness** | `"DpPropFairness"` | `"iBDpPropFairness"` | Optimally selects *k* UEs via dynamic programming based on proportional fairness |
| **DP: Criticality** | `"DpCriticality"` | `"iBDpCriticality"` | Optimally selects *k* UEs via dynamic programming based on traffic criticality |
| **DP: Multi-Criteria** | `"DpMultiCriteria"` | `"iBDpMultiCriteria"` | Optimally selects *k* UEs via dynamic programming based on multiple metrics |


## xApp MAC Scheduling

The xApp scheduler operates at the near-RT RIC level and guides the dApp over a longer control horizon.

| Algorithm | ID | Description |
|---|---|---|
| **Bypass** | `"Bypass"` | Passes control through to the dApp without modification; used as a baseline or when xApp-level intervention is not required |
