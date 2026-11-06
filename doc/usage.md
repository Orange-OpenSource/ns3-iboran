# Usage

This document describes how to run ib-ORAN simulations and campaigns. Paths are relative to `contrib/iboran`.

## Single run

A single simulation runs from `model/iboran_campaign`:

```shell
python3 iboran_run.py [--rat_type RAT_TYPE] [--k K] [--dapp_algo_ue_selection ALGO] ...
```

All parameters are optional. An omitted parameter takes its value from [`iboranConfig.yaml`](../model/iboran_campaign/iboranConfig.yaml). The run writes the merged configuration to `notebook/logs/runConfig.yaml`, which the ns-3 program reads.

## Parameters

| Parameter | Description | Values | Config key |
|---|---|---|---|
| `--rat_type` | Radio access technology | `LTE`, `NR` | `rat_type` |
| `--rng_run` | Replication index, used as ns-3 `RngRun` and as the seed of every other random generator (see [reproducibility](#reproducibility)) | Integer, 1 or more | `common.rngRun` |
| `--k` | Maximum number of UEs scheduled in parallel per TTI or slot | Integer | `orchestrator.k` |
| `--dapp_control_period_sf` | dApp control period, in subframes | Integer | `orchestrator.dapp_control_period_sf` |
| `--xapp_control_period_f` | xApp control period, in frames | Integer | `orchestrator.xapp_control_period_f` |
| `--dapp_algo_ue_selection` | dApp UE selection algorithm | See [algorithms](algorithms.md#dapp-ue-selection) | `orchestrator.dapp_algo_ue_selection` |
| `--xapp_algo_rbg_bounding` | xApp RBG bounding algorithm | See [algorithms](algorithms.md#xapp-rbg-bounding) | `orchestrator.xapp_algo_rbg_bounding` |
| `--dataset_id` | Dataset identifier (see [simulation data](simulation-data.md#datasets)) | `toy`, `market-1501` | `orchestrator.dataset_id` |
| `--data_format` | Application payload format (see [simulation data](simulation-data.md#payload-formats)) | `IMAGE`, `EMBEDDING` | `orchestrator.data_format` |
| `--embedder` | Embedding extraction model (see [simulation data](simulation-data.md#embedding-models)) | `TransReID`, `CLIP` | `orchestrator.embedder` |
| `--oid_range` | Inclusive range of object IDs for UE assignment and intent generation (see [simulation data](simulation-data.md#object-ids-and-intents)) | `low,high` | `orchestrator.oid_range` |
| `--save_dapp_log` | Save the dApp control log to a file | `true`, `false` | `orchestrator.save_dapp_log` |
| `--verbose` | Print run statistics and a KPI summary to the console | `true`, `false` | `orchestrator.verbose` |

## Examples

Run with the values of the configuration file:

```shell
python3 iboran_run.py
```

Run NR with the intent-based top-k CQI scheduler, `k=3`, replication 5:

```shell
python3 iboran_run.py --rat_type="NR" --k=3 --dapp_algo_ue_selection="ibTopKCqi" --rng_run=5
```

## Campaign

[`iboran_campaign.py`](../model/iboran_campaign/iboran_campaign.py) defines `IbOranCampaign`, which sweeps parameters and control algorithms over Monte Carlo replications and calls `iboran_run.py` once per configuration and replication. The notebook [`iboran_evaluation.ipynb`](../notebook/iboran_evaluation.ipynb) sets up a campaign and compares scheduling algorithms on the [KPIs](kpis.md).

A campaign caches the KPI records of each run and reuses them on the next call with the same settings (see [cached results](simulation-data.md#cached-results)).

## Reproducibility

`--rng_run` fixes one replication. Two runs with the same parameters and the same `--rng_run` give identical results. Every random element derives from it: the ns-3 random variable streams, the assignment of object IDs to UEs, the generated intents, and the `Random` scheduler. Runs of different algorithms with the same `--rng_run` share the same channel and traffic realization, which allows paired comparisons. `IbOranCampaign` uses `--rng_run = mc_idx + 1` for Monte Carlo iteration `mc_idx`.
