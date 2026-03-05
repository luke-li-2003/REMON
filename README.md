# REMON

REMON (Remote External Memory Over Networks) is a user-space disaggregated memory
system that extends a server's memory using remote DRAM over standard TCP/IP. It
manages memory at page granularity and transparently moves pages between local
RAM, remote memory nodes, and an optional local disk swap. This repo contains the
research prototype, a DuckDB integration, and the benchmarking harness.

Highlights of REMON:
- Runs entirely in user space over commodity networks.
- Fallback to local disk swap when remote memory is unavailable.
- On analytical benchmarks, REMON reduces latency vs local disk swap under
  memory pressure and stays close to in-memory performance for RAM-fit datasets.

## System Overview

REMON is composed of a compute node and one or more memory nodes:
- Compute node: runs the database/application and manages virtual memory in user
  space. It handles page faults, tracks page states, and decides when to keep
  pages local, move them to remote memory, or fall back to disk.
- Memory node: contributes a pool of remote DRAM and serves page storage and
  retrieval over the network, enabling memory expansion without specialized
  hardware or kernel changes.
- Optional local swap: used only as a fallback when remote memory is unavailable
  or constrained.

## Build and Run

REMON builds and runs on Ubuntu 22.04.

### Installation

Install dependencies and build core components:
   ```bash
   ./setup.pl
   ```

### Configure REMON

Copy `experiments/default.config` to `experiments/build/remon.config` and modify the config file accordingly:
- `MAX_RAM_IN_GB`: local RAM limit before swapping
- `MAX_VM_IN_GB`: maximum virtual memory size
- `PEER`: memory node IP (**leave empty for local-only mode**)
- `SWAP_DISK`: local swap directory
- `PAGE_SIZE`: page size in bytes

If no `PEER` is configured, REMON falls back to local disk swap when needed. For
remote memory, the initial TCP connection is expected to succeed and remain
connected.

### Start a memory node

Run this on a host that will provide remote memory:
```bash
cd memory_node/build-release
./memory_node
```

### Using DuckDB with REMON

DuckDB is built under `duckdb/` with allocator changes that route memory
allocation through REMON and safely handle page faults on system calls.

To run DuckDB with REMON:
```bash
export LD_LIBRARY_PATH="$(pwd)/compute_node/build-release:$LD_LIBRARY_PATH"
./duckdb/build-release/duckdb
```

## Experiments (TPC-H)

The `experiments/` directory contains the evaluation harness and TPC-H tooling.

Quick start:
```bash
cd experiments

./tpch_gen.pl testset_1 1
./build.pl
./run.pl my_results
```

Results are written to `experiments/results/my_results/`.
  in `experiments/tpch/testset_1/`.

## Acknowledgements

We acknowledge the contributions of several individuals to early stages of this work. Zongxin Liu developed and implemented the REMON prototype during his engagement with the research group from 2022 to 2024. Bryan Yan conducted early experimental evaluations of REMON. Wing Piu Lee introduced us to R3map and performed initial experiments assessing its performance. This work was in part funded by NSERC and ORF.
