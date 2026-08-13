import argparse
import errno
import json
import os
import sys
from collections import deque
from dataclasses import dataclass
from typing import List, Optional, Union

import matplotlib.animation as animation
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator

DEFAULT_WINDOW = 100
DEFAULT_INTERVAL = 300
READ_CHUNK_SIZE = 65536


def parse_args(argv: Optional[List[str]]):
		parser = argparse.ArgumentParser(
			description="Live plot of query durations read as JSONL from stdin."
		)
		parser.add_argument("--window", type=int, default=DEFAULT_WINDOW,
							 help="number of most recent queries to display")
		parser.add_argument("--interval", type=int, default=DEFAULT_INTERVAL,
							 help="chart refresh interval in milliseconds")
		return parser.parse_args(argv)


@dataclass
class DelayChangeEvent:
	query_number: int
	type_delay: str
	delay_ms: Union[int, float, str]

# TODO: убрать Singleton и нормально по-человечески разбить на классы
class LiveUpdatedGraph:

	def __init__(self, argv: Optional[List[str]] = None):
		self.args = parse_args(argv)
		self.window = self.args.window
		self.interval_ms = self.args.interval

		# --- состояние чтения stdin ---
		self.stdin_fd = sys.stdin.fileno()
		os.set_blocking(self.stdin_fd, False)
		self.pending_bytes = b""

		# --- накопленные данные для графика ---
		self.durations_ms = deque(maxlen=self.window)
		self.query_numbers = deque(maxlen=self.window)
		self.timestamps = deque(maxlen=self.window)
		self.delay_changes: List[DelayChangeEvent] = []

		# --- matplotlib ---
		self.fig, self.ax = plt.subplots(figsize=(10, 5))
		self.line, = self.ax.plot([], [], marker="o", markersize=3, linewidth=1, color="#1f77b4")
		self.configure_axes()

		self.animation = animation.FuncAnimation(
			self.fig, self.update, interval=self.interval_ms, blit=False,
			cache_frame_data=False
		)

	def configure_axes(self) -> None:
		self.ax.set_xlabel("query number")
		self.ax.set_ylabel("duration (ms)")
		self.ax.grid(True, alpha=0.3)
		self.ax.set_title("Query duration")
		self.ax.set_xlim(left=0)
		self.ax.set_ylim(bottom=0)
		self.ax.xaxis.set_major_locator(MaxNLocator(integer=True))

	def read_available_lines(self) -> List[str]:

		chunks = []
		while True:
			try:
				chunk = os.read(self.stdin_fd, READ_CHUNK_SIZE)
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

		self.pending_bytes += b"".join(chunks)

		*complete_lines, self.pending_bytes = self.pending_bytes.split(b"\n")
		return [line_bytes.decode("utf-8", errors="replace") for line_bytes in complete_lines]

	def ingest_line(self, raw_line: str) -> None:
		raw_line = raw_line.strip()
		if not raw_line:
			return

		try:
			log_record = json.loads(raw_line)
		except json.JSONDecodeError:
			return

		record_type = log_record.get("record_type", "query")

		if record_type == "change_delay":
			self.delay_changes.append(DelayChangeEvent(
				query_number=log_record.get("query_number"),
				type_delay=log_record.get("type_delay", "?"),
				delay_ms=log_record.get("delay_ms", "?"),
			))
			return

		if "duration_ms" not in log_record:
			return

		self.durations_ms.append(log_record["duration_ms"])
		self.query_numbers.append(log_record.get("query_number", len(self.query_numbers) + 1))
		self.timestamps.append(log_record.get("timestamp", ""))

	def visible_changes(self) -> List[DelayChangeEvent]:
		min_query = min(self.query_numbers, default=0)
		return [
			event for event in self.delay_changes
			if event.query_number is not None and event.query_number >= min_query
		]

	def redraw_delay_markers(self, y_max: float) -> None:
		for axv_line in self.ax.lines[1:]:
			axv_line.remove()
		for artist in list(self.ax.texts):
			artist.remove()

		for event in self.visible_changes():
			self.ax.axvline(x=event.query_number, color="red", linestyle="--", linewidth=1)
			self.ax.annotate(
				f"{event.type_delay}={event.delay_ms}ms",
				xy=(event.query_number, y_max),
				xytext=(2, -10),
				textcoords="offset points",
				rotation=90,
				va="top",
				fontsize=7,
				color="red",
			)

	def update_title(self) -> None:
		latest_change = self.delay_changes[-1] if self.delay_changes else None
		change_suffix = (
			f", last change: {latest_change.type_delay}={latest_change.delay_ms}ms @q{latest_change.query_number}"
			if latest_change is not None
			else ""
		)
		self.ax.set_title(
			f"Query duration (last {self.window} queries, "
			f"latest: {self.timestamps[-1]}{change_suffix})"
		)

	def update(self, frame):
		lines = self.read_available_lines()

		for raw_line in lines:
			self.ingest_line(raw_line)

		if not self.durations_ms:
			return self.line,

		self.line.set_data(list(self.query_numbers), list(self.durations_ms))
		self.ax.relim()
		self.ax.autoscale_view()

		_, x_max = self.ax.get_xlim()
		_, y_max = self.ax.get_ylim()
		self.ax.set_xlim(left=0)
		self.ax.set_ylim(bottom=0)
		self.ax.xaxis.set_major_locator(MaxNLocator(integer=True))

		self.redraw_delay_markers(y_max)
		self.update_title()

		return self.line,

	def run(self) -> None:
		plt.tight_layout()
		plt.show()


if __name__ == "__main__":
	LiveUpdatedGraph().run()
