#!/usr/bin/env python3
"""Summarize REMON MEM_INFO allocation sizes and allocator printouts.

For each allocation size this reports:
  - total_allocations: count of `MEM_INFO a <size> ...` lines
  - arenachunk_printouts: count of `MEMINFO: ArenaChunk <size> bytes` lines
  - columndata_printouts: count of `MEMINFO: ColumnData <requested> <actual> bytes`
    lines, keyed by actual allocated size
  - filebuffer_printouts: count of `MEMINFO: FileBuffer <actual> bytes` lines
  - inferred_other_allocations: total_allocations minus known allocator printouts
"""

from __future__ import annotations

import argparse
import csv
import re
import sys
from collections import Counter
from pathlib import Path
from typing import Iterable


MEM_INFO_ALLOC_RE = re.compile(r"\bMEM_INFO\s+a\s+(\d+)\b")
ARENACHUNK_RE = re.compile(r"\bMEM_INFO:\s+ArenaChunk\s+(\d+)\s+bytes\b")
COLUMNDATA_RE = re.compile(r"\bMEM_INFO:\s+ColumnData\s+(\d+)\s+(\d+)\s+bytes\b")
FILEBUFFER_RE = re.compile(r"\bMEM_INFO:\s+FileBuffer\s+(\d+)\s+bytes\b")


def count_meminfo(
    lines: Iterable[str],
) -> tuple[Counter[int], Counter[int], Counter[int], Counter[int]]:
    """Return allocation and allocator printout counts keyed by size in bytes."""
    allocation_counts: Counter[int] = Counter()
    arenachunk_counts: Counter[int] = Counter()
    columndata_counts: Counter[int] = Counter()
    filebuffer_counts: Counter[int] = Counter()

    for line in lines:
        allocation_match = MEM_INFO_ALLOC_RE.search(line)
        if allocation_match:
            allocation_counts[int(allocation_match.group(1))] += 1

        arenachunk_match = ARENACHUNK_RE.search(line)
        if arenachunk_match:
            arenachunk_counts[int(arenachunk_match.group(1))] += 1

        columndata_match = COLUMNDATA_RE.search(line)
        if columndata_match:
            actual_allocated_size = int(columndata_match.group(1))
            columndata_counts[actual_allocated_size] += 1

        filebuffer_match = FILEBUFFER_RE.search(line)
        if filebuffer_match:
            filebuffer_counts[int(filebuffer_match.group(1))] += 1

    return allocation_counts, arenachunk_counts, columndata_counts, filebuffer_counts


def build_rows(
    allocation_counts: Counter[int],
    arenachunk_counts: Counter[int],
    columndata_counts: Counter[int],
    filebuffer_counts: Counter[int],
) -> list[dict[str, int]]:
    rows: list[dict[str, int]] = []

    all_sizes = (
        set(allocation_counts)
        | set(arenachunk_counts)
        | set(columndata_counts)
        | set(filebuffer_counts)
    )
    for size in sorted(all_sizes):
        total_allocations = allocation_counts[size]
        arenachunk_printouts = arenachunk_counts[size]
        columndata_printouts = columndata_counts[size]
        filebuffer_printouts = filebuffer_counts[size]
        rows.append(
            {
                "size_bytes": size,
                "total_allocations": total_allocations,
                "arenachunk_printouts": arenachunk_printouts,
                "columndata_printouts": columndata_printouts,
                "filebuffer_printouts": filebuffer_printouts,
                "inferred_other_allocations": total_allocations
                - arenachunk_printouts
                - columndata_printouts
                - filebuffer_printouts,
            }
        )

    return rows


def print_table(rows: list[dict[str, int]]) -> None:
    headers = [
        "size_bytes",
        "total_allocations",
        "arenachunk_printouts",
        "columndata_printouts",
        "filebuffer_printouts",
        "inferred_other_allocations",
    ]
    widths = {
        header: max(len(header), *(len(str(row[header])) for row in rows))
        for header in headers
    }

    print(" ".join(header.rjust(widths[header]) for header in headers))
    print(" ".join("-" * widths[header] for header in headers))
    for row in rows:
        print(" ".join(str(row[header]).rjust(widths[header]) for header in headers))


def write_csv(rows: list[dict[str, int]], output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", newline="", encoding="utf-8") as output_file:
        writer = csv.DictWriter(
            output_file,
            fieldnames=[
                "size_bytes",
                "total_allocations",
                "arenachunk_printouts",
                "columndata_printouts",
                "filebuffer_printouts",
                "inferred_other_allocations",
            ],
        )
        writer.writeheader()
        writer.writerows(rows)


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Analyze REMON MEM_INFO allocations and allocator printouts by size."
        )
    )
    parser.add_argument(
        "log_file",
        type=Path,
        help="Path to the REMON output/log file to analyze.",
    )
    parser.add_argument(
        "-o",
        "--output-csv",
        type=Path,
        help="Optional path for writing the summary as CSV.",
    )
    return parser


def main() -> int:
    args = build_arg_parser().parse_args()

    with args.log_file.open("r", encoding="utf-8", errors="replace") as log_file:
        (
            allocation_counts,
            arenachunk_counts,
            columndata_counts,
            filebuffer_counts,
        ) = count_meminfo(log_file)

    rows = build_rows(
        allocation_counts,
        arenachunk_counts,
        columndata_counts,
        filebuffer_counts,
    )
    if not rows:
        print(f"No MEM_INFO allocations or allocator printouts found in {args.log_file}")
        return 1

    print_table(rows)

    negative_rows = [
        row for row in rows if row["inferred_other_allocations"] < 0
    ]
    if negative_rows:
        print(
            "\nWarning: some sizes have more known allocator printouts than "
            "allocation lines, so their inferred other-allocation count is negative.",
            file=sys.stderr,
        )

    if args.output_csv:
        write_csv(rows, args.output_csv)
        print(f"\nWrote CSV summary to {args.output_csv}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
