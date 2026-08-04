from utils.db import DBConnection
from utils.sql_parser import parse_sql_file

import datetime
import json
import sys
import time

def main():
    queryList = parse_sql_file(sys.argv[1])

    with open(sys.argv[2], "w", encoding="utf-8") as logfile:
        with DBConnection(synchronous_commit="remote_write") as conn: # коннект по параметрам, указанные в .env
            curr = conn.cursor()
            queryNumber = 0

            for query in queryList:
                queryNumber += 1
                begin_time = time.perf_counter_ns()

                if query["type"] == "statement":
                    curr.execute(query["sql"])
                elif query["type"] == "transaction":
                    for short_query in query["statements"]:
                        curr.execute(short_query)

                end_time = time.perf_counter_ns()
                duration_ms = (end_time - begin_time) / 1_000_000  # convert to milliseconds

                logRecord = {
                    "timestamp": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                    "query_number": queryNumber,
                    "query_type": query["type"],
                    "duration_ms": duration_ms
                }

                json.dump(logRecord, logfile, ensure_ascii=False)
                logfile.write("\n")
                logfile.flush()

if __name__ == "__main__":
    main()
