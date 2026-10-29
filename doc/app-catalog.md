# App Catalog

This catalog lists the Open RAN Apps currently available in ib-ORAN, organized by deployment layer and control timescale. To contribute a new app, see [`CONTRIBUTING.md`](../CONTRIBUTING.md).


## dApps (O-DU)

**Control loop timescale:** < 10 ms  
**Deployed control period:** on the scale of an OFDM subframe

| Name | dApp ID | Description | Additional Parameters |
|---|---|---|---|
| dApp MAC Scheduler | `"MacScheduler"` | Performs MAC-layer UE scheduling at the O-DU in real time, selecting which UEs to serve each TTI/slot and their RBGs and MCS, within the per-UE RBG bounds issued by the xApp. Reports its grants per UE to the xApp over E2SM-DAPP. | `k`, `algo_ue_selection` |


## xApps (Near-RT RIC)

**Control loop timescale:** 10 ms to 1 s  
**Deployed control period:** on the scale of an OFDM frame

| Name | xApp ID | Description | Additional Parameters |
|---|---|---|---|
| xApp MAC Scheduler | `"MacScheduler"` | Guides the dApp scheduler over a longer control horizon by issuing per-UE bounds on the RBGs the dApp may grant in one TTI/slot, based on the configured algorithm. | `algo_rbg_bounding` |


## rApps (Non-RT RIC)

> **Note:** rApps are not yet implemented. This section will be populated as non-RT RIC support is added.
