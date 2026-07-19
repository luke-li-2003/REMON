#!/usr/bin/env python3

import argparse
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(
        description="Run TPC-H benchmark with simplified arguments"
    )

    parser.add_argument(
        "cores",
        type=int,
        choices=range(1, 9),
        help="Number of CPU cores to use (starting from CPU 4)"
    )
    parser.add_argument(
        "scale_factor",
        type=int,
        choices=range(1, 20),
        help="TPC-H scale factor"
    )
    parser.add_argument(
        "round",
        type=int,
        help="Number of benchmark rounds"
    )
    parser.add_argument(
        "queries",
        help="Comma-separated query list (e.g. 1,3,5)"
    )
    parser.add_argument(
        "stdout_file",
        nargs="?",
        default=None,
        help="Optional file to redirect stdout"
    )

    args = parser.parse_args()

    # Generate CPU affinity string
    cpu_range = f"4-{args.cores + 3}"

    # Generate log name
    log_name = (
        f"tpch_ScaleFactor_{args.scale_factor}"
        f"_Round_{args.round}"
        f"_Queries_{args.queries}"
    )

    cmd = [
        "taskset",
        "-c",
        cpu_range,
        "./build/experiment",
        "tpch_stream",
        f"-dataDir=./tpch/testset_{args.scale_factor}/",
        "-queryDir=./tpch/queries/",
        "-logDir=.",
        f"-logName={log_name}",
        '-logData="Selected TPC-H queries"',
        f"-Rounds={args.round}",
        f"-Queries={args.queries}",
    ]

    if (args.queries == "all"):
        cmd = cmd[:-1]

    print("Running:")
    print(" ".join(cmd))

    # Redirect stdout if requested
    stdout = None
    if args.stdout_file:
        stdout = open(args.stdout_file, "w")

    try:
        subprocess.run(
            cmd,
            stdout=stdout,
            check=True
        )
    finally:
        if stdout:
            stdout.close()


if __name__ == "__main__":
    main()
