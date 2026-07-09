#!/usr/bin/env python3

import os
import sys


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <input_file>")
        sys.exit(1)

    input_file = sys.argv[1]

    if not os.path.isfile(input_file):
        print(f"Error: File '{input_file}' not found.")
        sys.exit(1)

    base, ext = os.path.splitext(input_file)
    output_file = f"{base}_cleanup{ext}"

    with open(input_file, "r", encoding="utf-8") as infile, \
         open(output_file, "w", encoding="utf-8") as outfile:

        for line in infile:
            if "MEM_INFO" not in line:
                continue

            # Keep everything after "MEM_INFO "
            cleaned = line.split("MEM_INFO", 1)[1].lstrip()
            outfile.write(cleaned)

    print(f"Cleaned file written to: {output_file}")


if __name__ == "__main__":
    main()
