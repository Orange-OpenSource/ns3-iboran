# Message Structure Reference

This document lists the control messages exchanged through the E2 and E3 interfaces, organized by service model and message type. Each table is split into **Collective** fields (carried once per message) and **Per-UE** fields (repeated for each UE in the payload).

## E2 Service Model

### E2SM-KPM — RIC REPORT

> Reported periodically from the O-DU to the near-RT RIC, carrying radio performance indicators.

| Collective Message Content | Unit | Valid Range | Data Type | Per-UE Message Content | Unit | Valid Range | Data Type |
|:---|:---:|:---:|:---:|:---|:---:|:---:|:---:|
| Type | — | As defined | `uint16` | RNTI | — | — | `uint16` |
| Timestamp | ns | — | `uint64` | CQI | — | 0–15 | `uint8` |
| Frame Number | — | 0–1023 | `uint32` | Buffer Size | Bytes | — | `uint32` |
| Number of UEs | — | — | `uint32` | QCI | — | 1–9 | `uint8` |
| Total Buffer Size | Bytes | — | `uint32` | Achievable Rate | Mbps | — | `double` |
| Number of RBGs | — | 0–Max RBG | `uint16` | Actual Throughput | Mbps | — | `double` |
| RBG Size | PRB | 1–4 | `int` | Total Bytes Served | Bytes | — | `uint32` |
| Link Direction | — | DOWNLINK / UPLINK | `uint8` | Packet Delay | ms | — | `uint32` |
| ID | — | — | `uint32` | | | | |


### E2SM-RC — RIC CONTROL

> Sent from the near-RT RIC to the O-DU to override or configure MAC scheduling behavior.

| Collective Message Content | Unit | Valid Range | Data Type | Per-UE Message Content | Unit | Valid Range | Data Type |
|:---|:---:|:---:|:---:|:---|:---:|:---:|:---:|
| Type | — | As defined | `uint16` | RNTI | — | — | `uint16` |
| Max UE Selection Size | — | — | `uint32` | Max RBG Allocation | — | 0–Max RBG | `uint16` |
| Skip This Step | — | No / Yes | `bool` | | | | |
| Link Direction | — | DOWNLINK / UPLINK | `uint8` | | | | |
| ID | — | — | `uint32` | | | | |


### E2SM-DAPP — RIC REPORT

> Reported from the dApp (O-DU) to the near-RT RIC, summarizing the executed real-time control step.

| Collective Message Content | Unit | Valid Range | Data Type | Per-UE Message Content | Unit | Valid Range | Data Type |
|:---|:---:|:---:|:---:|:---|:---:|:---:|:---:|
| Type | — | As defined | `int` | | | | |
| Number of Selected UEs | — | — | | | | | |
| Total Number of Allocated RBGs | — | 0–Max RBG | | | | | |
| RBG Size | PRB | 1–4 | | | | | |
| Link Direction | — | DOWNLINK / UPLINK | | | | | |
| ID | — | — | | | | | |


### E2SM-DAPP — RIC CONTROL

> Sent from the near-RT RIC to the dApp (O-DU) to override or configure the upcoming real-time control step.

| Collective Message Content | Unit | Valid Range | Data Type | Per-UE Message Content | Unit | Valid Range | Data Type |
|:---|:---:|:---:|:---:|:---|:---:|:---:|:---:|
| Type | — | As defined | `int` | | | | |
| Max UE Selection Size | — | — | | | | | |
| Total Available RBGs | — | 0–Max RBG | | | | | |
| Skip This Step | — | No / Yes | `bool` | | | | |
| Link Direction | — | DOWNLINK / UPLINK | `int` | | | | |
| ID | — | — | | | | | |


## E3 Service Model
 
### E3SM — E3 INDICATION
 
> Reported from the MAC and RLC layers to the local dApp, carrying radio performance indicators including intent-related information shared by UEs.
 
| Collective Message Content | Unit | Valid Range | Data Type | Per-UE Message Content | Unit | Valid Range | Data Type |
|:---|:---:|:---:|:---:|:---|:---:|:---:|:---:|
| Type | — | As defined | `uint16` | RNTI | — | — | `uint16` |
| Timestamp | ns | — | `uint64` | CQI | — | 0–15 | `uint8` |
| Subframe Number | — | 0–9 | `uint32` | Buffer Size | Bytes | — | `uint32` |
| Frame Number | — | 0–1023 | `uint32` | Content Criticality | — | 1–10 | `uint8` |
| Number of UEs | — | — | `uint32` | Content ID | — | Variant | `char` |
| Total Buffer Size | Bytes | — | `uint32` | QCI | — | 1–9 | `uint8` |
| Initial RBG | — | 0–Max RBG | `uint16` | Achievable Rate | Mbps | — | `double` |
| Number of RBGs | — | 0–Max RBG | `uint16` | Actual Throughput | Mbps | — | `double` |
| RBG Size | PRB | 1–4 | `int` | Total Bytes Served | Bytes | — | `uint32` |
| Link Direction | — | DOWNLINK / UPLINK | `uint8` | Packet Delay | ms | — | `uint32` |
| ID | — | — | `uint32` | | | | |

 
### E3SM — E3 CONTROL
 
> Sent from the local dApp to the MAC scheduler to control its behavior on a real-time basis.
 
| Collective Message Content | Unit | Valid Range | Data Type | Per-UE Message Content | Unit | Valid Range | Data Type |
|:---|:---:|:---:|:---:|:---|:---:|:---:|:---:|
| Type | — | As defined | `uint16` | RNTI | — | — | `uint16` |
| Number of Selected UEs | — | — | `uint32` | Initial Allocated RBG | — | 0–Max RBG | `uint16` |
| Skip This Step | — | No / Yes | `bool` | Number of Allocated RBGs | — | 0–Max RBG | `uint16` |
| Link Direction | — | DOWNLINK / UPLINK | `uint8` | MCS | — | 0–28 | `uint8` |
| ID | — | — | `uint32` | | | | |