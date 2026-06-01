# Contributing to ib-ORAN

Thank you for your interest in contributing! ib-ORAN is an open research project and contributions of all kinds are welcome — whether it's a new scheduling algorithm, a new rApp/xApp/dApp, a bug fix, or a documentation improvement.

Please follow the guidelines below to keep the process smooth for everyone.

## Table of Contents

1. [Getting Started](#getting-started)
2. [Code Style](#code-style)
3. [Adding Apps (rApp / xApp / dApp)](#adding-apps-rapp--xapp--dapp)
4. [Documentation](#documentation)
5. [Reporting Issues](#reporting-issues)


## Getting Started

1. **Fork the repository** and clone your fork locally.
2. **Create a branch** from `main` using a descriptive name:
   - `feature/your-feature` for new functionality
   - `fix/your-fix` for bug fixes
   - `docs/your-update` for documentation changes
3. **Make your changes**, following the code style guidelines below.
4. **Test your changes** before submitting (see [Running](#running) in the main README).
5. **Submit a pull request** with a clear title and description of what was changed and why.

For large or structural changes, consider opening an issue first to discuss the approach before investing significant time.


## Code Style

- **C++**: Follow the existing [ns-3 coding conventions](https://www.nsnam.org/develop/contributing-code/coding-style/).
- **Python**: Formatted with [Black](https://github.com/psf/black). Run `black .` before submitting. Use type hints where applicable.
- Keep functions focused and well-named.
- Add inline comments for non-obvious logic.
- Avoid introducing new dependencies without prior discussion.


## Adding Apps (rApp / xApp / dApp)

ib-ORAN is designed to be extended with new Open RAN Apps. All three app types (rApp, xApp, dApp) are developed using Object-Oriented Programming (OOP). The recommended starting point is to study the base module implementation and the existing apps as reference before developing your own.

### Extending the C++/Python Interface

If your app requires new message content or additional interfaces between the ns-3 C++ simulation and the Python-based policy logic, you must adapt the relevant C++ and `.py` modules inside [`/model/iboran_interfaces/`](/model/iboran_interfaces/). After doing so:

1. Regenerate the corresponding Python bindings by performing a clean rebuild of ns-3 from scratch:
   ```bash
   ./ns3 clean
   ./ns3 configure --enable-examples --enable-tests
   ./ns3 build
   ```
2. Verify that the updated bindings are correctly reflected before proceeding with your app logic.

### Checklist

When submitting a new app, ensure the following:

- Follow the interface conventions for the relevant timescale (non-RT, near-RT, or real-time).
- Reference the appropriate service model in your implementation and documentation.
- Add an entry to the [App catalog](/doc/apps-catalog.md) describing your app, its control timescale, target use case, and the algorithm(s) it implements.
- Include at least one usage example or test scenario.


## Documentation

- Update the README or relevant `doc/` files if your change affects user-facing behavior.
- Update [`RELEASE_NOTES.md`](RELEASE_NOTES.md) for significant changes.
- If you add a new parameter or CLI flag to `iboran_run.py`, update the parameters table in the main README accordingly.


## Reporting Issues

Please report bugs and feature requests via the project's [issue tracker](../../issues). To help us reproduce and address the problem efficiently, include:

- ib-ORAN version (commit hash or release tag)
- ns-3 version
- System configuration (OS, compiler version, Python version)
- Steps to reproduce
- Expected vs. actual behavior
- Any relevant error output or logs