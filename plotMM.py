from re import X
import os

os.environ.setdefault("MPLCONFIGDIR", "/tmp/matplotlib")

import cmasher as cmr
import csv
import matplotlib as mpl
import matplotlib.pyplot as plt
import matplotlib.colors as colors
import numpy as np
from matplotlib import scale as mscale
from matplotlib import transforms as mtransforms
from matplotlib.ticker import FuncFormatter
from matplotlib.ticker import LogFormatter
import math
import mpl_scatter_density
import warnings

warnings.filterwarnings(
    "ignore",
    message="All-NaN slice encountered",
    category=RuntimeWarning,
    module="mpl_scatter_density.generic_density_artist")

# Config
maxEntries = 0 # 0 = All
prepare_alloc_graph = True
prepare_alloc_size_graph = True
prepare_free_graph = False
prepare_p99 = True
show_plot = False
save_pngs = False 
save_large_pngs = True
y_max = 1000*1000*2
dpi_lo = 80
dpi_hi = 200
title_fontsize = 36
xaxis_fontsize = 20

# Replays
replays = {
    # CRT
    "crtmalloc_1x_writebyte": {
        "csv_filename" : "/tmp/doom3_replayreport_crt_1x_WriteByte.csv",
        "chart_title" : "crt",
        "friendly_name": "crt",
    },
    # Lemon 
    "lemonmalloc_1x_writebyte": {
        "csv_filename" : "/tmp/doom3_replayreport_lemon_1x_WriteByte.csv",
        "chart_title" : "lemon",
        "friendly_name": "lemon",
    },
}

# Replays to process
selected_replays = ["crtmalloc_1x_writebyte", "lemonmalloc_1x_writebyte"]
percentile_replays = [
    "crtmalloc_1x_writebyte", 
    "lemonmalloc_1x_writebyte", 
]


# Constants
kilobyte = 1024.0
megabyte = kilobyte * 1024.0
gigabyte = megabyte * 1024.0

# Utilities
def format_nanoseconds(ns):
    if ns < 1000:
        return f"{ns} ns"
    elif ns < 1000 * 1000:
        return f"{ns/1000} μs"
    elif ns < 1000 * 1000 * 1000:
        return f"{ns/1000/1000} ms"
    else:
        return f"{ns/1000/1000/1000} s"

def format_bytes(bytes):
    if bytes < 1024:
        return f"{bytes:.0f} bytes"
    elif bytes == 1024:
        return "1 kilobyte"
    elif bytes < 1024 * 1024:
        return f"{bytes/1024:.0f} kilobytes"
    elif bytes == 1024 * 1024:
        return "1 megabyte" 
    elif bytes < 1024 * 1024 * 1024:
        return f"{bytes/1024/1024} megabytes"
    else:
        return f"{1024.0/1024.0/1024.0} gigabytes"

class ColorbarFormatter(LogFormatter):
    def __call__(self, x, pos = None):
        return format_bytes(x)

def main():
    # Parse data
    mallocMax = 100 * megabyte
    color_min = 1
    color_norm = colors.LogNorm(vmin=color_min, vmax=mallocMax)

    replays_to_process = selected_replays if selected_replays != None else replays.keys()

    p99_data = {}
    os.makedirs("screenshots", exist_ok=True)

    for replay in replays_to_process:

        replay_entry = replays[replay]
        csv_filename = replay_entry["csv_filename"]
        chart_title = replay_entry["chart_title"]
        replay_friendly_name = replay_entry["friendly_name"]

        print(f"Parsing data: {csv_filename}")
        allocTimestamps = []
        allocTimes = []
        freeTimes = []
        allocSizes = []
        freeData = []
        with open(csv_filename) as csv_file:
            reader = csv.reader(csv_file)
            
            # skip header
            next(reader, None)

            # process data
            for row in reader:
                replayTimestamp = float(row[0])
                allocTime = float(row[1])
                allocSize = float(row[2])
                replayFreeTimestamp = float(row[3])
                freeTime = float(row[4])
                allocTimestamps.append(replayTimestamp)
                allocTimes.append(allocTime)
                allocSizes.append(allocSize)
                if replayFreeTimestamp != 0:
                    freeData.append((replayFreeTimestamp, freeTime, allocSize))
                if maxEntries > 0 and len(allocTimes) + len(freeTimes) >= maxEntries:
                    break
        print("Parse Complete\n")

        # Shared plot data
        def x_labels(tick, pos):
            nsPerMinute = 60e9
            nsPerSecond = 1e9
            minutes = int(tick / nsPerMinute)
            seconds = int((tick - minutes*nsPerMinute) / nsPerSecond)
            return f"{minutes}:{seconds:02d}"

        def y_labels(tick, pos):
            return format_nanoseconds(tick)

        def size_labels(tick, pos):
            return format_bytes(tick)

        cbar_ticks = [32,64,128,256,512,kilobyte,kilobyte*10, kilobyte*100, megabyte, megabyte*10, mallocMax]
        cmap = cmr.get_sub_cmap('nipy_spectral', 0.03, 0.96)

        def filtered_density_points(timestamps, times, sizes):
            filtered_timestamps = []
            filtered_times = []
            filtered_sizes = []
            skipped = 0

            for timestamp, time, size in zip(timestamps, times, sizes):
                if not (math.isfinite(timestamp) and math.isfinite(time) and math.isfinite(size)):
                    skipped += 1
                    continue

                if time <= 0 or size <= 0:
                    skipped += 1
                    continue

                filtered_timestamps.append(timestamp)
                filtered_times.append(time)
                filtered_sizes.append(min(size, mallocMax))

            if skipped > 0:
                print(f"Skipped {skipped} rows with invalid log-scale values")

            return filtered_timestamps, filtered_times, filtered_sizes

        def filtered_size_points(timestamps, sizes):
            filtered_timestamps = []
            filtered_sizes = []
            skipped = 0

            for timestamp, size in zip(timestamps, sizes):
                if not (math.isfinite(timestamp) and math.isfinite(size)):
                    skipped += 1
                    continue

                if size <= 0:
                    skipped += 1
                    continue

                filtered_timestamps.append(timestamp)
                filtered_sizes.append(min(size, mallocMax))

            if skipped > 0:
                print(f"Skipped {skipped} rows with invalid allocation size values")

            return filtered_timestamps, filtered_sizes

        # Alloc times
        if prepare_alloc_graph:
            plotAllocTimestamps, plotAllocTimes, plotAllocSizes = filtered_density_points(
                allocTimestamps,
                allocTimes,
                allocSizes)

            fig = plt.figure(figsize=(20,11.25))
            ax = fig.add_subplot(1,1,1, projection='scatter_density')
            density = ax.scatter_density(
                x=plotAllocTimestamps,
                y=plotAllocTimes,
                c=plotAllocSizes,
                cmap=cmap, 
                norm=color_norm)
            plt.semilogy(base=10)
            ax.xaxis.set_major_formatter(FuncFormatter(x_labels))
            ax.yaxis.set_major_formatter(FuncFormatter(y_labels))
            ax.set_ylabel("Alloc Time")
            ax.set_ylim(bottom=3,top=y_max)
            ax.set_xlabel("Replay Time", fontsize=xaxis_fontsize)
            ax.set_title(f"{chart_title} - alloc", fontsize=title_fontsize)
            ax.set_facecolor('#000000')
            fig.colorbar(density, ticks=cbar_ticks, format=ColorbarFormatter())

            save_filename = os.path.splitext(os.path.basename(csv_filename))[0] + "_alloc"
            if save_pngs:
                fig.savefig(f"screenshots/{save_filename}.png", bbox_inches='tight', dpi=dpi_lo)

            if save_large_pngs:
                fig.savefig(f"screenshots/{save_filename}_large.png", bbox_inches='tight', dpi=dpi_hi)

        # Alloc sizes
        if prepare_alloc_size_graph:
            plotAllocTimestamps, plotAllocSizes = filtered_size_points(
                allocTimestamps,
                allocSizes)

            fig = plt.figure(figsize=(20,11.25))
            ax = fig.add_subplot(1,1,1)
            ax.scatter(
                plotAllocTimestamps,
                plotAllocSizes,
                s=3,
                alpha=1,
                c='#0fb5ae',
                linewidths=0,
                rasterized=True)
            plt.semilogy(base=10)
            ax.xaxis.set_major_formatter(FuncFormatter(x_labels))
            ax.yaxis.set_major_formatter(FuncFormatter(size_labels))
            ax.set_ylabel("Alloc Size")
            ax.set_ylim(bottom=1, top=mallocMax)
            ax.set_xlabel("Replay Time", fontsize=xaxis_fontsize)
            ax.set_title(f"{chart_title} - alloc size", fontsize=title_fontsize)
            ax.set_facecolor('#000000')

            save_filename = os.path.splitext(os.path.basename(csv_filename))[0] + "_alloc_size"
            if save_pngs:
                fig.savefig(f"screenshots/{save_filename}.png", bbox_inches='tight', dpi=dpi_lo)

            if save_large_pngs:
                fig.savefig(f"screenshots/{save_filename}_large.png", bbox_inches='tight', dpi=dpi_hi)

        # Free times
        if prepare_free_graph:
            # Sort data by free time
            freeData.sort(key=lambda entry : entry[0])

            # Extract data
            freeTimestamps = [entry[0] for entry in freeData]
            freeTimes = [entry[1] for entry in freeData]
            allocSizes = [min(entry[2], mallocMax) for entry in freeData]
            freeTimestamps, freeTimes, allocSizes = filtered_density_points(
                freeTimestamps,
                freeTimes,
                allocSizes)

            fig = plt.figure(figsize=(20,11.25))
            ax = fig.add_subplot(1,1,1, projection='scatter_density')
            density = ax.scatter_density(
                x=freeTimestamps, 
                y=freeTimes,
                c=allocSizes, 
                cmap=cmap, 
                norm=color_norm)
            plt.semilogy(base=10)
            ax.xaxis.set_major_formatter(FuncFormatter(x_labels))
            ax.yaxis.set_major_formatter(FuncFormatter(y_labels))
            ax.set_ylabel("Free Time")
            ax.set_ylim(bottom=3, top=y_max)
            ax.set_xlabel("Replay Time", fontsize=xaxis_fontsize)
            ax.set_title(f"{chart_title} - free", fontsize=title_fontsize)
            ax.set_facecolor('#000000')
            fig.colorbar(density, ticks=cbar_ticks, format=ColorbarFormatter())

            save_filename = os.path.splitext(os.path.basename(csv_filename))[0] + "_free"
            if save_pngs:
                fig.savefig(f"screenshots/{save_filename}.png", bbox_inches='tight', dpi=dpi_lo)

            if save_large_pngs:
                fig.savefig(f"screenshots/{save_filename}_large.png", bbox_inches='tight', dpi=dpi_hi)

        # Store malloc/free times for p99 plot
        if prepare_p99 and replay in percentile_replays:
            if len(freeTimes) == 0:
                freeTimes = [entry[1] for entry in freeData]

            # Sort times
            allocTimes.sort()
            freeTimes.sort()

            p99_data[replay_friendly_name] = {
                "allocs": allocTimes,
                "frees": freeTimes
            }

    if prepare_p99: 
        # Utility
        def clamp(v, min, max):
            if v < min:
                return min
            elif v > max:
                return max
            else:
                return v

        def lerp(frac, a, b):
            return a*(1-frac) + b*frac

        def lerp_map_range(v, in_min, in_max, out_min, out_max):
            v = clamp(v, in_min, in_max)
            frac = clamp(float(v - in_min) / float(in_max - in_min), 0.0, 1.0)
            return lerp(frac, out_min, out_max)

        for i in range(2):
            fig = plt.figure(figsize=(20,11.25))
            ax = fig.add_subplot()

            plt.semilogy(base=10)
            #ax.xaxis.set_major_formatter(FuncFormatter(x_labels))
            ax.yaxis.set_major_formatter(FuncFormatter(y_labels))
            ax.set_ylabel("Alloc Time")
            ax.set_xlim(left=0, right=101)
            ax.set_ylim(bottom=3, top=y_max)
            ax.set_xlabel("Percentile", fontsize=xaxis_fontsize)
            ax.set_title(f"Alloc Time (Percentile)", fontsize=title_fontsize)
            ax.set_facecolor('#000000')
            ax.tick_params(axis='x', labelsize=xaxis_fontsize-2)

            # colors from https://spectrum.adobe.com/page/color-for-data-visualization/
            p99_colors = ['#0fb5ae', '#4046ca', '#f68511', '#de3d82', '#7e84fa', '#72e06a']
            
            colorIdx = 0
            for key in p99_data:
                allocs = p99_data[key]["allocs"]

                buckets = []

                # Carefully define buckets
                def appendHelper(min, max, step):
                    i = min
                    while i < max:
                        buckets.append(i)
                        i += step
                appendHelper(1.0, 99.9, 0.1)
                appendHelper(99.9, 99.99, 0.01)
                appendHelper(99.99, 99.999, 0.001)
                appendHelper(99.999, 99.9999, 0.0001)

                # Lerp all values from p99.9999 to p100
                p_lo = 99.9999
                p_hi = 100.0
                idx_lo = int(p_lo/100 * len(allocs))
                idx_hi = len(allocs) - 1
                for idx in range(idx_lo+1, idx_hi + 1):
                    p = lerp_map_range(idx, idx_lo, idx_hi, p_lo, p_hi)
                    buckets.append(p)

                # Add end point
                #buckets.append(100)

                alloc_bucket_values = []
                for bucket in buckets:
                    idx = bucket/100.0 * len(allocs)
                    idx = int(min(idx, len(allocs) - 1))
                    alloc_bucket_values.append(allocs[idx])

                ax.plot(buckets,alloc_bucket_values,label=key,color=p99_colors[colorIdx % len(p99_colors)])
                colorIdx = colorIdx + 1
            
            ax.legend(loc='upper left', prop={'size': 14})

            if i == 0:
                # Full size
                ax.set_xlim(left=0, right=101)

                if save_pngs:
                    fig.savefig(f"screenshots/percentile_alloc.png", bbox_inches='tight', dpi=dpi_lo)
                
                if save_large_pngs:
                    fig.savefig(f"screenshots/percentile_alloc_large.png", bbox_inches='tight', dpi=dpi_hi)

               
            else:
                # Zoom image
                def log_map_range(v, in_min, in_max, out_min, out_max):
                    v = clamp(v, in_min, in_max)
                    frac = clamp((v - in_min) / (in_max - in_min), 0.0, 1.0)
                    if frac == 0:
                        return out_min
                    scaled_frac = math.log(lerp(frac, 1, 10), 10)
                    result = lerp(scaled_frac, out_min, out_max)
                    return result

                def transform_one(value):
                    sections = 5
                    section_size = 9 / sections
                    def section_marker(i):
                        return 90 + section_size*i

                    if value <= 99.0:
                        return log_map_range(value, 90.0, 99.0, section_marker(0), section_marker(1))
                    elif value <= 99.9:
                        return log_map_range(value, 99.0, 99.9, section_marker(1), section_marker(2))
                    elif value <= 99.99:
                        return log_map_range(value, 99.9, 99.99, section_marker(2), section_marker(3))
                    elif value <= 99.999:
                        return log_map_range(value, 99.99, 99.999, section_marker(3), section_marker(4))
                    elif value <= 99.9999:
                        return log_map_range(value, 99.999, 99.9999, section_marker(4), section_marker(5))
                    else:
                        return lerp_map_range(value, 99.9999, 100.0, section_marker(5), 100.0)

                def transform_arr(values_arr):
                    return [transform_one(value) for value in values_arr]

                def inverse_transform_one(value):
                    sections = 5
                    section_size = 9 / sections
                    def section_marker(i):
                        return 90 + section_size*i

                    if value <= section_marker(1):
                        return log_map_range(value, section_marker(0), section_marker(1), 90.0, 99.0)
                    elif value <= section_marker(2):
                        return log_map_range(value, section_marker(1), section_marker(2), 99.0, 99.9)
                    elif value <= section_marker(3):
                        return log_map_range(value, section_marker(2), section_marker(3), 99.9, 99.99)
                    elif value <= section_marker(4):
                        return log_map_range(value, section_marker(3), section_marker(4), 99.99, 99.999)
                    elif value <= section_marker(5):
                        return log_map_range(value, section_marker(4), section_marker(5), 99.999, 99.9999)
                    else:
                        return lerp_map_range(value, section_marker(5), 100.0, 99.9999, 100.0)

                def x_scale(values):
                    values = np.asarray(values)
                    values_shape = values.shape
                    transformed = [transform_one(float(value)) for value in values.reshape(-1)]
                    return np.asarray(transformed).reshape(values_shape)

                def x_scale_inverse(values):
                    values = np.asarray(values)
                    values_shape = values.shape
                    transformed = [inverse_transform_one(float(value)) for value in values.reshape(-1)]
                    return np.asarray(transformed).reshape(values_shape)

                ax.set_xscale('function', functions=(x_scale, x_scale_inverse))
                
                # Set major ticks
                ticks = [90, 99, 99.9, 99.99, 99.999, 99.9999, 100]
                tick_labels = ["p90", "p99", "p99.9", "p99.99", "p99.999", "p99.9999", "p100"]
                ax.set_xticks(ticks)
                ax.set_xticklabels(tick_labels, fontsize=xaxis_fontsize-2)
                ax.set_xlim(left=90, right=101)

                # Set minor ticks
                def xlabel_helper(tick, pos):
                    return ''
                ax.xaxis.set_minor_formatter(FuncFormatter(xlabel_helper))
                ax.xaxis.set_minor_locator(plt.FixedLocator([
                    91,92,93,94,95,96,97,98,
                    99.1, 99.2, 99.3, 99.4, 99.5, 99.6, 99.7, 99.8,
                    99.91, 99.92, 99.93, 99.94, 99.95, 99.96, 99.97, 99.98,
                    99.991, 99.992, 99.993, 99.994, 99.995, 99.996, 99.997, 99.998,
                    99.9991, 99.9992, 99.9993, 99.9994, 99.9995, 99.9996, 99.9997, 99.9998,
                    ]))

                if save_pngs:
                    fig.savefig(f"screenshots/percentile_alloc_zoomed.png", bbox_inches='tight', dpi=dpi_lo)

                if save_large_pngs:
                    fig.savefig(f"screenshots/percentile_alloc_zoomed_large.png", bbox_inches='tight', dpi=dpi_hi)

    if show_plot:
        plt.show()

if __name__=="__main__":
    main()
