# Contributing to ib-ORAN

ib-ORAN is an open research project, and contributions are welcome: new apps, scheduling algorithms, bug fixes and documentation. This document describes how to contribute. Everyone taking part in the project follows the [code of conduct](.github/CODE_OF_CONDUCT.md). Paths are relative to `contrib/iboran`.

## Questions and bug reports

Ask questions in [Discussions](https://github.com/Orange-OpenSource/ns3-iboran/discussions), not in Issues. Report bugs and request features in [Issues](https://github.com/Orange-OpenSource/ns3-iboran/issues). Check existing issues before opening a new one.

A bug report should include:

- the ib-ORAN commit or release tag
- the ns-3, ns3-ai, nr and Python versions
- the OS and compiler version
- the command that failed, with its parameters
- the expected and the actual behavior
- the full error output or logs

## Pull requests

1. Fork the repository and clone your fork into `contrib/iboran` of an ns-3 tree (see [installation](doc/installation.md)).
2. Create a branch from `main`, named after the type of change:

   | Prefix | Change |
   |---|---|
   | `feature/` | New functionality |
   | `fix/` | Bug fix |
   | `docs/` | Documentation only |

3. Make your changes, following the [code style](#code-style) and the [file headers](#file-headers).
4. [Test your changes](#testing-your-changes).
5. Update the [documentation](#documentation) that your change affects.
6. Open a pull request against `main`. State what changed and why, and link the related issue.

Open an issue first for large or structural changes, so the approach can be discussed before you invest time in it.

Write commit messages in the imperative mood, with a subject line of about 50 characters, for example `Add Clamp xApp RBG bounding algorithm`.

## Testing your changes

ib-ORAN has no automated test suite yet. Before opening a pull request, run the setup script and one simulation per RAT from a clean build.

From `contrib/iboran`:

```shell
bash quick-setup.sh
```

From `model/iboran_campaign`:

```shell
python3 iboran_run.py --rat_type="LTE" --rng_run=1
python3 iboran_run.py --rat_type="NR" --rng_run=1
```

If your change touches an app or an algorithm, also run it with its own parameters, for example `--dapp_algo_ue_selection` or `--xapp_algo_rbg_bounding`, in both its standard and `ib` variants. If it touches the RAN model, delete the cached results first (see [cached results](doc/simulation-data.md#cached-results)).

## Code style

| Language | Rules | Check |
|---|---|---|
| C++ | [ns-3 coding style](https://www.nsnam.org/docs/contributing/html/coding-style.html) | From the ns-3 root: `python3 utils/check-style-clang-format.py contrib/iboran` |
| Python | [Black](https://github.com/psf/black) formatting and type hints where applicable | From `contrib/iboran`: `black .` and `flake8` |

Keep functions short and named after what they do, comment logic that is not obvious, and discuss any new dependency in an issue before adding it.

## File headers

Every new source file starts with a license header. Original contributions are MIT-licensed. A C++ file that derives from ns-3 or ns3-ai code (for example, a class that inherits from an ns-3 scheduler) keeps the GPL-2.0-only license of its origin and must be listed in [THIRD-PARTY.txt](THIRD-PARTY.txt).

Python:

```python
# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md
```

C++:

```cpp
/*
 * Software Name: ib-ORAN
 * SPDX-FileCopyrightText: Copyright (c) Orange SA
 * SPDX-License-Identifier: MIT
 *
 * This software is distributed under the MIT license,
 * see the "LICENSE" file for more details
 * or https://spdx.org/licenses/MIT.html
 *
 * Authors: see CONTRIBUTORS.md
 */
```

C++ derived from GPL code:

```cpp
/*
 * Software Name: ib-ORAN
 * SPDX-FileCopyrightText: Copyright (c) Orange SA
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This software is derived from ns-3 and ns3-ai, which are distributed
 * under the GNU General Public License v2.0.
 * See https://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * Authors: see CONTRIBUTORS.md
 */
```

## Adding an app

dApps, xApps and rApps are Python classes in [`model/iboran_controller/`](model/iboran_controller). They derive from `IbOranController` in [`iboran_controller.py`](model/iboran_controller/iboran_controller.py), which implements the control loop, the skip logic and the statistics. `IbOranDApp` in [`iboran_dapp.py`](model/iboran_controller/iboran_dapp.py) and `IbOranXApp` in [`iboran_xapp.py`](model/iboran_controller/iboran_xapp.py) specialize it for each timescale, and the MAC schedulers in `iboran_dapp_mac_scheduler.py` and `iboran_xapp_mac_scheduler.py` are complete examples.

A new app must:

- derive from the base class of its timescale (see the [apps](doc/apps.md) table for the timescales)
- use the service models of its location (see [service models](doc/service-models.md))
- expose its settings as parameters of `iboran_run.py` and keys of `iboranConfig.yaml`
- be listed in [apps](doc/apps.md), with its ID, description, interfaces and parameters

### Changing the C++/Python interface

If the app needs new message fields or a new message type, change both the C++ and the Python files in [`model/iboran_interfaces/`](model/iboran_interfaces). Then regenerate the Python bindings with a clean rebuild from the ns-3 root:

```shell
./ns3 clean
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

Check that the new fields reach Python before writing the app logic, and update [service models](doc/service-models.md).

## Adding an algorithm

dApp UE selection algorithms are in [`model/iboran_controller/iboran_dapp_algos/`](model/iboran_controller/iboran_dapp_algos) and xApp RBG bounding algorithms are in [`model/iboran_controller/iboran_xapp_algos/`](model/iboran_controller/iboran_xapp_algos). To add one:

1. Derive a class from `DAppAlgoUeSelection` or `XAppAlgoRbgBounding` and implement `process()`.
2. Register it in `dapp_algos_factory.py` or `xapp_algos_factory.py`, as a standalone algorithm or as a family with metrics.
3. List it in [algorithms](doc/algorithms.md), with its standard and intent-based names.

The intent-based variant needs no extra code: the `ib` prefix is resolved to the registered base algorithm, which then receives the link intents.

## Documentation

Update the documentation in the same pull request as the change:

| Change | File to update |
|---|---|
| New or changed run parameter | [usage](doc/usage.md#parameters) and [`iboranConfig.yaml`](model/iboran_campaign/iboranConfig.yaml) |
| New app | [apps](doc/apps.md) |
| New algorithm | [algorithms](doc/algorithms.md) |
| New or changed message field | [service models](doc/service-models.md) |
| New KPI | [KPIs](doc/kpis.md) |
| New input, output or dataset | [simulation data](doc/simulation-data.md) |
| New dependency or install step | [installation](doc/installation.md) and the [README](README.md#installation) |
| Any user-facing change | [CHANGELOG.md](CHANGELOG.md), in the [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) format |

The files in `doc/` follow these conventions:

- Headings are in sentence case. Each file opens with one H1 title and one paragraph that states its scope and, if the file gives paths, the folder they are relative to.
- Links between files are relative, such as `../model/...` from `doc/`. Folders that exist only after a run are written as code, not linked.
- Identifiers, values and paths are in code format, without quotes. Quotes appear only in shell commands.
- Reference content is in tables with sentence-case column names.
- Notes and warnings use GitHub alerts (`> [!NOTE]`, `> [!WARNING]`).
- Shell blocks use the `shell` language tag.

## Credits

Contributors who agree to be named are listed in [CONTRIBUTORS.md](CONTRIBUTORS.md).
