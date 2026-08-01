# ib-ORAN: Intent-Based Open RAN

An ns-3-based Open RAN simulation environment with native support for intent-based orchestration.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://spdx.org/licenses/MIT.html)
[![ns-3](https://img.shields.io/badge/ns--3-compatible-brightgreen)](https://www.nsnam.org)
[![RAT](https://img.shields.io/badge/RAT-LTE%20%7C%20NR-orange)](https://www.3gpp.org/)
<!-- [![ANR COMSEMA](https://img.shields.io/badge/Funded%20by-ANR%20COMSEMA-blueviolet)](https://sites.google.com/view/anrcomsema) -->


## License

The original contributions in this repository are licensed under the [MIT License](https://spdx.org/licenses/MIT.html).

> This means you are free to use, modify, and distribute this software, with or without modification, provided you retain the original copyright notice. See the [`LICENSE`](/LICENSE) file for the full terms.

### Third-Party Components

This project makes use of third-party open-source components. Their copyright notices and license terms are preserved in their respective directories and summarized in [`THIRD-PARTY.txt`](THIRD-PARTY.txt). Do not remove or modify the license and copyright files present in any third-party component directory.


## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Configuration](#configuration)
4. [Getting Started](#getting-started)
5. [Troubleshooting](#troubleshooting)
6. [Contributing](#contributing)
7. [Acknowledgements](#acknowledgements)
8. [Further Reading](#further-reading)


## Overview

ib-ORAN (module name: `ns3-iboran`) is a comprehensive simulation environment and design framework built on the proven [ns-3](https://www.nsnam.org) network simulator. It is primarily developed to implement and verify intent-based operations within Open RAN architectures.

This framework enables direct control of Physical Layer (PHY) and Medium Access Control (MAC) functions through RAN applications — rApps and xApps hosted on RAN Intelligent Controllers (RICs), as well as distributed dApps — spanning the full spectrum of control timescales from non-real-time (non-RT) to near-real-time (near-RT) to real-time.

ib-ORAN bridges high-performance ns-3 C++ simulations and Python-based policy logic via a shared-memory interface, decoupling the simulation engine from the control plane without sacrificing performance. This architecture natively supports non-RT and near-RT RIC implementations alongside real-time dApp design, enabling intent-based, semantics-aware exchange of payload data across diverse use cases.

Building on the foundations of [ns3-gym](https://github.com/tkn-tub/ns3-gym) and [ns3-ai](https://github.com/hust-diangroup/ns3-ai), ib-ORAN extends both frameworks with intent-based and semantics-aware capabilities purpose-built for Open RAN network control — going beyond reinforcement learning hooks to provide a full orchestration layer for intelligent, policy-driven RAN behavior.

ib-ORAN supports both 3GPP LTE and New Radio (NR) standards:
- **LTE**: implemented through an adapted version of the built-in `lte` module in ns-3.
- **NR**: built on top of the [ns-3 nr](https://gitlab.com/cttc-lena/nr) module.


## Architecture

<div align="center">
  <img src="./doc/architecture.png" alt="ib-ORAN Architecture" width="70%"/>
</div>

The architecture spans three control planes — non-RT RIC, near-RT RIC, and real-time dApp — interconnected via the standardized E2 and A1 interfaces, plus the newly introduced E3 interface, which is not yet standardized. The shared-memory bridge between ns-3 and the Python orchestration layer is a core design principle, enabling clean separation of simulation logic from intelligent control.

Concretely, A1 carries policy guidance from the non-RT RIC's rApps to the near-RT RIC's xApps; E2 connects the near-RT RIC to the O-DU (ns-3 RAN), carrying KPM reports, RIC control, and dApp-related messages; and E3 connects the dApp to the MAC/RLC layers, enabling sub-10ms local control at the O-DU. 

### Message Structures

See the [`message-structure.md`](/doc/message-structure.md) for the complete lists of control messages by service model exchanged over the implemented interfaces.

### App Inventory (dApp / xApp / rApp)

Browse the [`app-catalog.md`](/doc/app-catalog.md) for the full list of developed dApps, xApps, and rApps. To contribute a new app or algorithm, refer to the [`CONTRIBUTING.md`](CONTRIBUTING.md).


## Configuration

### Simulation Data

- Browse the [`simulation-data.md`](/doc/simulation-data.md) for the full details of simulator input and output data, plus the supporting datasets and embedding extraction methods.
- Default simulation configuration parameters are defined in [`iboranConfig.yaml`](/model/iboran_campaign/iboranConfig.yaml).

### Intent Production

ib-ORAN produces intents as sets of object IDs — one set per link direction — requested by, or of interest to, a network operator. The default intent production method is:

- **Pseudo-Random** — samples a fixed number of object IDs per link direction uniformly at random from `--oid_range`, seeded by `--k` for reproducibility.

> **Note:** More advanced intent production methods (specifying object IDs directly instead of sampling them) are currently under development.


## Getting Started

### Installation and Testing

A structured installation and testing guide is available in the [`install-and-test.md`](/doc/install-and-test.md).

### Performance Evaluation

Beyond single runs, the [`iboran_campaign.py`](/model/iboran_campaign/iboran_campaign.py) script facilitates large-scale evaluation by sweeping over different parameters and algorithms. This makes it easy to compare RAN operation strategies from multiple perspectives using interactive computing platforms (e.g., Jupyter Notebook).

See [`iboran_evaluation.ipynb`](/notebook/iboran_evaluation.ipynb) for an illustrative example showing how to set up comparisons across key parameters and scheduling algorithms.

### Visualization via NetAnim

ib-ORAN automatically generates an XML animation file compatible with [NetAnim](https://www.nsnam.org/wiki/NetAnim), enabling graphical visualization of nodes, packet transmissions, and mobility patterns.

#### Step 1: Clone and Build NetAnim

```bash
cd /path/to/ns-3
git clone https://gitlab.com/nsnam/netanim.git
cd netanim
mkdir build && cd build
cmake .. && cmake --build .
```

#### Step 2: Launch NetAnim

```bash
cd build
./netanim
```

To display the animation, launch the generated XML file (`iboran-lte-anim.xml` or `iboran-nr-anim.xml`) from `iboran/notebook/netanim/`.


## Troubleshooting

### Python Module Import Errors

Verify that Python dependencies are installed in the correct environment:

```bash
pip install -e contrib/ai/python_utils
pip install -e contrib/ai/model/gym-interface/py
```

### Common Issues

If you encounter further issues, please check the existing [GitHub Issues](../../issues) before opening a new one. When reporting a bug, include your ns-3 version, Python version, OS, and the full error output.


## Contributing

Contributions are welcome — new algorithms, apps, datasets, and fixes alike. Read the [`CONTRIBUTING.md`](CONTRIBUTING.md) before opening a pull request, and check the existing issues first to avoid duplicate work.


## Acknowledgements

### Funding

ib-ORAN is part of the ANR COMSEMA project, supported by the French National Research Agency.

### Citation

If you use ib-ORAN in your research, please cite this repository:

```bibtex
@software{iboran,
  author    = {Agheli, Pouya and Lefebvre, Grégoire},
  title     = {ib-ORAN: Intent-Based Open RAN},
  year      = {2026},
  url       = {https://github.com/Orange-OpenSource/ns3-iboran},
  license   = {MIT}
}
```

Or the corresponding publication available at [IEEE Xplore](https://ieeexplore.ieee.org/document/11577276) and [arXiv](https://arxiv.org/abs/2605.30079).


## Further Reading

- [ns-3 LTE Scheduler Documentation](https://www.nsnam.org/docs/models/html/lte-design.html#mac-scheduler)
- [ns-3 LTE Design Overview](https://www.nsnam.org/docs/models/html/lte-design.html)
- [5G-LENA (ns-3 NR) Documentation](https://cttc-lena.gitlab.io/nr/html/)
- [3GPP Specifications](https://www.3gpp.org/)
- [O-RAN Alliance](https://www.o-ran.org/)
- [O-RAN E2 Interface Specification](https://www.o-ran.org/specifications)
- [ANR COMSEMA Project](https://sites.google.com/view/anrcomsema)
