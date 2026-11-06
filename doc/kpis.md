# KPIs

This document defines the KPIs that ib-ORAN reports for each run. KPIs are reported per link direction and written to `notebook/logs/kpi.json` (see [simulation data](simulation-data.md#outputs)). Paths are relative to `contrib/iboran`.

## Standard KPIs

| KPI | Definition | Unit |
|---|---|---|
| `PDR` | Received packets divided by transmitted packets, over all flows of the link | % |
| `Throughput` | Received bits over the active period of the link (first transmission to last reception), divided by the number of flows | kbps per UE |
| `Latency` | Mean delay of the received packets | ms |
| `Jitter` | Mean packet delay variation | ms |

## Intent-based KPIs

| KPI | Definition | Unit |
|---|---|---|
| `FidelityScore` | Fidelity of the recovered payload of a UE to the original, computed by the KPI module of the payload format in [`model/iboran_kpi/`](../model/iboran_kpi) | - |
| `SatisfactionScore` | Mean `FidelityScore` over the intent-relevant UEs of the link. A relevant UE without a usable output counts as 0. | - |

A link with no intent-relevant UE reports a `SatisfactionScore` of NaN, which campaign averages skip (see [object IDs and intents](simulation-data.md#object-ids-and-intents)).

## Control KPIs

| KPI | Definition | Unit |
|---|---|---|
| `PRBUtilizationRatio` | PRBs allocated by the dApp divided by PRBs offered, at the dApp decision steps | % |
| `DAppDecisionTime` | Mean wall-clock time per dApp decision | ms |
| `XAppDecisionTime` | Mean wall-clock time per xApp decision | ms |