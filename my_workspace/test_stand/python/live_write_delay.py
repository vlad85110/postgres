import argparse
import json
import os
import sys
from collections import deque

import matplotlib.animation as animation
import matplotlib.pyplot as plt

DEFAULT_WINDOW = 1000
DEFAULT_INTERVAL = 300

def parse_args():
    parser = argparse.ArgumentParser(description="Live plot of query durations read as JSONL from stdin.")
    parser.add_argument("--window", type=int, default=DEFAULT_WINDOW,
                        help="number of most recent queries to display")
    parser.add_argument("--interval", type=int, default=DEFAULT_INTERVAL,
                        help="chart refresh interval in milliseconds")
    return parser.parse_args()

def main():
    args = parse_args()
    window = args.window

    os.set_blocking(sys.stdin.fileno(), False)

    durations_ms = deque(maxlen=window)
    queryNumbers = deque(maxlen=window)
    timestamps = deque(maxlen=window)

    fig, ax = plt.subplots(figsize=(10, 5))
    line, = ax.plot([], [], marker="o", markersize=3, linewidth=1,
                    color="#1f77b4")

    ax.set_xlabel("query number")
    ax.set_ylabel("duration (ms)")
    ax.grid(True, alpha=0.3)
    ax.set_title("Query duration")

    def update(frame):
        try:
            lines = sys.stdin.readlines()
        except (BlockingIOError, ValueError):
            lines = []

        if not lines:
            return line,

        for currentLine in lines:
            currentLine = currentLine.strip()
            if not currentLine:
                continue

            try:
                logRecord = json.loads(currentLine)
            except json.JSONDecodeError:
                continue

            if "duration_ms" not in logRecord:
                continue

            durations_ms.append(logRecord["duration_ms"])
            queryNumbers.append(logRecord.get("query_number", len(queryNumbers) + 1))
            timestamps.append(logRecord.get("timestamp", ""))

        if not durations_ms:
            return line,

        line.set_data(list(queryNumbers), list(durations_ms))
        ax.relim()
        ax.autoscale_view()

        ax.set_title(
            f"Query duration (last {window} queries, latest: {timestamps[-1]})"
        )

        return line,

    ani = animation.FuncAnimation(
        fig, update, interval=args.interval, blit=False,
        cache_frame_data=False
    )

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    main()