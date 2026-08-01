# Changelog

All notable changes to ib-ORAN are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Versioning follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

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