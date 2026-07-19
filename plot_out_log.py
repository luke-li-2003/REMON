#!/usr/bin/env python3

import argparse
import math
import re
from collections import defaultdict, deque

import matplotlib.pyplot as plt


# Example:
# MEM_INFO: ColumnData 4096 2304 bytes
CLASS_RE = re.compile(
    r"MEM_INFO:\s+([A-Za-z_][A-Za-z0-9_]*)\s+(\d+).*bytes"
)

# Example:
# Info (...) : MEM_INFO a 4096 0x... pid timestamp
RAW_ALLOC_RE = re.compile(
    r"MEM_INFO\s+a\s+(\d+)\s+0x[0-9a-fA-F]+\s+\d+\s+(\d+)"
)

ITER_RE = re.compile(r">>> RUN ITERATION (\d+)")
iterations = []


def parse_log(filename):
    """
    Returns a list of
        (time_ns, size, allocator_name)
    """

    pending = defaultdict(deque)
    allocations = []

    with open(filename, "r", errors="ignore") as f:
        current_iteration = None
        for line in f:

            # --------- iterations ----------------
            m = ITER_RE.search(line)
            if m:
                current_iteration = int(m.group(1))
                iterations.append((current_iteration, None))   # timestamp to be filled later
                continue

            # ---------- class allocator ----------
            m = CLASS_RE.search(line)
            if m:
                allocator = m.group(1)
                size = int(m.group(2))
                pending[size].append(allocator)
                continue

            # ---------- raw allocation ----------
            m = RAW_ALLOC_RE.search(line)
            if m:
                size = int(m.group(1))
                timestamp = int(m.group(2))

                if pending[size]:
                    allocator = pending[size].popleft()
                else:
                    allocator = "Unknown"

                allocations.append((timestamp, size, allocator))

                # Record timestamp for the most recent iteration marker
                if iterations and iterations[-1][1] is None:
                    iterations[-1] = (iterations[-1][0], timestamp)

    return allocations


def main():
    parser = argparse.ArgumentParser(
        description="Plot DuckDB allocation timeline."
    )

    parser.add_argument(
        "logfile",
        help="Input allocation log"
    )

    parser.add_argument(
        "-o",
        "--output",
        help="Export figure instead of displaying it"
    )

    args = parser.parse_args()

    allocs = parse_log(args.logfile)

    if not allocs:
        print("No allocations found.")
        return

    t0 = 0#min(a[0] for a in allocs)

    xs = [(a[0] - t0) / 1e9 for a in allocs]
    ys = [a[1] for a in allocs]
    names = [a[2] for a in allocs]

    unique = sorted(set(names))

    cmap = plt.get_cmap("tab20")

    fig, ax = plt.subplots(figsize=(14, 8))

    for i, name in enumerate(unique):
        idx = [j for j, n in enumerate(names) if n == name]

        ax.scatter(
            [xs[j] for j in idx],
            [ys[j] for j in idx],
            s=8,
            alpha=0.5,
            label=name
        )

    # rulers
    iteration_times = [
        (idx, (ts - t0) / 1e9)
        for idx, ts in iterations
        if ts is not None
    ]
    for idx, t in iteration_times:
        ax.axvline(
            x=t,
            color="black",
            linestyle="--",
            linewidth=0.8,
            alpha=0.5,
        )
    
        ax.text(
            t,
            1.01,
            f"Iter {idx}",
            rotation=90,
            transform=ax.get_xaxis_transform(),
            ha="center",
            va="bottom",
            fontsize=8,
        )

    # ----- x axis -----
    ax.set_xlim(0, 20)
    ax.set_xlabel("Elapsed Time (s)")

    # ----- y axis -----
    ax.set_yscale("log", base=2)

    min_size = min(ys)
    max_size = max(ys)

    low = int(math.floor(math.log2(min_size)))
    high = int(math.ceil(math.log2(max_size)))

    ticks = [2 ** i for i in range(low, high + 1)]

    ax.set_yticks(ticks)
    ax.set_yticklabels([f"$2^{{{i}}}$" for i in range(low, high + 1)])

    ax.set_ylabel("Allocation Size (bytes)")

    ax.grid(True, which="both", alpha=0.3)

    ax.legend(
        fontsize=8,
        markerscale=2,
        bbox_to_anchor=(1.02, 1),
        loc="upper left",
    )

    plt.tight_layout()

    if args.output:
        plt.savefig(args.output, dpi=300)
    else:
        plt.show()


if __name__ == "__main__":
    main()
