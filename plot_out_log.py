#!/usr/bin/env python3

import re
import argparse
import sys
from collections import defaultdict
import math

import matplotlib.pyplot as plt
import matplotlib.ticker as mticker

# Matches lines like:
# MEM_INFO: ArenaChunk 4096 bytes
# MEM_INFO: VectorCacheBuffer 2048 2048 bytes 1
# MEM_INFO: ColumnData 4096 2304 bytes
class_pattern = re.compile(
    r"^MEM_INFO:\s+([A-Za-z0-9_]+)\s+(\d+)"
)

# Matches lines like:
# Info (...) : MEM_INFO a 4096 0x... pid timestamp
raw_alloc_pattern = re.compile(
    r"MEM_INFO a\s+(\d+)\s+0x[0-9a-fA-F]+\s+\d+\s+(\d+)"
)

pending_class = None

allocations = []

parser = argparse.ArgumentParser()
parser.add_argument("logfile")
parser.add_argument("outfile", nargs='?', default=None)
args = parser.parse_args()

with open(args.logfile, "r", errors="ignore") as f:
    for line in f:

        # Remember the most recent class allocator
        m = class_pattern.match(line)
        if m:
            pending_class = {
                "class": m.group(1),
                "size": int(m.group(2)),
            }
            continue

        # Look for raw allocation
        m = raw_alloc_pattern.search(line)
        if m:
            size = int(m.group(1))
            timestamp = int(m.group(2))

            if pending_class is not None:
                allocator = pending_class["class"]

                # Optional sanity check
                if pending_class["size"] != size:
                    print(
                        f"Warning: class size {pending_class['size']} "
                        f"!= raw size {size}"
                    )
                    print(line)
            else:
                allocator = "unknown"

            allocations.append((timestamp, size, allocator))

            # Consume the pending class
            pending_class = None

# ---------------- Plot ----------------

if not allocations:
    raise RuntimeError("No allocations found.")

t0 = allocations[0][0]

by_allocator = defaultdict(lambda: ([], []))

for ts, size, allocator in allocations:
    x = (ts - t0) / 1e9  # ns -> seconds
    by_allocator[allocator][0].append(x)
    by_allocator[allocator][1].append(size)

plt.figure(figsize=(14, 7))

for allocator, (xs, ys) in sorted(by_allocator.items()):
    if allocator == "unknown":
        plt.scatter(xs, ys, s=4, alpha=0.1, label=allocator)
    else:
        plt.scatter(xs, ys, s=8, alpha=0.7, label=allocator)

# ----- Log2 y-axis -----

plt.yscale("log", base=2)

# Determine range of powers of 2 to display
min_size = min(size for _, size, _ in allocations)
max_size = max(size for _, size, _ in allocations)

min_exp = int(math.floor(math.log2(min_size)))
max_exp = int(math.ceil(math.log2(max_size)))

ticks = [2**e for e in range(min_exp, max_exp + 1)]

ax = plt.gca()
ax.set_yticks(ticks)

# Label as powers of two
ax.set_yticklabels([f"$2^{{{e}}}$" for e in range(min_exp, max_exp + 1)])

# Optional: disable minor ticks
ax.yaxis.set_minor_locator(mticker.NullLocator())

plt.xlabel("Time elapsed (s)")
plt.ylabel("Allocation size (bytes)")
plt.xlim(0, 10)
plt.ylim(64, 2**26)
plt.title("DuckDB Memory Allocations")
plt.grid(True, which="major", alpha=0.3)

plt.legend(markerscale=2, fontsize=8)
plt.tight_layout()
if args.outfile:
	plt.savefig(args.outfile, dpi=300)
plt.show()

