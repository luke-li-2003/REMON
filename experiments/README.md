# Experiments

TPC-H benchmark experiments for evaluating Remon remote memory performance.

## Prerequisites

Before running experiments, ensure you have built the project using the top-level `setup.pl`:

```bash
cd ..
./setup.pl
```

## Quick Start

1. **Generate TPC-H data** (scale factor 1 = ~1GB):
   ```bash
   ./tpch_gen.pl testset_1 1
   ```

2. **Build the experiment executable**:
   ```bash
   ./build.pl
   ```

3. **Run the experiment**:
   ```bash
   ./run.pl my_results
   ```

Results are saved to `./results/my_results/`.

## Directory Structure

```
experiments/
├── build.pl          # Build script for experiment executable
├── run.pl            # Example experiment runner
├── tpch_gen.pl       # TPC-H data generation script
├── default.config    # Remon configuration (RAM limit, memory node IP, etc.)
├── main.cpp          # Experiment harness source code
├── CMakeLists.txt    # CMake build configuration
└── tpch/             # TPC-H tools (dbgen, qgen, query templates)
```

## Configuration

Edit `default.config` to customize:

- `MAX_RAM_IN_GB`: Local RAM limit before swapping to remote memory
- `MAX_VM_IN_GB`: Maximum virtual memory size
- `PEER`: Memory node IP address (leave empty for local-only mode)
- `SWAP_DISK`: Path to local swap directory
- `PAGE_SIZE`: Page size in bytes

## Running with Remote Memory

1. Start the memory node server:
   ```bash
   cd ../memory_node/build-release
   ./memory_node
   ```

2. Update `default.config` with the memory node IP:
   ```
   PEER: <memory-node-ip>
   ```

3. Run experiments as usual.

## Generating Larger Datasets

```bash
./tpch_gen.pl testset_2 2    # ~2GB
./tpch_gen.pl testset_4 4    # ~4GB
./tpch_gen.pl testset_10 10  # ~10GB
```

