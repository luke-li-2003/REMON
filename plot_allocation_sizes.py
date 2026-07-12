import argparse
import pandas as pd
import matplotlib.pyplot as plt


def summarize_second_column(filename):
    # Read a whitespace-separated file (spaces and/or tabs)
    df = pd.read_csv(filename, sep=r"\s+", header=None)

    if df.shape[1] < 2:
        raise ValueError("Input file must contain at least two columns.")

    # Count occurrences in the second column
    second_col = df.iloc[:, 1]
    counts = second_col.value_counts().sort_index()

    # Calculate percentages
    summary = pd.DataFrame({
        "Count": counts,
        "Percentage": (counts / counts.sum() * 100).round(2)
    })

    # Prevent pandas from truncating the output
    with pd.option_context(
        "display.max_rows", None,
        "display.max_columns", None,
        "display.width", None,
        "display.max_colwidth", None,
    ):
        print("\nSummary of unique values in the second column:")
        print("-" * 50)
        print(summary)
        print("-" * 50)
        print(f"Total entries: {counts.sum()}")

    # Generate pie chart
    plt.figure(figsize=(8, 8))
    plt.pie(
        counts,
        labels=counts.index.astype(str),
        autopct="%1.1f%%",
        startangle=90
    )
    plt.title("Distribution of Second Column Values")
    plt.axis("equal")

    output_file = "second_column_pie.png"
    #plt.savefig(output_file, dpi=300, bbox_inches="tight")
    #print(f"\nPie chart saved as '{output_file}'.")

    plt.show()


def main():
    parser = argparse.ArgumentParser(
        description="Summarize the values in the second column of a whitespace-separated file."
    )
    parser.add_argument(
        "filename",
        help="Path to the whitespace-separated input file."
    )

    args = parser.parse_args()

    summarize_second_column(args.filename)


if __name__ == "__main__":
    main()
