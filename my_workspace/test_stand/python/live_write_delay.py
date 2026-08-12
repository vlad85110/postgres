import argparse
import errno
import json
import os
import sys
from collections import deque

import matplotlib.animation as animation
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator

DEFAULT_WINDOW = 1000
DEFAULT_INTERVAL = 300
READ_CHUNK_SIZE = 65536

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

	stdin_fd = sys.stdin.fileno()
	os.set_blocking(stdin_fd, False)
	pending_bytes = b""

	durations_ms = deque(maxlen=window)
	queryNumbers = deque(maxlen=window)
	timestamps = deque(maxlen=window)
	delayChanges = []  # list of (query_number, type_delay, delay_ms)

	fig, ax = plt.subplots(figsize=(10, 5))
	line, = ax.plot([], [], marker="o", markersize=3, linewidth=1,
					 color="#1f77b4")

	ax.set_xlabel("query number")
	ax.set_ylabel("duration (ms)")
	ax.grid(True, alpha=0.3)
	ax.set_title("Query duration")
	ax.set_xlim(left=0)
	ax.set_ylim(bottom=0)
	ax.xaxis.set_major_locator(MaxNLocator(integer=True))

	def read_available_lines():
		"""
		Reads all currently available bytes from the non-blocking stdin fd
		using raw os.read(), so a partial read never raises/loses data the
		way BufferedReader/TextIOWrapper.readlines() can on non-blocking fds.
		"""
		nonlocal pending_bytes

		chunks = []
		while True:
			try:
				chunk = os.read(stdin_fd, READ_CHUNK_SIZE)
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

		pending_bytes += b"".join(chunks)

		*complete_lines, pending_bytes = pending_bytes.split(b"\n")
		return [line_bytes.decode("utf-8", errors="replace") for line_bytes in complete_lines]

	def update(frame):
		lines = read_available_lines()

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

			recordType = logRecord.get("record_type", "query")

			if recordType == "change_delay":
				delayChanges.append((
					logRecord.get("query_number"),
					logRecord.get("type_delay", "?"),
					logRecord.get("delay_ms", "?"),
				))
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

		xMin, xMax = ax.get_xlim()
		yMin, yMax = ax.get_ylim()
		ax.set_xlim(left=0, right=max(xMax, 1))
		ax.set_ylim(bottom=0, top=max(yMax, 1))
		ax.xaxis.set_major_locator(MaxNLocator(integer=True))

		for axvLine in ax.lines[1:]:
			axvLine.remove()

		for artist in list(ax.texts):
			artist.remove()

		minQuery = min(queryNumbers, default=0)

		for queryNumber, typeDelay, delayMs in delayChanges:
			if queryNumber is None or queryNumber < minQuery:
				continue

			ax.axvline(x=queryNumber, color="red", linestyle="--", linewidth=1)
			ax.annotate(
				f"{typeDelay}={delayMs}ms",
				xy=(queryNumber, yMax),
				xytext=(2, -10),
				textcoords="offset points",
				rotation=90,
				va="top",
				fontsize=7,
				color="red",
			)

		latestChange = delayChanges[-1] if delayChanges else None
		changeSuffix = (
			f", last change: {latestChange[1]}={latestChange[2]}ms @q{latestChange[0]}"
			if latestChange is not None
			else ""
		)

		ax.set_title(
			f"Query duration (last {window} queries, latest: {timestamps[-1]}{changeSuffix})"
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
