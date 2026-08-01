import argparse
import json
import time
from collections import deque
from pathlib import Path

import matplotlib.pyplot as plt
import matplotlib.animation as animation

DEFAULT_PATH = Path("PATH/TO/replica-telemetry.jsonl")
DEFAULT_WINDOW = 100
STAGE_FILTER = "apply"
PROCESS_FILTER = "startup"


class TailReader:
    """Follows a growing JSON Lines file, tolerant to rotation/truncation."""

    def __init__(self, path: Path):
        self.path = path
        self.file = None
        self.inode = None
        self.position = 0

    def _ensure_open(self):
        stat = self.path.stat()

        needs_reopen = (
                self.file is None
                or stat.st_ino != self.inode
                or stat.st_size < self.position
        )

        if needs_reopen:
            if self.file is not None:
                self.file.close()

            self.file = self.path.open("r", encoding="utf-8")
            self.inode = stat.st_ino
            self.position = 0

    def read_new_lines(self):
        try:
            self._ensure_open()
        except FileNotFoundError:
            return []

        lines = self.file.readlines()
        self.position = self.file.tell()
        return lines


def parse_events(lines):
    for line in lines:
        line = line.strip()
        if not line:
            continue
        try:
            yield json.loads(line)
        except json.JSONDecodeError:
            continue


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", nargs="?", default=str(DEFAULT_PATH))
    parser.add_argument("--window", type=int, default=DEFAULT_WINDOW,
                        help="number of most recent points to display")
    parser.add_argument("--interval", type=float, default=300,
                        help="chart refresh interval in milliseconds")
    args = parser.parse_args()

    path = Path(args.path)
    window = args.window

    reader = TailReader(path)

    timestamps = deque(maxlen=window)
    durations_ms = deque(maxlen=window)
    seq = deque(maxlen=window)
    counter = 0

    fig, ax = plt.subplots(figsize=(10, 5))
    line, = ax.plot([], [], marker="o", markersize=3, linewidth=1,
                    color="#1f77b4")
    ax.set_title(f"WAL receiver write delay, live (last {window} events)")
    ax.set_xlabel("event #")
    ax.set_ylabel("duration (ms)")
    ax.grid(True, alpha=0.3)

    def update(_frame):
        nonlocal counter

        lines = reader.read_new_lines()
        new_events = 0

        for event in parse_events(lines):
            if event.get("process") != PROCESS_FILTER:
                continue
            if event.get("stage") != STAGE_FILTER:
                continue

            duration_us = event.get("duration_us", 0)
            counter += 1

            seq.append(counter)
            durations_ms.append(duration_us / 1000)
            timestamps.append(event.get("ts", ""))
            new_events += 1

        if new_events == 0:
            return line,

        line.set_data(list(seq), list(durations_ms))

        ax.relim()
        ax.autoscale_view()

        if timestamps:
            ax.set_title(
                f"WAL receiver write delay, live "
                f"(last {window} events, latest at {timestamps[-1]})"
            )

        return line,

    ani = animation.FuncAnimation(
        fig, update, interval=args.interval, blit=False, cache_frame_data=False
    )

    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()