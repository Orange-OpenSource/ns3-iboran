# Apps

This document lists the Open RAN applications implemented in ib-ORAN, grouped by deployment location and control timescale. To add an app, follow [CONTRIBUTING.md](../CONTRIBUTING.md#adding-an-app) and list it here.

| App type | Location | Control loop timescale | Control period in ib-ORAN |
|---|---|---|---|
| dApp | O-DU | Below 10 ms | Multiples of a subframe (`--dapp_control_period_sf`) |
| xApp | Near-RT RIC | 10 ms to 1 s | Multiples of a frame (`--xapp_control_period_f`) |
| rApp | Non-RT RIC | Above 1 s | Not implemented |

## dApps

| Name | ID | Description | Interfaces | Parameters |
|---|---|---|---|---|
| dApp MAC scheduler | `MacScheduler` | Schedules UEs at the O-DU in real time. At each control step, it selects the UEs to serve in the TTI or slot and assigns their RBGs and MCS, within the per-UE RBG bounds set by the xApp. It reports its per-UE grants to the xApp over E2SM-DAPP. | E3SM, E2SM-DAPP | `--k`, `--dapp_algo_ue_selection` |

The UE selection algorithms are listed in [algorithms](algorithms.md#dapp-ue-selection).

## xApps

| Name | ID | Description | Interfaces | Parameters |
|---|---|---|---|---|
| xApp MAC scheduler | `MacScheduler` | Guides the dApp MAC scheduler over a longer horizon. At each control step, it sets per-UE bounds on the RBGs the dApp may grant in one TTI or slot. | E2SM-KPM, E2SM-RC, E2SM-DAPP | `--xapp_algo_rbg_bounding` |

The RBG bounding algorithms are listed in [algorithms](algorithms.md#xapp-rbg-bounding).

## rApps

> [!NOTE]
> rApps are not implemented yet. This section will list them once Non-RT RIC support is added.