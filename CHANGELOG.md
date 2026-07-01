# Changelog

All notable changes to ib-ORAN are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Versioning follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## 2.0.0 (2025-07-01)

### Added
- 3GPP NR RAT support based on the 5G-LENA NR module.


## 1.0.0 (2025-06-01)

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