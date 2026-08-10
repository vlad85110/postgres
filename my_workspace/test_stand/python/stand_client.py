from utils.db import DBConnection
from utils.sql_parser import parse_sql_file

import argparse
import datetime
import json
import sys
import time
import signal
import os

DEFAULT_SIGNAL_PID = None

def parse_args():
    parser = argparse.ArgumentParser(description="Executes SQL queries and reports duration as JSONL to stdout.")

    parser.add_argument("sql_file", help="path to the SQL file with queries")
    parser.add_argument("--signal-pid", type=int, default=DEFAULT_SIGNAL_PID,
                         help="PID of the process to signal during the run")
    parser.add_argument("--signal-num", type=int, default=signal.SIGUSR2,
                         help="signal number to send (default: SIGUSR2)")
    parser.add_argument("--fixed-delay-start-query", type=int, default=None,
                         help="query number at which the first SIGUSR2 is sent "
                              "(switches receiver from no-delay to fixed-delay phase)")
    parser.add_argument("--signal-repeat-every", type=int, default=None,
                         help="after the fixed-delay phase has started, resend SIGUSR2 "
                              "every N queries to grow the delay further")

    return parser.parse_args()

def send_signal(pid, signum, queryNumber):
    try:
        os.kill(pid, signum)
    except (ProcessLookupError, PermissionError, OSError):
        return None

    return {
        "record_type": "signal",
        "timestamp": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "query_number": queryNumber,
        "target_pid": pid,
        "signal_num": signum
    }

def main():
    args = parse_args()

    queryList = parse_sql_file(args.sql_file)

    fixedDelayEntered = False

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

            shouldEnterFixedDelay = (
                not fixedDelayEntered
                and args.fixed_delay_start_query is not None
                and queryNumber == args.fixed_delay_start_query
            )

            shouldGrowDelay = (
                fixedDelayEntered
                and args.signal_repeat_every
                and (queryNumber - args.fixed_delay_start_query) % args.signal_repeat_every == 0
            )

            if shouldEnterFixedDelay:
                fixedDelayEntered = True
                signalRecord = send_signal(args.signal_pid, args.signal_num, queryNumber)
            elif shouldGrowDelay:
                signalRecord = send_signal(args.signal_pid, args.signal_num, queryNumber)
            else:
                signalRecord = None

            if signalRecord is not None:
                json.dump(signalRecord, sys.stdout, ensure_ascii=False)
                sys.stdout.write("\n")
                sys.stdout.flush()

if __name__ == "__main__":
    main()
