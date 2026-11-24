# KPIs

This document defines the KPIs that ib-ORAN reports for each run. KPIs are reported per link direction and written to `notebook/logs/kpi.json` (see [simulation data](simulation-data.md#outputs)). Paths are relative to `contrib/iboran`.

## Standard KPIs

| KPI | Definition | Unit |
|---|---|---|
| `PDR` | Received packets divided by transmitted packets, over the data flows of the link | % |
| `Throughput` | Received bits over the active period of the link (first transmission to last reception), divided by the number of data flows | kbps per UE |
| `Latency` | Mean delay of the received packets | ms |
| `Jitter` | Mean packet delay variation | ms |

These KPIs come from the ns-3 flow monitor and count only data flows: remote host to UE port `2000 + i` (downlink) and UE to remote host port `4000 + i` (uplink), for UE index *i* from 0. Core signaling and TCP acknowledgment flows are excluded. They are per packet: over TCP, a retransmission counts as an extra packet with its own delay, so the wait for recovery shows only in the [message log](application.md#message-log).

> [!WARNING]
> Over TCP, set `appSaveMessageLog: true`. Throughput is then measured from the link's earliest `firstTxNs` to its latest `lastRxNs` in the message log. Without the log, it is measured over the whole connection, from the SYN at application start to the FIN at application stop. That value is not comparable with UDP, and the orchestrator prints a warning.

## Intent-based KPIs

| KPI | Definition | Unit |
|---|---|---|
| `FidelityScore` | Fidelity of the recovered payload to the original, computed per UE by the KPI module of the payload format in [`model/iboran_kpi/`](../model/iboran_kpi), and averaged over all UEs of the link (a UE without a usable output counts as 0) | - |
| `SatisfactionScore` | Sum of the `FidelityScore` of the intent-relevant UEs, divided by the number of UEs of the link, so it never exceeds `FidelityScore` and is 0 on a link with no intent-relevant UE | - |

Before scoring, the receiver fills the lost elements of a payload according to `appFillPolicy` (see [application layer](application.md#reception)). `kpi.json` also holds the other metrics of the KPI module, averaged over all UEs of the link in the same way.

## Control KPIs

| KPI | Definition | Unit |
|---|---|---|
| `PRBUtilizationRatio` | PRBs allocated by the dApp divided by PRBs offered, at the dApp decision steps | % |
| `DAppDecisionTime` | Mean wall-clock time per dApp decision | ms |
| `XAppDecisionTime` | Mean wall-clock time per xApp decision | ms |
