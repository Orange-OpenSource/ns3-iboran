# Installation

This document describes how to build ib-ORAN on top of ns-3 and verify the installation. To run simulations, see [usage](usage.md). Paths are relative to the ns-3 root.

## Prerequisites

| Dependency | Tested version |
|---|---|
| [ns-3](https://gitlab.com/nsnam/ns-3-dev) | 3.48 |
| [ns3-ai](https://github.com/hust-diangroup/ns3-ai) | `main`, commit `b8c9858` |
| [5G-LENA nr](https://gitlab.com/cttc-lena/nr) | 5.1 |
| Python | 3.11+ |
| g++ or clang++ | 11.1+ or 17.0+ |
| CMake | 3.25+ |
| Boost C++ libraries | 1.89+ |
| Protocol Buffers | 6.33.0+ |
| pybind11 | 3.0+ |
| yaml-cpp | any version found by CMake |

> [!NOTE]
> Newer releases of ns-3 and nr are expected to work, but each nr release supports one ns-3 release, so pick a pair from the [nr release timeline](https://5g-lena.cttc.es/features/roadmap/). For ns3-ai, use the `main` branch: its release tags (1.0.0 to 1.2.0) predate the message interface that ib-ORAN uses.

The `nr` module is required for both RATs, since the ib-ORAN library links against it. The Python packages are listed in [`requirements.txt`](../requirements.txt) and are installed by the setup script.

## Steps

Install ns-3 first, with ns3-ai cloned into `contrib/ai` and nr cloned into `contrib/nr`. Follow the installation guides of [ns-3](https://www.nsnam.org/docs/installation/html/), [ns3-ai](https://github.com/hust-diangroup/ns3-ai/blob/main/docs/install.md) and [nr](https://gitlab.com/cttc-lena/nr), or clone the tested versions directly:

```shell
git clone --branch ns-3.48 --depth 1 https://gitlab.com/nsnam/ns-3-dev.git ns-3
cd ns-3
git clone https://github.com/hust-diangroup/ns3-ai.git contrib/ai
git clone --branch v5.1 --depth 1 https://gitlab.com/cttc-lena/nr.git contrib/nr
```

The ns3-ai examples do not build with ns-3.48, and ib-ORAN does not need them, so remove them:

```shell
rm -rf contrib/ai/examples
```

Then follow the steps below.

### Step 1: Clone ib-ORAN

Clone ib-ORAN into `contrib/iboran`. Contributors should use SSH:

```shell
git clone git@github.com:Orange-OpenSource/ns3-iboran.git contrib/iboran
```

Otherwise, use HTTPS:

```shell
git clone https://github.com/Orange-OpenSource/ns3-iboran.git contrib/iboran
```

### Step 2: Set up the Python environment

Create a virtual environment in the ns-3 root and install the ns3-ai Python interfaces. Skip this step if you did it while installing ns3-ai.

```shell
python3 -m venv .venv
source .venv/bin/activate
pip install -e contrib/ai/python_utils
pip install -e contrib/ai/model/gym-interface/py
```

The setup script in step 4 expects the virtual environment at `.venv` in the ns-3 root.

### Step 3: Configure and build ns-3

Reconfigure and rebuild ns-3 to register the new module:

```shell
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

With examples enabled, this also builds the two ib-ORAN simulation programs, `ns3iboran_lte` and `ns3iboran_nr`. To rebuild only one of them later:

```shell
./ns3 build ns3iboran_lte
./ns3 build ns3iboran_nr
```

### Step 4: Run the setup script

The `quick-setup.sh` script checks the prerequisites and source files, installs the Python dependencies into the virtual environment, rebuilds the module and registers `contrib/iboran` on the Python path of the virtual environment. Run it from `contrib/iboran`:

```shell
cd contrib/iboran
bash quick-setup.sh
```

To use ib-ORAN from a Python interpreter outside the virtual environment, such as a separate Jupyter kernel, add the module to `PYTHONPATH` in `~/.bashrc` or `~/.zshrc`:

```shell
export PYTHONPATH="/path/to/ns-3/contrib/iboran:$PYTHONPATH"
```

## Troubleshooting

### pip reports "not a valid editable requirement"

pip did not find a Python project at `contrib/ai/python_utils` or `contrib/ai/model/gym-interface/py`. Either the commands ran outside the ns-3 root, or ns3-ai was cloned at a release tag, which lacks these folders. Run the commands from the ns-3 root, with ns3-ai on its `main` branch.

### ns3-ai examples fail to build

With ns-3.48, the ns3-ai `rl-tcp` examples fail with "declaration of 'GetInstanceTypeId' overrides a 'final' function". ib-ORAN does not use them. Delete the `contrib/ai/examples` folder, then reconfigure and rebuild ns-3.

### Python cannot import ns3-ai or ib-ORAN

Activate the virtual environment used during installation, install the ns3-ai interfaces in it, then rebuild:

```shell
source .venv/bin/activate
pip install -e contrib/ai/python_utils
pip install -e contrib/ai/model/gym-interface/py
./ns3 configure --enable-examples --enable-tests
./ns3 build
```
