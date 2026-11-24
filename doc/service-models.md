# Service models

This document lists the fields of the messages exchanged over the E2 and E3 interfaces, by service model and message type. Each message has collective fields, carried once per message, and per-UE fields, repeated for each UE in the message.

Conventions used in the tables:

- *Type* is the message type code of `MessageType` in [`iboran_shm_protocol.py`](../model/iboran_interfaces/iboran_shm_protocol.py).
- *Max UE* is `ProtocolLimits.MAX_UE_NUM` in the same file. *Max RBG* is the number of RBGs of the configured bandwidth.
- Data types are C++ types for messages that cross the shared memory. E2SM-DAPP messages are Python objects relayed by the E2-E3 bridge, so their fields are Python `int` and `bool`.
- `-` means no unit or no fixed range.

> [!NOTE]
> E3SM content is not standardized: nGRG RR-2025-05 leaves it to each dApp. For quantities also reported over E2SM-KPM, ib-ORAN uses the same names, units and definitions on E3.

| Service model | Message | Code | Direction |
|---|---|---|---|
| E2SM-KPM | [RIC INDICATION](#e2sm-kpm-ric-indication) | `0x11` | O-DU to Near-RT RIC |
| E2SM-RC | [RIC CONTROL REQUEST](#e2sm-rc-ric-control-request) | `0x12` | Near-RT RIC to O-DU |
| E2SM-DAPP | [INDICATION](#e2sm-dapp-indication) | `0x21` | dApp to Near-RT RIC |
| E2SM-DAPP | [CONTROL REQUEST](#e2sm-dapp-control-request) | `0x22` | Near-RT RIC to dApp |
| E3SM | [INDICATION](#e3sm-indication) | `0x01` | O-DU MAC and RLC to dApp |
| E3SM | [CONTROL REQUEST](#e3sm-control-request) | `0x02` | dApp to O-DU RAN function |

## E2 service models

### E2SM-KPM RIC INDICATION

Reported by the O-DU to the Near-RT RIC (E2SM-KPM Report Style 5, per UE). Measurement names follow 3GPP TS 28.552; fields marked *extension* have no TS 28.552 counterpart.

| Collective field | Unit | Range | Data type |
|---|---|---|---|
| Type | - | `0x11` | `uint16` |
| RIC requestor ID | - | - | `uint16` |
| RIC instance ID | - | - | `uint16` |
| RAN function ID | - | `1` (KPM) | `uint16` |
| Indication SN | - | - | `uint32` |
| Cell ID | - | - | `uint16` |
| Present | - | `0`, `1` | `uint8` |
| Collection start time | ns | - | `uint64` |
| Granularity period | ms | multiple of 10 | `uint32` |
| Number of UEs | - | 0–max UE | `uint32` |
| Number of RBGs | - | 0–max RBG | `uint16` |
| RBG size | PRB | 1–4 (LTE), 1–16 (NR) | `int` |
| Link direction | - | `DOWNLINK`, `UPLINK` | `uint8` |

| Per-UE field | Unit | Range | Data type |
|---|---|---|---|
| RNTI | - | - | `uint16` |
| UE ID | - | IMSI, 0 if not known yet | `uint64` |
| QoS ID (measurement label) | - | QCI 1–9 (LTE), 5QI 1–255 (NR) | `uint8` |
| Actual throughput | kbps | - | `double` |
| Data volume served in the period | kbit | - | `uint32` |
| CQI (extension) | - | 0–15 | `uint8` |
| Buffer size (extension) | bytes | - | `uint32` |
| Content ID (extension) | - | Object ID string | `char[MAX_ID_LEN]` |
| Head-of-line delay (extension) | ms | - | `uint32` |

The RAN sends one indication per reporting period subscribed at setup. In the other TTIs or slots, Present is 0 and the per-UE fields are empty. Actual throughput is `DRB.UEThpDl/Ul` of TS 28.552, averaged over the last 100 ms. Data volume served is `DRB.RlcSduTransmittedVolumeDL/UL`: the bytes scheduled for the UE since its previous report, over all layers, without padding. The collection period runs from `collectionStartTime` to the indication, and `granularityPeriod` gives its length. It can exceed the subscribed period when no UE had data at the end of a period.

> [!NOTE]
> Data volume is counted when the RAN schedules the data, not when it is received. On the uplink it can therefore exceed the volume received. A UE is reported only when it has data waiting, so data sent after its last report is not counted.

### E2SM-RC RIC CONTROL REQUEST

Sent by the Near-RT RIC to the O-DU to configure or override the RAN function.

| Collective field | Unit | Range | Data type |
|---|---|---|---|
| Type | - | `0x12` | `uint16` |
| RIC requestor ID | - | - | `uint16` |
| RIC instance ID | - | - | `uint16` |
| RAN function ID | - | `2` (RC) | `uint16` |
| Request SN | - | - | `uint32` |
| Max UE selection size | - | 0–max UE | `uint32` |
| Skip this step | - | `false`, `true` | `bool` |
| Link direction | - | `DOWNLINK`, `UPLINK` | `uint8` |

| Per-UE field | Unit | Range | Data type |
|---|---|---|---|
| UE ID | - | IMSI, 0: inert entry | `uint64` |
| Max RBG allocation | - | 0–max RBG | `uint16` |

### E2SM-DAPP INDICATION

Reported by the dApp to the Near-RT RIC. It summarizes the real-time control steps executed since the last xApp decision.

| Collective field | Unit | Range | Data type |
|---|---|---|---|
| Type | - | `0x21` | `int` |
| dApp ID | - | - | `int` |
| RAN function ID | - | `3` (DAPP) | `int` |
| Indication SN | - | - | `int` |
| Number of selected UEs | - | 0–max UE | `int` |
| Total number of allocated RBGs | - | 0–max RBG | `int` |
| RBG size | PRB | 1–4 (LTE), 1–16 (NR) | `int` |
| Link direction | - | `DOWNLINK`, `UPLINK` | `int` |

| Per-UE field | Unit | Range | Data type |
|---|---|---|---|
| RNTI | - | - | `int` |
| UE ID | - | IMSI, 0 if not known yet | `int` |
| Number of allocated RBGs in the last TTI or slot | - | 0–max RBG | `int` |
| Sum of allocated RBGs since the last xApp decision | - | - | `int` |
| Number of scheduled TTIs or slots since the last xApp decision | - | - | `int` |

### E2SM-DAPP CONTROL REQUEST

Sent by the Near-RT RIC to the dApp to configure or override the upcoming real-time control steps.

| Collective field | Unit | Range | Data type |
|---|---|---|---|
| Type | - | `0x22` | `int` |
| RIC requestor ID | - | - | `int` |
| RIC instance ID | - | - | `int` |
| RAN function ID | - | `3` (DAPP) | `int` |
| Request SN | - | - | `int` |
| Max UE selection size | - | 0–max UE | `int` |
| Total available RBGs | - | 0–max RBG | `int` |
| Skip this step | - | `false`, `true` | `bool` |
| Link direction | - | `DOWNLINK`, `UPLINK` | `int` |
| Decision ID | - | - | `int` |

| Per-UE field | Unit | Range | Data type |
|---|---|---|---|
| RNTI | - | last known, 0 if none | `int` |
| UE ID | - | IMSI | `int` |
| Max RBG allocation | - | 0–max RBG | `int` |

## E3 service model

### E3SM INDICATION

Reported by the MAC and RLC layers of the O-DU to the local dApp. It carries radio performance indicators and the intent-related information shared by the UEs.

| Collective field | Unit | Range | Data type |
|---|---|---|---|
| Type | - | `0x01` | `uint16` |
| dApp ID | - | - | `uint16` |
| Indication SN | - | - | `uint32` |
| Cell ID | - | - | `uint16` |
| Timestamp | ns | - | `uint64` |
| Subframe number | - | 0–9 | `uint32` |
| Frame number | - | 0–1023 | `uint32` |
| Number of UEs | - | 0–max UE | `uint32` |
| Total buffer size | bytes | - | `uint32` |
| Initial RBG | - | 0–max RBG | `uint16` |
| Number of RBGs | - | 0–max RBG | `uint16` |
| RBG size | PRB | 1–4 (LTE), 1–16 (NR) | `int` |
| Link direction | - | `DOWNLINK`, `UPLINK` | `uint8` |

| Per-UE field | Unit | Range | Data type |
|---|---|---|---|
| RNTI | - | - | `uint16` |
| UE ID | - | IMSI, 0 if not known yet | `uint64` |
| QoS ID | - | QCI 1–9 (LTE), 5QI 1–255 (NR) | `uint8` |
| Actual throughput | kbps | - | `double` |
| CQI | - | 0–15 | `uint8` |
| Buffer size | bytes | - | `uint32` |
| Content ID | - | Object ID string | `char[MAX_ID_LEN]` |
| Head-of-line delay | ms | - | `uint32` |
| Content criticality | - | 1–10 | `uint8` |
| Achievable rate | kbps | - | `double` |

The first eight fields have the same names, units and definitions as in the [E2SM-KPM RIC INDICATION](#e2sm-kpm-ric-indication). Content criticality and content ID are published by the application of each UE (see [application layer](application.md#criticality)).

### E3SM CONTROL REQUEST

Sent by the local dApp to the RAN function to control it in real time.

| Collective field | Unit | Range | Data type |
|---|---|---|---|
| Type | - | `0x02` | `uint16` |
| dApp ID | - | - | `uint16` |
| Request SN | - | - | `uint32` |
| Number of selected UEs | - | 0–max UE | `uint32` |
| Skip this step | - | `false`, `true` | `bool` |
| Link direction | - | `DOWNLINK`, `UPLINK` | `uint8` |

| Per-UE field | Unit | Range | Data type |
|---|---|---|---|
| RNTI | - | - | `uint16` |
| Initial allocated RBG | - | 0–max RBG | `uint16` |
| Number of allocated RBGs | - | 0–max RBG | `uint16` |
| MCS | - | 0–28 | `uint8` |

## Setup and control outcome

Before its first indication, the RAN function sends a setup request, which stands for E2 SETUP and E3 SETUP. The apps answer with the subscription, which stands for RIC SUBSCRIPTION and E3 SETUP RESPONSE.

A phase field in each shared-memory message tells setup (`0`) from run (`1`).

| Setup request field | Unit | Range | Data type |
|---|---|---|---|
| Cell ID | - | - | `uint16` |
| RAN function mask | - | bit `ID - 1` per RAN function | `uint32` |

| Setup response field | Unit | Range | Data type |
|---|---|---|---|
| RIC requestor ID | - | - | `uint16` |
| RIC instance ID | - | - | `uint16` |
| KPM reporting period | ms | multiple of 10 | `uint32` |
| dApp ID | - | - | `uint16` |

Every indication carries the outcome of the previous control requests of its link, which stands for RIC CONTROL ACKNOWLEDGE and RIC CONTROL FAILURE.

| Control outcome field | Unit | Range | Data type |
|---|---|---|---|
| E3 request SN | - | - | `uint32` |
| E3 cause | - | `ControlCause` | `uint8` |
| E3 rejected UE entries | - | 0–max UE | `uint16` |
| E2SM-RC request SN | - | - | `uint32` |
| E2SM-RC cause | - | `ControlCause` | `uint8` |

Cause `NONE` acknowledges the request. Otherwise the cause is the last failure of the request. Causes are listed in `ControlCause` in [`iboran_shm_protocol.py`](../model/iboran_interfaces/iboran_shm_protocol.py).
