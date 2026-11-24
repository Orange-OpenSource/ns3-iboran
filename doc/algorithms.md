# Algorithms

This document lists the built-in algorithms of the dApp and xApp MAC schedulers (see [apps](apps.md)). The dApp algorithm is set with `--dapp_algo_ue_selection` and the xApp algorithm with `--xapp_algo_rbg_bounding` (see [parameters](usage.md#parameters)).

## Variants

Every algorithm comes in two variants:

| Variant | Name | Behavior |
|---|---|---|
| Standard | `<Algorithm>` | Applies the policy to the reported measurements only. |
| Intent-based | `ib<Algorithm>` | Incorporates the intent of each link direction in addition to the measurements, prioritizing the candidate UEs whose content matches the intent. |

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

The metric-driven families use one of five metrics: CQI, buffer size, proportional fairness, content criticality, or a weighted combination of these (multi-criteria). Each of these families has one algorithm per metric, named `<Family><Metric>`. The intent-based variant adds the prefix `ib`.

| Metric | Top-k | Greedy | DP |
|---|---|---|---|
| CQI | `TopKCqi` | `GreedyCqi` | `DpCqi` |
| Buffer size | `TopKBufferSize` | `GreedyBufferSize` | `DpBufferSize` |
| Proportional fairness | `TopKPropFairness` | `GreedyPropFairness` | `DpPropFairness` |
| Content criticality | `TopKCriticality` | `GreedyCriticality` | `DpCriticality` |
| Multi-criteria | `TopKMultiCriteria` | `GreedyMultiCriteria` | `DpMultiCriteria` |

| Algorithm | Name | Description |
|---|---|---|
| Random | `Random` | Selects UEs uniformly at random, seeded by `--rng_run` |
| Round-robin | `RoundRobin` | Selects UEs in circular order |

## xApp RBG bounding

The xApp algorithms set, for each candidate UE, the maximum number of RBGs the dApp may grant in one TTI or slot. A bound holds until the xApp replaces it.

| Algorithm | Name | Description |
|---|---|---|
| Bypass | `Bypass` | Gives every candidate UE the full number of available RBGs, so the xApp does not constrain the dApp. Serves as a baseline. |
| Clamp | `Clamp` | Bounds the RBG grant of each candidate UE to its current demand |
