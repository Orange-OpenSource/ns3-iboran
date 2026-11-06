# Algorithms

This document lists the built-in algorithms of the dApp and xApp MAC schedulers (see [apps](apps.md)). The dApp algorithm is set with `--dapp_algo_ue_selection` and the xApp algorithm with `--xapp_algo_rbg_bounding` (see [parameters](usage.md#parameters)).

## Variants

Every algorithm comes in two variants:

| Variant | Name | Behavior |
|---|---|---|
| Standard | `<Algorithm>` | Applies the policy to the reported KPMs only. |
| Intent-based | `ib<Algorithm>` | Incorporates the intent of each link direction in addition to the KPMs, prioritizing the candidate UEs whose content matches the intent. |

> [!NOTE]
> Learning-based algorithms for the dApp and the xApp are implemented and will be publicly released in an upcoming version.

## dApp UE selection

The dApp algorithms select the UEs to schedule in each TTI or slot. `--k` sets the maximum number of selected UEs, and only UEs with data in their buffer are candidates. The algorithms fall into five families:

| Family | Strategy |
|---|---|
| Random and round-robin | Baselines with no metric-driven selection |
| Top-k | Selects the *k* UEs with the highest metric value |
| Greedy | Selects the *k* UEs with the highest metric value per RBG demanded |
| DP | Selects the UE set that maximizes the summed metric within the available RBGs, solved as a knapsack problem with dynamic programming |

The metric-driven families use one of five metrics: CQI, buffer occupancy, proportional fairness, traffic criticality, or a weighted combination of these (multi-criteria).

| Algorithm | Standard | Intent-based | Description |
|---|---|---|---|
| Random | `Random` | `ibRandom` | Selects UEs uniformly at random, seeded by `--rng_run` |
| Round-robin | `RoundRobin` | `ibRoundRobin` | Selects UEs in circular order |
| Top-k CQI | `TopKCqi` | `ibTopKCqi` | Selects the *k* UEs with the highest CQI |
| Top-k buffer | `TopKBufferLevel` | `ibTopKBufferLevel` | Selects the *k* UEs with the largest buffer occupancy |
| Top-k proportional fairness | `TopKPropFairness` | `ibTopKPropFairness` | Selects the *k* UEs with the highest proportional fairness metric |
| Top-k criticality | `TopKCriticality` | `ibTopKCriticality` | Selects the *k* UEs with the highest traffic criticality |
| Top-k multi-criteria | `TopKMultiCriteria` | `ibTopKMultiCriteria` | Selects the *k* UEs with the highest multi-criteria score |
| Greedy CQI | `GreedyCqi` | `ibGreedyCqi` | Selects the *k* UEs with the highest CQI per RBG demanded |
| Greedy buffer | `GreedyBufferLevel` | `ibGreedyBufferLevel` | Selects the *k* UEs with the largest buffer occupancy per RBG demanded |
| Greedy proportional fairness | `GreedyPropFairness` | `ibGreedyPropFairness` | Selects the *k* UEs with the highest proportional fairness metric per RBG demanded |
| Greedy criticality | `GreedyCriticality` | `ibGreedyCriticality` | Selects the *k* UEs with the highest traffic criticality per RBG demanded |
| Greedy multi-criteria | `GreedyMultiCriteria` | `ibGreedyMultiCriteria` | Selects the *k* UEs with the highest multi-criteria score per RBG demanded |
| DP CQI | `DpCqi` | `ibDpCqi` | Maximizes the summed CQI within the available RBGs |
| DP buffer | `DpBufferLevel` | `ibDpBufferLevel` | Maximizes the summed buffer occupancy within the available RBGs |
| DP proportional fairness | `DpPropFairness` | `ibDpPropFairness` | Maximizes the summed proportional fairness metric within the available RBGs |
| DP criticality | `DpCriticality` | `ibDpCriticality` | Maximizes the summed traffic criticality within the available RBGs |
| DP multi-criteria | `DpMultiCriteria` | `ibDpMultiCriteria` | Maximizes the summed multi-criteria score within the available RBGs |

## xApp RBG bounding

The xApp algorithms set, for each candidate UE, the maximum number of RBGs the dApp may grant in one TTI or slot. A bound holds until the xApp replaces it.

| Algorithm | Standard | Intent-based | Description |
|---|---|---|---|
| Bypass | `Bypass` | `ibBypass` | Gives every candidate UE the full number of available RBGs, so the xApp does not constrain the dApp. Serves as a baseline. |
| Clamp | `Clamp` | `ibClamp` | Bounds the RBG grant of each candidate UE to its current demand |
