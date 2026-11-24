# Changelog

All notable changes to ib-ORAN are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Versioning follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## 4.0.0 (2026-11-24)

### Added
- E2 and E3 setup exchange: each MAC scheduler announces its cell and RAN functions, and the apps reply with the KPM reporting period, the RIC request ID and the dApp ID.
- Control outcome of every E3SM and E2SM-RC CONTROL REQUEST, with a failure cause, counted in the app stats.
- The RAN rejects an E2SM-RC CONTROL REQUEST whose RAN function ID is not RC, with cause `INVALID_RAN_FUNCTION`.
- Dedicated NR QoS flow carrying the 5QI of the data format.

### Changed
- Breaking: messages, structs and algorithm arguments renamed to O-RAN terms (E2AP RIC INDICATION and RIC CONTROL REQUEST, E3 indication and control request, E2SM-DAPP indication and control request). Custom algorithms must rename the `process()` arguments.
- Breaking: messages carry the RIC request ID, the RAN function ID and sequence numbers instead of `id`, plus the cell ID. Per-UE entries carry the UE ID (IMSI), and E2SM-RC bounds are keyed by UE ID.
- Breaking: `qci` is renamed `qosId`. It holds the QCI on LTE and the 5QI on NR.
- The RAN sends E2SM-KPM indications at the subscribed period and marks the other TTIs and slots as not present.
- Breaking: E2SM-KPM indications carry TS 28.552 measurements: `DRB.UEThpDl/Ul` (kbps) replaces `actualThroughput`, and `DRB.RlcSduTransmittedVolumeDL/UL` (kbit per period) replaces `totalBytesServed`. `collectionStartTime` and `granularityPeriod` replace `timestamp`. `frameNo`, `totalBufferLevel` and `achievableRate` leave KPM. CQI, buffer size, content ID and head-of-line delay are documented as ib-ORAN extensions.
- Breaking: E3 indications use the KPM names, units and field order for shared per-UE quantities (`drbUeThp` in kbps replaces `actualThroughput`); `totalBytesServed` leaves E3.
- Breaking: E3SM `achievableRate` is in kbps instead of Mbps, as `drbUeThp`.
- Breaking: `bufferLevel` and `totalBufferLevel` are renamed `bufferSize` and `totalBufferSize`, after the Buffer Size field of TS 38.321, and the dApp algorithms `TopKBufferLevel`, `GreedyBufferLevel`, `DpBufferLevel` and their `ib` variants are renamed `TopKBufferSize`, `GreedyBufferSize` and `DpBufferSize`. Cached results made under the old names no longer match.

### Removed
- The developer reference `model/iboran_application/README.md`.

### Fixed
- The reported QoS ID leaves out SRBs, which made every LTE UE report QCI 1, and follows the 3GPP priority levels.
- The dApp dropped a UE whose demand exceeded the available RBGs when no xApp bound capped it, which could leave a TTI or slot unscheduled. The grant is now capped at the available RBGs.
- Round Robin gave the lowest priority to a UE never scheduled, which then waited until the other UEs had emptied their buffers, and compared a time that wraps every 10.24 s with absolute times. A UE never scheduled now comes first.
- The NR downlink never served a UE with fewer than 16 bytes waiting, which kept the UE active until the end of the run, used up its scheduling turns and lost the last fragment of its message.
- The image quality KPI downloaded the CLIP model to `contrib/notebook/models/emb/CLIP`, outside ib-ORAN. Both embedding models are now stored in `notebook/models/emb/` of ib-ORAN, whoever loads them.

### Known issues
- Only one cell is supported. With `xnbNum` above 1, the dApp and the xApp keep one state per link for all cells.

## 3.0.0 (2026-11-18)

### Added
- Application-layer message framing: a 44-byte fragment header on every packet, reassembly at the receiver per sender and message ID, and fill policies for lost elements (`appFraming`, `appFillPolicy`).
- Content type detection for PNG, JPEG, BMP, H.264 Annex B, MP4, float32, float16 and 1-byte embedding codes, text and JSON, with JPEG and H.264 criticality maps.
- `SendMessage()` on the transmission application, and `TxMessage` and `RxMessage` trace sources.
- Saving of every received message (`appSaveAllMessages`) and a per-message transmission and reception log in `notebook/logs/appMessages.json` (`appSaveMessageLog`), with the payload ranges lost in each message.
- `doc/application.md`, and a developer reference in `model/iboran_application/README.md`.
- xApp control log in `notebook/logs/xapp_logs/` (`--save_xapp_log`).

### Changed
- Breaking: link traffic keys in `common` renamed with the `app` prefix: `enableDlFlow` to `appDlEnable`, `dlPacketSize` to `appDlPacketSize`, `dlBitRate` to `appDlBitRate`, and `dlUseUdp` to `appDlTransport`, which takes `"UDP"` or `"TCP"` instead of a boolean; the same for uplink. Configuration files with the old keys load without error, but the keys are ignored and the defaults apply.
- Breaking: `PacketSize` and `BitRateBps` include the fragment header; with `appFraming: true` a 512-byte packet carries 468 payload bytes. Results differ from earlier versions; `appFraming: false` restores the previous transfer.
- `MinPacketInterval` attribute is a `Time` instead of an integer in milliseconds, so the interval computed from packet size and bit rate is no longer truncated to 0.
- UDP packets are capped at `MaxDatagramSize` (1472 bytes) to avoid IP fragmentation.
- TCP minimum retransmission timeout set to 200 ms, as in Linux, instead of the ns-3 default of 1 s; a lost segment that fast retransmit cannot recover now delays its message by about 200 ms instead of 1 s.
- Default dataset in `iboranConfig.yaml` changed from Market-1501 to `toy`, with `CLIP` embeddings, so a fresh clone runs without downloading a dataset.
- Default link bit rates set to 2 Mbps downlink and 1 Mbps uplink.
- Default NR bandwidth set to 10 MHz, so UEs compete for resources.

### Removed
- `dlTxRepeat` and `ulTxRepeat`: the transmission application sends the loaded file once. Configuration files that still set them load without error; the keys are ignored.

### Fixed
- Lost UDP packets shifted every later byte of the payload, so `FidelityScore` compared misaligned embedding dimensions.
- JPEG inputs were parsed with the PNG chunk parser.
- The content ID was not republished when the RNTI changed after a handover.
- `IMAGE` outputs were saved as `.png` while the KPI step looked for the dataset's `input_image_format`, so `IMAGE` runs on Market-1501 were not scored.
- `PrintApplicationStats` matched applications to the wrong UE and link direction when both links were enabled, and read past the application container with the uplink only. It now numbers UEs from 1, as the other outputs do.
- Standard KPIs counted core-network signaling and TCP acknowledgment flows; they now count only the data flows of each link.
- Over TCP, throughput ran from the SYN at application start to the FIN at application stop; with `appSaveMessageLog: true` it now runs from the link's first message sent to its last message received, and without the log the orchestrator prints a warning.
- `transmissionMode` is the ns-3 index counted from 0, so the default `2` is TM3 (open-loop spatial multiplexing); the configuration comments gave 3GPP numbers, and values outside 0 to 6 are now rejected.

### Known issues
- With NR, uplink results may differ between two runs with the same parameters and `--rng_run`. The variation also occurs with the stock 5G-LENA scheduler and is under investigation.

## 2.2.1 (2026-11-07)

### Added
- Code of conduct (`.github/CODE_OF_CONDUCT.md`, Contributor Covenant 2.0) and security policy (`.github/SECURITY.md`).
- `doc/usage.md`, `doc/kpis.md` and `doc/visualization.md`.

### Changed
- README restructured into architecture, installation, usage, documentation, contributing and citation sections.
- Documentation reorganized: `install-and-test.md` split into `installation.md` and `usage.md`, `app-catalog.md` renamed to `apps.md`, `built-in-algos.md` to `algorithms.md` and `message-structure.md` to `service-models.md`, and KPI definitions moved from `simulation-data.md` to `kpis.md`.
- `CONTRIBUTING.md` rewritten with testing steps, file headers, guides for adding apps and algorithms, and documentation conventions.

### Fixed
- Broken links and anchors in the README, `CONTRIBUTING.md` and `doc/`.
- Documentation errors: `nr` listed as required for both RATs, `yaml-cpp` added to the prerequisites, and the per-UE RBG bound of E2SM-RC and E2SM-DAPP given in RBGs instead of PRBs.
- `quick-setup.sh`: CMake check raised to 3.25+ as required by ns-3.48, Protocol Buffers version check corrected, ns3-ai `python_utils` path fixed, and shebang moved to the first line.

## 2.2.0 (2026-10-29)

### Added
- `Clamp` xApp RBG bounding algorithm (standard and `ib` variants), bounding each UE's RBG grant to its current demand.
- Reproducible replications via `--rng_run` (`common.rngRun`), seeding ns-3 and every other random generator; runs with the same value share the same channel and traffic realization.
- Per-UE content in E2SM-DAPP REPORT and CONTROL messages, enabling per-UE RBG bounds and grant feedback between xApp and dApp.
- `XAppDecisionTime` KPI, KPI definitions in `doc/simulation-data.md`, and campaign caching of per-run KPI records.

### Changed
- xApp RBG bounds persist until replaced, and the dApp serves a UE up to its bound instead of dropping it.
- Throughput and packet delay in KPM/E3 reports are now measured by the MAC schedulers instead of estimated.
- Standard KPIs (PDR, throughput, latency, jitter) are aggregated per link; `PRBUtilizationRatio`, `DAppDecisionTime` and `SatisfactionScore` computations corrected.
- New defaults in `iboranConfig.yaml`: NR, Market-1501 dataset, EMBEDDING data format.
- Algorithm and app interfaces now receive the E2SM-DAPP messages; custom algorithms must update their `process()` signatures.
- New dependency: `yacs`.

### Removed
- `Numerology` attribute of `IbOranNrMacScheduler`; the slot duration is taken from the PHY configuration.

### Fixed
- Off-by-one PRB demand in the LTE TBS lookup.
- Non-reproducible UE object-ID assignment in the ns-3 environment.
- Campaign runs recording KPIs of a previous run after a failure.

## 2.1.0 (2026-08-01)

### Added
- A standalone KPI definition module, `iboran_kpi`, providing shared foundations for KPI measurement across data formats.
- A unified simulation parameter configuration module, `iboranConfig.yaml`, consolidating LTE/NR RAN parameters and simulation variables (e.g. `k`, the maximum parallel UEs scheduled per TTI/slot). Individual parameters can still be overridden via CLI flags when running `iboran_run.py`.
- Configurable per-dataset filename patterns (`datasets:` section in `iboranConfig.yaml`), enabling datasets with irregular naming — e.g. Market-1501 — instead of the previous fixed `input_dl_<id>.png` scheme.
- Implemented end-to-end "EMBEDDING" data format support: CLIP and TransReID extraction backends.

### Changed
- Refactored dApp/xApp control-loop logic into a shared `IbOranController` base class; statistics collection is now centralized rather than duplicated per app.
- Replaced wildcard imports and scattered magic constants with explicit imports and centralized enums (`LinkDirection`, `RatType`, `MessageType`, `ProtocolLimits`).
- E2SM-DAPP CONTROL verification failures now raise `ValueError` instead of printing a warning and continuing.
- New dependencies: `pydantic`, `ruamel.yaml`, `gdown`.

### Removed
- `DataFormat::CUSTOM` is no longer a supported data format.

## 2.0.0 (2026-07-01)

### Added
- 3GPP NR RAT support based on the 5G-LENA NR module.

## 1.0.0 (2026-06-01)

### Added
- Initial public release of ib-ORAN.
- 3GPP LTE RAT support.
- Shared-memory C++/Python bridge between the RAN and control layer.
- Python-based intent orchestration layer for xApp and dApp execution.
- MAC scheduling algorithms as xApp and dApp plugins.
- E3 interface for real-time, closed-loop dApp control.
- E2 KPM and RC service models for xApp control.
- Campaign script for automated large-scale parameter sweep evaluations.
- Jupyter notebook for interactive post-campaign performance analysis.
- NetAnim XML output for simulation animation and visualization.

### License
- Original Orange contributions released under MIT.
  Third-party derived C++ modules remain under GPLv2.
  See `LICENSE` and `THIRD-PARTY.txt` for full details.

---

<!-- Add future releases above this line in the format:

## X.Y.Z (YYYY-MM-DD)

### Added
### Changed
### Deprecated
### Removed
### Fixed
### Security
### Known issues
### License  ← use this section if the license changes

-->
