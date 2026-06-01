# Installation Guide

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Installation](#installation)
   - [Step 1: Clone the Repository](#step-1-clone-the-repository)
   - [Step 2: Re-Configure and Re-Build ns-3](#step-2-re-configure-and-re-build-ns-3)
   - [Step 3: Build the ib-ORAN Module](#step-3-build-the-ib-oran-module)
   - [Step 4: Set Python Path](#step-4-set-python-path)
   - [Step 5: Run the Quick Setup Script](#step-5-run-the-quick-setup-script)
3. [Python Dependencies Note](#python-dependencies-note)


## Prerequisites

**Required Software:**

| Dependency | Version |
|---|---|
| [ns-3](https://gitlab.com/nsnam/ns-3-dev) | 3.46.1 |
| [ns3-ai](https://github.com/hust-diangroup/ns3-ai/blob/main/docs/install.md) | 1.2.0 |
| CMake | 3.10+ |
| Boost C++ libraries | 1.89+ |
| Protocol Buffers | 6.33.0+ |
| pybind11 | 3.0+ |
| Python | 3.11+ |
| g++ or clang++ | 11.1+ / 17.0+ |

> **Tip:** It is strongly recommended to use a Python virtual environment (see [Python Dependencies Note](#python-dependencies-note) below).

> **Tip:** It is strongly recommended to use a stable release of ns-3. Avoid unstable versions such as 3.47.


## Installation

Ensure you have successfully installed [ns-3](https://gitlab.com/nsnam/ns-3-dev) along with its required components:
- [ns3-ai](https://github.com/hust-diangroup/ns3-ai/blob/main/docs/install.md) — cloned into `contrib/ai`

Then follow the steps below.

> **Tip:** If you encounter configuration errors related to inconsistent function definitions while installing ns3-ai, try deleting the `/examples` folder and rebuilding.


### Step 1: Clone the Repository

Clone ib-ORAN into the `contrib/iboran` directory inside your ns-3 root.

Via SSH *(recommended for contributors)*:
```bash
git clone git@ghttps://github.com/Orange-OpenSource/ns3-iboran.git contrib/iboran
```

Via HTTPS:
```bash
git clone https://github.com/Orange-OpenSource/ns3-iboran.git contrib/iboran
```


### Step 2: Re-Configure and Re-Build ns-3

From the ns-3 root directory, re-run configure and build to register the new contrib module:

```bash
cd /path/to/ns-3
./ns3 configure --enable-examples --enable-tests
./ns3 build
```


### Step 3: Build the ib-ORAN Module

```bash
# LTE module
./ns3 build ns3iboran_lte
```


### Step 4: Set Python Path

To allow Python to resolve ib-ORAN's modules correctly — whether running scripts directly or from a Jupyter notebook — add the following to your `~/.bashrc` or `~/.zshrc`:

```bash
export PYTHONPATH="$(realpath /path/to/ns-3/contrib/iboran):$PYTHONPATH"
```

Then reload your shell:

```bash
source ~/.bashrc   # or ~/.zshrc
```


### Step 5: Run the Quick Setup Script

The `quick-setup.sh` script automates all remaining steps: virtual environment setup, Python dependency installation, and verification.

```bash
cd contrib/iboran
chmod +x quick-setup.sh
bash quick-setup.sh
```


## Python Dependencies Note

<a name="python-dependencies-note"></a>

If a virtual environment was not created and activated during the ns3-ai installation, you must install the required Python packages manually:

```bash
pip install -e contrib/ai/python_utils
pip install -e contrib/ai/model/gym-interface/py
```

Then rebuild ns-3 and the ib-ORAN module to apply the changes:

```bash
./ns3 build
./ns3 build ns3iboran_lte
```
