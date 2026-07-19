#!/usr/bin/env python3
"""Summarize REMON MEM_INFO allocation printouts by allocation size.

The parser understands both raw REMON allocation records:

    MEM_INFO a 2048 ...

and allocator printouts where the allocator name is followed by numbers:

    MEM_INFO: VectorCacheBuffer 2048 2048 bytes 1
    VectorCacheBuffer 2048 2048 bytes 1

For allocator printouts, only the first number after the allocator name is used.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Iterable


RAW_ALLOCATION_RE = re.compile(r"\bMEM_INFO\s+a\s+(\d+)\b")
RAW_FREE_RE = re.compile(r"\bMEM_INFO\s+f\b")
MEM_INFO_PREFIX = "MEM_INFO:"
ALLOCATOR_LINE_RE = re.compile(
    r"^\s*(?P<allocator>[A-Za-z_][A-Za-z0-9_:<>~]*)\s+(?P<size>\d+)\b"
)
NON_ALLOCATOR_TOKENS = {
    "Debug",
    "Error",
    "Info",
    "MEM_INFO",
    "Trace",
    "Warn",
    "Warning",
    "Query",
    "SIM",
    "STB"
}


def allocator_body(line: str) -> str | None:
    """Return the allocator-oriented part of a MEM_INFO line, if present."""
    if RAW_ALLOCATION_RE.search(line) or RAW_FREE_RE.search(line):
        return None

    if MEM_INFO_PREFIX in line:
        return line.split(MEM_INFO_PREFIX, 1)[1].strip()

    return None
    # Also support cleaned logs that contain only the allocator printout body.
    return line.strip()


def parse_allocator_printout(line: str) -> tuple[str, int] | None:
    body = allocator_body(line)
    if not body:
        return None

    m = ALLOCATOR_LINE_RE.match(body)
    if not m:
        return None

    allocator = m.group("allocator")
    if allocator in NON_ALLOCATOR_TOKENS:
        return None

    return allocator, int(m.group("size"))


def count_meminfo(
    lines: Iterable[str],
) -> tuple[Counter[int], dict[int, Counter[str]]]:
    total_allocations: Counter[int] = Counter()
    allocator_counts: dict[int, Counter[str]] = defaultdict(Counter)

    for line in lines:
        allocation_match = RAW_ALLOCATION_RE.search(line)
        if allocation_match:
            total_allocations[int(allocation_match.group(1))] += 1
            continue

        allocator_printout = parse_allocator_printout(line)
        if allocator_printout:
            allocator, size = allocator_printout
            allocator_counts[size][allocator] += 1

    return total_allocations, allocator_counts


def build_rows(
    total_allocations: Counter[int],
    allocator_counts: dict[int, Counter[str]],
) -> tuple[list[dict[str, int]], list[str]]:
    allocators = sorted({
        allocator
        for counts in allocator_counts.values()
        for allocator in counts
    })
    sizes = sorted(set(total_allocations) | set(allocator_counts))
    rows: list[dict[str, int]] = []

    for size in sizes:
        known_total = sum(allocator_counts[size].values())
        raw_total = total_allocations[size]
        has_raw_total = raw_total > 0
        total = raw_total if has_raw_total else known_total

        row = {"size_bytes": size, "total_allocations": total}
        for allocator in allocators:
            row[allocator] = allocator_counts[size][allocator]
        row["Unknown"] = total - known_total if has_raw_total else 0
        rows.append(row)

    return rows, allocators


def print_table(rows: list[dict[str, int]], allocators: list[str]) -> None:
    headers = ["size_bytes", "total_allocations", *allocators, "Unknown"]
    widths = {
        header: max(len(header), *(len(str(row[header])) for row in rows))
        for header in headers
    }

    print("  ".join(header.rjust(widths[header]) for header in headers))
    print("  ".join("-" * widths[header] for header in headers))
    for row in rows:
        print("  ".join(str(row[header]).rjust(widths[header]) for header in headers))


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Summarize MEM_INFO allocations by size and allocator."
    )
    parser.add_argument("log_file", type=Path, help="Path to the log file to parse.")
    return parser


def main() -> int:
    args = build_arg_parser().parse_args()

    try:
        with args.log_file.open("r", encoding="utf-8", errors="replace") as log_file:
            total_allocations, allocator_counts = count_meminfo(log_file)
    except OSError as error:
        print(f"Could not read {args.log_file}: {error}", file=sys.stderr)
        return 1

    rows, allocators = build_rows(total_allocations, allocator_counts)
    if not rows:
        print(f"No MEM_INFO allocation records found in {args.log_file}")
        return 1

    print_table(rows, allocators)

    negative_unknown_rows = [row for row in rows if row["Unknown"] < 0]
    if negative_unknown_rows:
        print(
            "\nWarning: some sizes have more allocator printouts than raw "
            "allocation records, so Unknown is negative for those rows.",
            file=sys.stderr,
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
