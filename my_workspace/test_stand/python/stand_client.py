import requests

from utils.db import DBConnection
from utils.sql_parser import parse_sql_file

import argparse
import datetime
import json
import os
import sys
import time

BASE_URL = "http://localhost:8080"

DEFAULT_LIVE_DELAY_WINDOW = "100"
DEFAULT_LIVE_DELAY_INTERVAL = "300"


def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("sql_file", help="path to the SQL file with queries")

    parser.add_argument("--type-delay", type=str, required=True,
                         choices=["write", "flush", "apply"],
                         help="type of delay to set (write, flush, apply)")
    parser.add_argument("--after-queries", type=int, required=True,
                         help="set the delay once, right after this many queries have run")
    parser.add_argument("--delay-ms", type=int, required=True,
                         help="delay value in milliseconds to set after --after-queries queries")
    parser.add_argument("--window", default=DEFAULT_LIVE_DELAY_WINDOW,
                         help="number of most recent queries to display on the graph")
    parser.add_argument("--interval", default=DEFAULT_LIVE_DELAY_INTERVAL,
                         help="graph refresh interval in milliseconds")
    parser.add_argument("--live-graph-script", default="live_write_delay.py",)
    parser.add_argument("--python-path", type=str, required=True,
                         help="path to Python interpreter")
    return parser.parse_args()


def check_file_exists(path: str) -> bool:
    if not os.path.exists(path):
        print(f"Error: file not found: {path}", file=sys.stderr)
        return False
    return True


def spawn_live_graph(python_path: str, live_graph_script: str, window: str, interval: str) -> int:
    read_fd, write_fd = os.pipe()

    pid = os.fork()

    if pid == 0:
        os.close(write_fd)
        os.dup2(read_fd, 0)
        os.close(read_fd)

        try:
            os.execv(python_path, [
                python_path,
                live_graph_script if live_graph_script else "live_write_delay.py",
                "--window", window,
                "--interval", interval,
            ])
        except OSError as exc:
            print(f"Error: execv live_write_delay.py failed: {exc}", file=sys.stderr)
            os._exit(1)

    os.close(read_fd)
    os.dup2(write_fd, 1)  # stdout родителя -> write_fd (в pipe, к потомку)
    os.close(write_fd)

    return pid


def set_delay(type_delay: str, ms: int, queryNumber: int):
    try:
        response = requests.post(
            f"{BASE_URL}/set_delay",
            json={"type_delay": type_delay, "ms": ms}
        )
    except requests.exceptions.RequestException:
        return None

    try:
        json_response = dict(response.json())
    except (json.JSONDecodeError, ValueError):
        return None

    if json_response.get("status") != "ok":
        return None

    return {
        "record_type": "change_delay",
        "timestamp": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "query_number": queryNumber,
        "type_delay": type_delay,
        "delay_ms": ms
    }


def main():
    args = parse_args()

    if not check_file_exists(args.python_path) \
            or not check_file_exists(args.live_graph_script) \
            or not check_file_exists(args.sql_file):
        sys.exit(1)

    queryList = parse_sql_file(args.sql_file)

    child_pid = spawn_live_graph(
        args.python_path, args.live_graph_script, str(args.window), str(args.interval)
    )

    delayAlreadySet = False

    with DBConnection(synchronous_commit="remote_write") as conn:  # коннект по параметрам, указанные в .env
        curr = conn.cursor()
        queryNumber = 0

        for query in queryList:
            queryNumber += 1
            begin_time = time.perf_counter_ns()

            if query["type"] == "statement":
                curr.execute(query["sql"])
                conn.commit()
            elif query["type"] == "transaction":
                for short_query in query["statements"]:
                    curr.execute(short_query)

            end_time = time.perf_counter_ns()
            duration_ms = (end_time - begin_time) / 1_000_000  # convert to milliseconds

            queryRecord = {
                "timestamp": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                "query_number": queryNumber,
                "query_type": query["type"],
                "duration_ms": duration_ms
            }

            json.dump(queryRecord, sys.stdout, ensure_ascii=False)
            sys.stdout.write("\n")
            sys.stdout.flush()

            shouldSetDelay = (
                not delayAlreadySet
                and queryNumber >= args.after_queries
            )

            if shouldSetDelay:
                requestRecord = set_delay(args.type_delay, args.delay_ms, queryNumber)
                if requestRecord is not None:
                    delayAlreadySet = True
            else:
                requestRecord = None

            if requestRecord is not None:
                json.dump(requestRecord, sys.stdout, ensure_ascii=False)
                sys.stdout.write("\n")
                sys.stdout.flush()

    os.close(1)
    os.waitpid(child_pid, 0)


if __name__ == "__main__":
    main()
