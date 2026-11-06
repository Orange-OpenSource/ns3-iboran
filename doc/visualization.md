# Visualization

This document describes how to view ib-ORAN runs. Paths are relative to the ns-3 root.

## NetAnim

Each run writes an XML animation file for [NetAnim](https://www.nsnam.org/wiki/NetAnim), which shows node positions, mobility and packet transmissions. The file is `iboran-lte-anim.xml` or `iboran-nr-anim.xml`, depending on the RAT, and is written to `contrib/iboran/notebook/netanim/`. Each run overwrites the previous file of the same RAT.

### Building NetAnim

NetAnim requires Qt (see the [NetAnim wiki page](https://www.nsnam.org/wiki/NetAnim) for supported versions). Clone and build it in the ns-3 root:

```shell
git clone https://gitlab.com/nsnam/netanim.git
cd netanim
mkdir build && cd build
cmake .. && cmake --build .
```

### Opening an animation

Start NetAnim from `netanim/build`:

```shell
./netanim
```

Then open `iboran-lte-anim.xml` or `iboran-nr-anim.xml` from `contrib/iboran/notebook/netanim/`.
