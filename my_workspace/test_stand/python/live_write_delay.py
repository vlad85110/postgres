import json
import sys
import time
from collections import deque
from pathlib import Path

import matplotlib.animation as animation
import matplotlib.pyplot as plt

DEFAULT_WINDOW = 1000
DEFAULT_INTERVAL = 300

def main():
    jsonlPath = Path(sys.argv[1])
    window = DEFAULT_WINDOW

    if len(sys.argv) > 2:
        window = int(sys.argv[2])

    durations_ms = deque(maxlen=window)
    queryNumbers = deque(maxlen=window)
    timestamps = deque(maxlen=window)

    filePosition = 0
    fileInode = None

    fig, ax = plt.subplots(figsize=(10, 5))
    line, = ax.plot([], [], marker="o", markersize=3, linewidth=1,
                    color="#1f77b4")

    ax.set_xlabel("query number")
    ax.set_ylabel("duration (ms)")
    ax.grid(True, alpha=0.3)
    ax.set_title("Query duration")

    def update(frame):
        nonlocal filePosition
        nonlocal fileInode

        try:
            fileStat = jsonlPath.stat()
        except FileNotFoundError:
            return line,

        if fileInode != fileStat.st_ino or fileStat.st_size < filePosition:
            filePosition = 0
            fileInode = fileStat.st_ino
            durations_ms.clear()
            queryNumbers.clear()
            timestamps.clear()

        with open(jsonlPath, "r", encoding="utf-8") as logfile:
            logfile.seek(filePosition)
            lines = logfile.readlines()
            filePosition = logfile.tell()

        for currentLine in lines:
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
        fig, update, interval=DEFAULT_INTERVAL, blit=False,
        cache_frame_data=False
    )

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    main()
