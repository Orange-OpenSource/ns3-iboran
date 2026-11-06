# Service models

This document lists the fields of the messages exchanged over the E2 and E3 interfaces, by service model and message type. Each message has collective fields, carried once per message, and per-UE fields, repeated for each UE in the message.

Conventions used in the tables:

- *Type* is the message type code of `MessageType` in [`iboran_shm_protocol.py`](../model/iboran_interfaces/iboran_shm_protocol.py).
- *Max UE* is `ProtocolLimits.MAX_UE_NUM` in the same file. *Max RBG* is the number of RBGs of the configured bandwidth.
- Data types are C++ types for messages that cross the shared memory. E2SM-DAPP messages are Python objects relayed by the E2-E3 bridge, so their fields are Python `int` and `bool`.
- `-` means no unit or no fixed range.

| Service model | Message | Code | Direction |
|---|---|---|---|
| E2SM-KPM | [RIC REPORT](#e2sm-kpm-ric-report) | `0x11` | O-DU to Near-RT RIC |
| E2SM-RC | [RIC CONTROL](#e2sm-rc-ric-control) | `0x12` | Near-RT RIC to O-DU |
| E2SM-DAPP | [RIC REPORT](#e2sm-dapp-ric-report) | `0x21` | dApp to Near-RT RIC |
| E2SM-DAPP | [RIC CONTROL](#e2sm-dapp-ric-control) | `0x22` | Near-RT RIC to dApp |
| E3SM | [E3 INDICATION](#e3sm-e3-indication) | `0x01` | O-DU MAC and RLC to dApp |
| E3SM | [E3 CONTROL](#e3sm-e3-control) | `0x02` | dApp to O-DU MAC scheduler |

## E2 service models

### E2SM-KPM RIC REPORT

Reported by the O-DU to the Near-RT RIC. It carries radio performance indicators.

| Collective field | Unit | Range | Data type |
|---|---|---|---|
| Type | - | `0x11` | `uint16` |
| Timestamp | ns | - | `uint64` |
| Frame number | - | 0–1023 | `uint32` |
| Number of UEs | - | 0–max UE | `uint32` |
| Total buffer size | bytes | - | `uint32` |
| Number of RBGs | - | 0–max RBG | `uint16` |
| RBG size | PRB | 1–4 | `int` |
| Link direction | - | `DOWNLINK`, `UPLINK` | `uint8` |
| ID | - | - | `uint32` |

| Per-UE field | Unit | Range | Data type |
|---|---|---|---|
| RNTI | - | - | `uint16` |
| CQI | - | 0–15 | `uint8` |
| Buffer size | bytes | - | `uint32` |
| QCI | - | 1–9 | `uint8` |
| Achievable rate | Mbps | - | `double` |
| Actual throughput | Mbps | - | `double` |
| Total bytes served | bytes | - | `uint32` |
| Packet delay | ms | - | `uint32` |

### E2SM-RC RIC CONTROL

Sent by the Near-RT RIC to the O-DU to configure or override the MAC scheduler.

| Collective field | Unit | Range | Data type |
|---|---|---|---|
| Type | - | `0x12` | `uint16` |
| Max UE selection size | - | 0–max UE | `uint32` |
| Skip this step | - | `false`, `true` | `bool` |
| Link direction | - | `DOWNLINK`, `UPLINK` | `uint8` |
| ID | - | - | `uint32` |

| Per-UE field | Unit | Range | Data type |
|---|---|---|---|
| RNTI | - | - | `uint16` |
| Max RBG allocation | - | 0–max RBG | `uint16` |

### E2SM-DAPP RIC REPORT

Reported by the dApp to the Near-RT RIC. It summarizes the real-time control steps executed since the last xApp decision.

| Collective field | Unit | Range | Data type |
|---|---|---|---|
| Type | - | `0x21` | `int` |
| Number of selected UEs | - | 0–max UE | `int` |
| Total number of allocated RBGs | - | 0–max RBG | `int` |
| RBG size | PRB | 1–4 | `int` |
| Link direction | - | `DOWNLINK`, `UPLINK` | `int` |
| ID | - | - | `int` |

| Per-UE field | Unit | Range | Data type |
|---|---|---|---|
| RNTI | - | - | `int` |
| Number of allocated RBGs in the last TTI | - | 0–max RBG | `int` |
| Sum of allocated RBGs since the last xApp decision | - | - | `int` |
| Number of scheduled TTIs since the last xApp decision | - | - | `int` |

### E2SM-DAPP RIC CONTROL

Sent by the Near-RT RIC to the dApp to configure or override the upcoming real-time control steps.

| Collective field | Unit | Range | Data type |
|---|---|---|---|
| Type | - | `0x22` | `int` |
| Max UE selection size | - | 0–max UE | `int` |
| Total available RBGs | - | 0–max RBG | `int` |
| Skip this step | - | `false`, `true` | `bool` |
| Link direction | - | `DOWNLINK`, `UPLINK` | `int` |
| ID | - | - | `int` |
| Decision ID | - | - | `int` |

| Per-UE field | Unit | Range | Data type |
|---|---|---|---|
| RNTI | - | - | `int` |
| Max RBG allocation | - | 0–max RBG | `int` |

## E3 service model

### E3SM E3 INDICATION

Reported by the MAC and RLC layers of the O-DU to the local dApp. It carries radio performance indicators and the intent-related information shared by the UEs.

| Collective field | Unit | Range | Data type |
|---|---|---|---|
| Type | - | `0x01` | `uint16` |
| Timestamp | ns | - | `uint64` |
| Subframe number | - | 0–9 | `uint32` |
| Frame number | - | 0–1023 | `uint32` |
| Number of UEs | - | 0–max UE | `uint32` |
| Total buffer size | bytes | - | `uint32` |
| Initial RBG | - | 0–max RBG | `uint16` |
| Number of RBGs | - | 0–max RBG | `uint16` |
| RBG size | PRB | 1–4 | `int` |
| Link direction | - | `DOWNLINK`, `UPLINK` | `uint8` |
| ID | - | - | `uint32` |

| Per-UE field | Unit | Range | Data type |
|---|---|---|---|
| RNTI | - | - | `uint16` |
| CQI | - | 0–15 | `uint8` |
| Buffer size | bytes | - | `uint32` |
| Content criticality | - | 1–10 | `uint8` |
| Content ID | - | Object ID string | `char[MAX_ID_LEN]` |
| QCI | - | 1–9 | `uint8` |
| Achievable rate | Mbps | - | `double` |
| Actual throughput | Mbps | - | `double` |
| Total bytes served | bytes | - | `uint32` |
| Packet delay | ms | - | `uint32` |

### E3SM E3 CONTROL

Sent by the local dApp to the MAC scheduler to control it in real time.

| Collective field | Unit | Range | Data type |
|---|---|---|---|
| Type | - | `0x02` | `uint16` |
| Number of selected UEs | - | 0–max UE | `uint32` |
| Skip this step | - | `false`, `true` | `bool` |
| Link direction | - | `DOWNLINK`, `UPLINK` | `uint8` |
| ID | - | - | `uint32` |

| Per-UE field | Unit | Range | Data type |
|---|---|---|---|
| RNTI | - | - | `uint16` |
| Initial allocated RBG | - | 0–max RBG | `uint16` |
| Number of allocated RBGs | - | 0–max RBG | `uint16` |
| MCS | - | 0–28 | `uint8` |
