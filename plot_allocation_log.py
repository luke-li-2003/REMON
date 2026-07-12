#!/usr/bin/env python3
"""Plot allocation sizes over time from REMON allocation logs.

Expected bare format:
    a <size> <address> <thread_or_pid> <timestamp_ns>
    f <address> <thread_or_pid> <timestamp_ns>

The parser also accepts full REMON log lines that contain the same fields after
`MEM_INFO`, for example:
    Info (...) : MEM_INFO a 4096 0x... 57484 10177545
"""

from __future__ import annotations

import argparse
import math
import os
from pathlib import Path
from typing import Iterable

os.environ.setdefault("MPLCONFIGDIR", "/tmp/matplotlib")

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def parse_allocations(lines: Iterable[str]) -> tuple[list[int], list[int]]:
    """Return allocation timestamps and sizes, ignoring frees and bad lines."""
    timestamps_ns: list[int] = []
    sizes_bytes: list[int] = []

    for line_number, line in enumerate(lines, start=1):
        parts = line.strip().split()
        if not parts:
            continue

        try:
            op_index = parts.index("MEM_INFO") + 1 if "MEM_INFO" in parts else 0
            op = parts[op_index]
        except IndexError:
            continue

        if op == "f":
            continue

        if op != "a":
            continue

        try:
            size_bytes = int(parts[op_index + 1])
            timestamp_ns = int(parts[-1])
        except (IndexError, ValueError):
            print(f"Skipping malformed allocation line {line_number}: {line.rstrip()}")
            continue

        sizes_bytes.append(size_bytes)
        timestamps_ns.append(timestamp_ns)

    return timestamps_ns, sizes_bytes


def powers_of_two_between(min_value: int, max_value: int) -> list[int]:
    """Return powers of two spanning the inclusive value range."""
    first_exponent = max(0, math.floor(math.log2(min_value)))
    last_exponent = math.ceil(math.log2(max_value))
    return [2**exponent for exponent in range(first_exponent, last_exponent + 1)]


def plot_allocations(
    timestamps_ns: list[int],
    sizes_bytes: list[int],
    output_path: Path,
    *,
    relative_time: bool,
) -> None:
    if not timestamps_ns:
        raise ValueError("no allocation lines were found")

    valid_points = [
        (timestamp, size)
        for timestamp, size in zip(timestamps_ns, sizes_bytes)
        if size > 0
    ]
    skipped_points = len(sizes_bytes) - len(valid_points)
    if not valid_points:
        raise ValueError("no positive allocation sizes were found for log-scale plotting")
    if skipped_points:
        print(f"Skipped {skipped_points} allocations with non-positive sizes")

    x_values = [timestamp for timestamp, _ in valid_points]
    y_values = [size for _, size in valid_points]
    x_label = "Timestamp (ns)"

    if relative_time:
        first_timestamp = x_values[0]
        x_values = [timestamp - first_timestamp for timestamp in x_values]
        x_label = "Time since first allocation (ns)"

    fig, ax = plt.subplots(figsize=(12, 6.75))
    ax.scatter(x_values, y_values, s=8, alpha=0.65)

    ax.set_title("Allocation Size Over Time")
    ax.set_xlabel(x_label)
    ax.set_ylabel("Allocation size (bytes)")
    ax.set_yscale("log")

    power_of_two_ticks = powers_of_two_between(min(y_values), max(y_values))
    for tick in power_of_two_ticks:
        ax.axhline(tick, color="0.55", linewidth=0.7, alpha=0.45, zorder=0)
    ax.set_yticks(power_of_two_ticks)
    ax.set_yticklabels([str(tick) for tick in power_of_two_ticks])
    ax.grid(True, axis="x", alpha=0.25)
    fig.tight_layout()

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, dpi=160)
    plt.close(fig)


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Parse REMON allocation logs and plot allocation size over time."
    )
    parser.add_argument(
        "log_file",
        type=Path,
        help="Input log file containing allocation/free lines.",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("allocation_over_time.png"),
        help="Output plot path. Default: allocation_over_time.png",
    )
    parser.add_argument(
        "--relative-time",
        action="store_true",
        help="Plot timestamps relative to the first allocation.",
    )
    return parser


def main() -> None:
    args = build_arg_parser().parse_args()

    with args.log_file.open("r", encoding="utf-8", errors="replace") as log_file:
        timestamps_ns, sizes_bytes = parse_allocations(log_file)

    plot_allocations(
        timestamps_ns,
        sizes_bytes,
        args.output,
        relative_time=args.relative_time,
    )

    print(f"Parsed {len(sizes_bytes)} allocations")
    print(f"Wrote plot to {args.output}")


if __name__ == "__main__":
    main()
