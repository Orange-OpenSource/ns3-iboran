# App Catalog

This catalog lists the Open RAN Apps currently available in ib-ORAN, organized by deployment layer and control timescale. To contribute a new app, see [`CONTRIBUTING.md`](../CONTRIBUTING.md).


## dApps (O-DU)

**Control loop timescale:** < 10 ms  
**Deployed control period:** on the scale of an OFDM subframe

| Name | dApp ID | Description | Additional Parameters |
|---|---|---|---|
| dApp MAC Scheduler | `"MacScheduler"` | Performs MAC-layer UE scheduling at the O-DU in real time, selecting which UEs to serve each TTI/slot based on the configured algorithm. | `k`, `algo_ue_selection` |


## xApps (Near-RT RIC)

**Control loop timescale:** 10 ms to 1 s  
**Deployed control period:** on the scale of an OFDM frame

| Name | xApp ID | Description | Additional Parameters |
|---|---|---|---|
| xApp MAC Scheduler | `"MacScheduler"` | Performs MAC-layer UE scheduling at the near-RT RIC, overriding or guiding the dApp scheduler over a longer control horizon based on the configured algorithm. | `k`, `algo_ue_selection` |
