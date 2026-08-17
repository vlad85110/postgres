import argparse
import errno
import json
import os
import sys
from collections import deque
from dataclasses import dataclass
from typing import Deque, List, Optional, Union

import matplotlib.animation as animation
import matplotlib.pyplot as plt
from matplotlib.axes import Axes
from matplotlib.figure import Figure
from matplotlib.lines import Line2D
from matplotlib.ticker import MaxNLocator

DEFAULT_WINDOW = 100
DEFAULT_INTERVAL = 300
READ_CHUNK_SIZE = 65536


@dataclass
class DelayChangeEvent:
    query_number: int
    delay_type: str
    delay_ms: Union[int, float, str]


@dataclass
class QueryDurationSample:
    query_number: int
    duration_ms: float
    timestamp: str


class JsonLineStreamReader:

    def __init__(self, file_descriptor: int, chunk_size: int = READ_CHUNK_SIZE):
        self._file_descriptor = file_descriptor
        self._chunk_size = chunk_size
        self._pending_bytes = b""
        os.set_blocking(self._file_descriptor, False)

    def read_lines(self) -> List[str]:
        chunks: List[bytes] = []

        while True:
            try:
                chunk = os.read(self._file_descriptor, self._chunk_size)
            except BlockingIOError:
                break
            except OSError as exc:
                if exc.errno == errno.EAGAIN:
                    break
                raise

            if not chunk:
                break

            chunks.append(chunk)

        if not chunks:
            return []

        self._pending_bytes += b"".join(chunks)
        *complete_lines, self._pending_bytes = self._pending_bytes.split(b"\n")

        return [line.decode("utf-8", errors="replace") for line in complete_lines]


class ReplicationDelayMeasurements:

    def __init__(self, window_size: int):
        self.window_size = window_size
        self.query_samples: Deque[QueryDurationSample] = deque(maxlen=window_size)
        self.delay_changes: List[DelayChangeEvent] = []

    def add_json_line(self, raw_line: str) -> None:
        raw_line = raw_line.strip()
        if not raw_line:
            return

        try:
            record = json.loads(raw_line)
        except json.JSONDecodeError:
            return

        if record.get("record_type", "query") == "change_delay":
            self.delay_changes.append(
                DelayChangeEvent(
                    query_number=record.get("query_number"),
                    delay_type=record.get("type_delay", "?"),
                    delay_ms=record.get("delay_ms", "?"),
                )
            )
            return

        if "duration_ms" not in record:
            return

        query_number = record.get("query_number")
        if query_number is None:
            query_number = len(self.query_samples) + 1

        self.query_samples.append(
            QueryDurationSample(
                query_number=query_number,
                duration_ms=record["duration_ms"],
                timestamp=record.get("timestamp", ""),
            )
        )

    @property
    def has_samples(self) -> bool:
        return bool(self.query_samples)

    @property
    def query_numbers(self) -> List[int]:
        return [sample.query_number for sample in self.query_samples]

    @property
    def durations_ms(self) -> List[float]:
        return [sample.duration_ms for sample in self.query_samples]

    @property
    def latest_timestamp(self) -> str:
        return self.query_samples[-1].timestamp if self.query_samples else ""

    @property
    def latest_delay_change(self) -> Optional[DelayChangeEvent]:
        return self.delay_changes[-1] if self.delay_changes else None

    @property
    def visible_delay_changes(self) -> List[DelayChangeEvent]:
        if not self.query_samples:
            return []

        first_visible_query = self.query_samples[0].query_number
        return [
            event
            for event in self.delay_changes
            if event.query_number is not None and event.query_number >= first_visible_query
        ]


class QueryDurationChart:

    def __init__(self, measurements: ReplicationDelayMeasurements):
        self.measurements = measurements
        self.figure: Figure
        self.axes: Axes
        self.line: Line2D

        self.figure, self.axes = plt.subplots(figsize=(10, 5))
        self.line, = self.axes.plot(
            [], [], marker="o", markersize=3, linewidth=1, color="#1f77b4"
        )
        self.configure_axes()

    def configure_axes(self) -> None:
        self.axes.set_xlabel("query number")
        self.axes.set_ylabel("duration (ms)")
        self.axes.set_title("Query duration")
        self.axes.grid(True, alpha=0.3)
        self.axes.set_xlim(left=0)
        self.axes.set_ylim(bottom=0)
        self.axes.xaxis.set_major_locator(MaxNLocator(integer=True))

    def redraw(self) -> Line2D:
        if not self.measurements.has_samples:
            return self.line

        self.line.set_data(
            self.measurements.query_numbers,
            self.measurements.durations_ms,
        )
        self.axes.relim()
        self.axes.autoscale_view()

        _, x_max = self.axes.get_xlim()
        _, y_max = self.axes.get_ylim()
        self.axes.set_xlim(left=0, right=max(x_max, 1))
        self.axes.set_ylim(bottom=0, top=max(y_max, 1))
        self.axes.xaxis.set_major_locator(MaxNLocator(integer=True))

        self.redraw_delay_markers(y_max)
        self.update_title()
        return self.line

    def redraw_delay_markers(self, y_max: float) -> None:
        for marker_line in self.axes.lines[1:]:
            marker_line.remove()
        for annotation in list(self.axes.texts):
            annotation.remove()

        for event in self.measurements.visible_delay_changes:
            self.axes.axvline(
                x=event.query_number,
                color="red",
                linestyle="--",
                linewidth=1,
            )
            self.axes.annotate(
                f"{event.delay_type}={event.delay_ms}ms",
                xy=(event.query_number, y_max),
                xytext=(2, -10),
                textcoords="offset points",
                rotation=90,
                va="top",
                fontsize=7,
                color="red",
            )

    def update_title(self) -> None:
        latest_change = self.measurements.latest_delay_change
        change_suffix = ""

        if latest_change is not None:
            change_suffix = (
                f", last change: {latest_change.delay_type}="
                f"{latest_change.delay_ms}ms @q{latest_change.query_number}"
            )

        self.axes.set_title(
            f"Query duration (last {self.measurements.window_size} queries, "
            f"latest: {self.measurements.latest_timestamp}{change_suffix})"
        )


class LiveQueryDurationMonitor:
    """Связывает stdin, хранилище измерений и живой график."""

    def __init__(self, window_size: int, refresh_interval_ms: int):
        self._reader = JsonLineStreamReader(sys.stdin.fileno())
        self._measurements = ReplicationDelayMeasurements(window_size)
        self._chart = QueryDurationChart(self._measurements)
        self._animation = animation.FuncAnimation(
            self._chart.figure,
            self._refresh,
            interval=refresh_interval_ms,
            blit=False,
            cache_frame_data=False,
        )

    def _refresh(self, _frame_number: int):
        for line in self._reader.read_lines():
            self._measurements.add_json_line(line)

        return (self._chart.redraw(),)

    def show(self) -> None:
        plt.tight_layout()
        plt.show()


def parse_arguments(argv: Optional[List[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Live plot of query durations read as JSONL from stdin."
    )
    parser.add_argument(
        "--window",
        type=int,
        default=DEFAULT_WINDOW,
        help="number of most recent queries to display",
    )
    parser.add_argument(
        "--interval",
        type=int,
        default=DEFAULT_INTERVAL,
        help="chart refresh interval in milliseconds",
    )
    return parser.parse_args(argv)


def main() -> None:
    arguments = parse_arguments()
    monitor = LiveQueryDurationMonitor(
        window_size=arguments.window,
        refresh_interval_ms=arguments.interval,
    )
    monitor.show()


if __name__ == "__main__":
    main()
