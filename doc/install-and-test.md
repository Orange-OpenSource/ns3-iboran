# Installation and Testing Guide

Complete, step-by-step instructions for building ib-ORAN on top of ns-3.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Installation](#installation)
   - [Step 1: Clone the Repository](#step-1-clone-the-repository)
   - [Step 2: Re-Configure and Re-Build ns-3](#step-2-re-configure-and-re-build-ns-3)
   - [Step 3: Build the ib-ORAN Module](#step-3-build-the-ib-oran-module)
   - [Step 4: Set Python Path](#step-4-set-python-path)
   - [Step 5: Run the Quick Setup Script](#step-5-run-the-quick-setup-script)
3. [Testing](#testing)
   - [Parameters](#parameters)
   - [Examples](#examples)
4. [Python Dependencies Note](#python-dependencies-note)


## Prerequisites

| Dependency | Version |
|---|---|
| [ns-3](https://gitlab.com/nsnam/ns-3-dev) | 3.48 |
| [ns3-ai](https://github.com/hust-diangroup/ns3-ai/blob/main/docs/install.md) | 1.2.0 |
| [ns-3 nr](https://gitlab.com/cttc-lena/nr) | 4.2 |
| CMake | 3.10+ |
| Boost C++ libraries | 1.89+ |
| Protocol Buffers | 6.33.0+ |
| pybind11 | 3.0+ |
| Python | 3.11+ |
| g++ or clang++ | 11.1+ / 17.0+ |

> **Tip:** It is strongly recommended to use a Python virtual environment (see [Python Dependencies Note](#python-dependencies-note) below).


## Installation

Ensure you have successfully installed [ns-3](https://gitlab.com/nsnam/ns-3-dev) along with its required components:

- [ns3-ai](https://github.com/hust-diangroup/ns3-ai/blob/main/docs/install.md) — cloned into `contrib/ai`
- [ns-3 nr](https://gitlab.com/cttc-lena/nr) — cloned into `contrib/nr`

Then follow the steps below.

> **Tip:** If you encounter configuration errors related to inconsistent function definitions while installing ns3-ai, try deleting the `/examples` folder and rebuilding.

### Step 1: Clone the Repository

Clone ib-ORAN into the `contrib/iboran` directory inside your ns-3 root.

Via SSH *(recommended for contributors)*:
```bash
cd /path/to/ns-3
git clone git@github.com:Orange-OpenSource/ns3-iboran.git contrib/iboran
```

Via HTTPS:
```bash
cd /path/to/ns-3
git clone https://github.com/Orange-OpenSource/ns3-iboran.git contrib/iboran
```

### Step 2: Re-Configure and Re-Build ns-3

From the ns-3 root directory, re-run configure and build to register the new contrib module:

```bash
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

### Step 3: Build the ib-ORAN Module

```bash
# LTE module
./ns3 build ns3iboran_lte

# NR module
./ns3 build ns3iboran_nr
```

### Step 4: Set Python Path

To allow Python to resolve ib-ORAN's modules correctly — whether running scripts directly or from a Jupyter notebook — add the following to your `~/.bashrc` or `~/.zshrc`:

```bash
cd /path/to/ns-3
export PYTHONPATH="$(realpath ./contrib/iboran):$PYTHONPATH"
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


## Testing

```bash
python3 iboran_run.py [--rat_type RAT_TYPE] [--k K] [--dapp_algo_ue_selection ALGO] ...
```

All parameters are optional. If omitted, default values are used.

### Parameters:

| Parameter | Description | Default |
|---|---|---|
| `--rat_type` | Radio Access Technology: `"LTE"` or `"NR"` | `"LTE"` |
| `--k` | Maximum parallel UEs scheduled per TTI/slot | `1` |
| `--dapp_control_period_sf` | dApp control period (in subframes) | `1` |
| `--xapp_control_period_f` | xApp control period (in frames) | `1` |
| `--dapp_algo_ue_selection` | dApp MAC scheduling algorithm (see the [Built-In Algorithms](/doc/built-in-algos.md)) | `"Random"` |
| `--xapp_algo_rbg_bounding` | xApp RBG bounding algorithm (see the [Built-In Algorithms](/doc/built-in-algos.md)) | `"Bypass"` |
| `--dataset_id` | Identifier of the dataset: `"toy"` or `"market-1501"` | `"toy"` |
| `--data_format` | Application data format: `"IMAGE"`, `"VIDEO"`, `"EMBEDDING"`, or `"TEXT"` | `"IMAGE"` |
| `--embedder` | Embedding extraction model: `"TransReID"` or `"CLIP"` | `"TransReID"` |
| `--oid_range` | Range of object IDs for intent generation (format: `low,high`) | `1,2` |
| `--save_dapp_log` | Save dApp control log to file: `true` or `false` | `false` |
| `--verbose` | Print statistics and KPIs' summary to console: `true` or `false` | `true` |

### Examples:

```bash
# Run with all defaults
python3 iboran_run.py

# NR with Intent-Based Top-k CQI scheduler, k=3, rest set as default
python3 iboran_run.py --rat_type="NR" --k=3 --dapp_algo_ue_selection="ibTopKCqi"
```


## Python Dependencies Note

<a name="python-dependencies-note"></a>

If you did not create and activate a virtual environment during the ns3-ai installation, you must manually activate your desired Python environment and install the required packages by running:

```bash
pip install -e contrib/ai/python_utils
pip install -e contrib/ai/model/gym-interface/py
```

Then rebuild ns-3 and the ib-ORAN module to apply the changes:

```bash
./ns3 configure --enable-examples --enable-tests
./ns3 build
./ns3 build ns3iboran_lte
./ns3 build ns3iboran_nr
```
