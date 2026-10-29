# Changelog

All notable changes to ib-ORAN are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Versioning follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

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

## [X.Y.Z] — YYYY-MM-DD

### Added
### Changed
### Deprecated
### Removed
### Fixed
### Security
### License  ← use this section if the license changes

-->